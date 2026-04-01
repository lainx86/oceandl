#include "oceandl/http_client.hpp"

#include <curl/curl.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>

#include <fmt/format.h>

#include "oceandl/utils.hpp"

namespace oceandl {

namespace {

constexpr double kDefaultConnectTimeoutSeconds = 15.0;
constexpr long kLowSpeedLimitBytesPerSecond = 64;
constexpr long kMaxLowSpeedObservationSeconds = 30;
constexpr int kMaxRetryAfterSeconds = 300;

struct CurlGlobalInit {
    CurlGlobalInit() {
        curl_global_init(CURL_GLOBAL_DEFAULT);
    }

    ~CurlGlobalInit() {
        curl_global_cleanup();
    }
};

CurlGlobalInit kCurlGlobalInit;

struct HeaderState {
    int status_code = 0;
    HeaderMap headers;
    bool response_started = false;
    std::size_t bytes_transferred = 0;
    std::string callback_error;
    ResponseHandler* handler = nullptr;
};

size_t discard_callback(char* data, size_t size, size_t nmemb, void* userdata) {
    (void)data;
    (void)userdata;
    return size * nmemb;
}

size_t header_callback(char* data, size_t size, size_t nmemb, void* userdata) {
    auto* state = static_cast<HeaderState*>(userdata);
    const auto line = trim(std::string_view(data, size * nmemb));
    if (line.empty()) {
        return size * nmemb;
    }

    if (line.rfind("HTTP/", 0) == 0) {
        state->headers.clear();

        const auto first_space = line.find(' ');
        if (first_space != std::string::npos) {
            const auto second_space = line.find(' ', first_space + 1);
            const auto status_slice = line.substr(
                first_space + 1,
                second_space == std::string::npos ? std::string::npos : second_space - first_space - 1
            );
            if (const auto parsed = parse_optional_uint(status_slice)) {
                state->status_code = static_cast<int>(*parsed);
            }
        }
        return size * nmemb;
    }

    const auto colon = line.find(':');
    if (colon == std::string::npos) {
        return size * nmemb;
    }

    state->headers[to_lower(trim(line.substr(0, colon)))] = trim(line.substr(colon + 1));
    return size * nmemb;
}

size_t write_callback(char* data, size_t size, size_t nmemb, void* userdata) {
    auto* state = static_cast<HeaderState*>(userdata);
    const auto byte_count = size * nmemb;
    if (byte_count == 0) {
        return 0;
    }

    try {
        if (!state->response_started && state->handler != nullptr) {
            state->handler->on_response_start(
                {.status_code = state->status_code, .headers = state->headers, .bytes_transferred = 0}
            );
            state->response_started = true;
        }
        if (state->handler != nullptr && !state->handler->on_chunk(std::string_view(data, byte_count))) {
            state->callback_error = "write callback rejected chunk.";
            return 0;
        }
    } catch (const std::exception& error) {
        state->callback_error = error.what();
        return 0;
    }

    state->bytes_transferred += byte_count;
    return byte_count;
}

void set_common_options(CURL* curl, const HttpRequest& request, const std::string& user_agent) {
    if (!std::isfinite(request.timeout_seconds) || request.timeout_seconds <= 0.0) {
        throw std::invalid_argument("timeout_seconds must be finite and greater than 0.");
    }

    const auto to_timeout_ms = [](double seconds) {
        const auto milliseconds = seconds * 1000.0;
        return static_cast<long>(
            std::min(milliseconds, static_cast<double>(std::numeric_limits<long>::max()))
        );
    };

    curl_easy_setopt(curl, CURLOPT_URL, request.url.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
#if LIBCURL_VERSION_NUM >= 0x075500
    curl_easy_setopt(curl, CURLOPT_PROTOCOLS_STR, "http,https");
    curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS_STR, "http,https");
#else
    curl_easy_setopt(curl, CURLOPT_PROTOCOLS, CURLPROTO_HTTP | CURLPROTO_HTTPS);
    curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS, CURLPROTO_HTTP | CURLPROTO_HTTPS);
#endif
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, to_timeout_ms(request.timeout_seconds));
    curl_easy_setopt(
        curl,
        CURLOPT_CONNECTTIMEOUT_MS,
        to_timeout_ms(std::min(request.timeout_seconds, kDefaultConnectTimeoutSeconds))
    );
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, kLowSpeedLimitBytesPerSecond);
    curl_easy_setopt(
        curl,
        CURLOPT_LOW_SPEED_TIME,
        std::max<long>(
            1,
            std::min(
                static_cast<long>(std::ceil(request.timeout_seconds)),
                kMaxLowSpeedObservationSeconds
            )
        )
    );
    if (request.buffer_size_bytes != 0) {
        const auto requested_buffer_size = static_cast<long>(
            std::min<std::uint64_t>(
                request.buffer_size_bytes,
                static_cast<std::uint64_t>(std::numeric_limits<long>::max())
            )
        );
        curl_easy_setopt(curl, CURLOPT_BUFFERSIZE, requested_buffer_size);
    }
    curl_easy_setopt(curl, CURLOPT_USERAGENT, user_agent.c_str());
}

HttpResponse perform_request(
    const HttpRequest& request,
    const std::string& user_agent,
    bool head_request,
    ResponseHandler* handler
) {
    std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> curl(curl_easy_init(), &curl_easy_cleanup);
    if (!curl) {
        throw NetworkError("failed to initialize libcurl.", CURLE_FAILED_INIT);
    }

    HeaderState state;
    state.handler = handler;

    set_common_options(curl.get(), request, user_agent);
    curl_easy_setopt(curl.get(), CURLOPT_HEADERFUNCTION, header_callback);
    curl_easy_setopt(curl.get(), CURLOPT_HEADERDATA, &state);

    std::unique_ptr<curl_slist, decltype(&curl_slist_free_all)> headers(nullptr, &curl_slist_free_all);
    for (const auto& header : request.headers) {
        headers.reset(curl_slist_append(headers.release(), header.c_str()));
    }
    if (headers) {
        curl_easy_setopt(curl.get(), CURLOPT_HTTPHEADER, headers.get());
    }

    if (head_request) {
        curl_easy_setopt(curl.get(), CURLOPT_NOBODY, 1L);
        curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, discard_callback);
    } else {
        curl_easy_setopt(curl.get(), CURLOPT_HTTPGET, 1L);
        curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, &state);
    }

    const auto code = curl_easy_perform(curl.get());

    if (!head_request && !state.response_started && handler != nullptr) {
        handler->on_response_start(
            {.status_code = state.status_code, .headers = state.headers, .bytes_transferred = 0}
        );
        state.response_started = true;
    }

    if (!state.callback_error.empty()) {
        throw std::runtime_error(state.callback_error);
    }

    if (code != CURLE_OK) {
        throw NetworkError(
            fmt::format("{} for {}", curl_easy_strerror(code), request.url),
            static_cast<int>(code)
        );
    }

    long response_code = 0;
    curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &response_code);

    return {
        .status_code = static_cast<int>(response_code),
        .headers = state.headers,
        .bytes_transferred = state.bytes_transferred,
    };
}

}  // namespace

NetworkError::NetworkError(std::string message, int code)
    : std::runtime_error(std::move(message)), code_(code) {}

int NetworkError::code() const {
    return code_;
}

std::optional<int> parse_retry_after_seconds(const HeaderMap& headers) {
    if (const auto iterator = headers.find("retry-after"); iterator != headers.end()) {
        if (const auto value = parse_optional_uint(iterator->second)) {
            return static_cast<int>(std::min<std::uint64_t>(*value, kMaxRetryAfterSeconds));
        }
    }
    return std::nullopt;
}

HttpStatusError::HttpStatusError(
    int status_code,
    std::string url,
    std::optional<int> retry_after_seconds
)
    : std::runtime_error(fmt::format("HTTP {} for {}", status_code, url)),
      status_code_(status_code),
      url_(std::move(url)),
      retry_after_seconds_(retry_after_seconds) {}

int HttpStatusError::status_code() const {
    return status_code_;
}

const std::string& HttpStatusError::url() const {
    return url_;
}

std::optional<int> HttpStatusError::retry_after_seconds() const {
    return retry_after_seconds_;
}

CurlHttpClient::CurlHttpClient(std::string user_agent) : user_agent_(std::move(user_agent)) {}

HttpResponse CurlHttpClient::head(const HttpRequest& request) {
    return perform_request(request, user_agent_, true, nullptr);
}

HttpResponse CurlHttpClient::get(const HttpRequest& request, ResponseHandler& handler) {
    return perform_request(request, user_agent_, false, &handler);
}

}  // namespace oceandl
