#include "oceandl/providers/base.hpp"

#include "oceandl/utils.hpp"

namespace oceandl {

namespace {

bool should_continue_without_head_metadata(int status_code) {
    switch (status_code) {
        case 403:
        case 405:
        case 408:
        case 429:
        case 500:
        case 501:
        case 502:
        case 503:
        case 504:
            return true;
        default:
            return false;
    }
}

}  // namespace

bool DatasetProvider::supports_dataset(const DatasetInfo& dataset) const {
    return dataset.provider_key == provider_info().id;
}

void DatasetProvider::validate_dataset(const DatasetInfo& dataset) const {
    if (!supports_dataset(dataset)) {
        throw std::invalid_argument(
            "Dataset '" + dataset.id + "' tidak cocok dengan provider '" + provider_info().id + "'."
        );
    }
}

std::vector<std::string> DatasetProvider::build_download_headers(
    const RemoteDownloadTarget& target,
    std::uint64_t start_byte,
    std::optional<std::string> if_range_value
) const {
    (void)target;
    std::vector<std::string> headers{"Accept-Encoding: identity"};
    if (start_byte > 0) {
        headers.push_back("Range: bytes=" + std::to_string(start_byte) + "-");
        if (if_range_value.has_value()) {
            headers.push_back("If-Range: " + *if_range_value);
        }
    }
    return headers;
}

RemoteFileMetadata DatasetProvider::fetch_remote_metadata(
    IHttpClient& client,
    const RemoteDownloadTarget& target,
    double timeout_seconds
) const {
    const auto response = client.head(
        {.url = target.url, .headers = build_download_headers(target), .timeout_seconds = timeout_seconds}
    );

    if (should_continue_without_head_metadata(response.status_code)) {
        return {};
    }
    if (response.status_code >= 400) {
        throw HttpStatusError(response.status_code, target.url);
    }

    RemoteFileMetadata metadata;
    if (const auto iterator = response.headers.find("content-length"); iterator != response.headers.end()) {
        metadata.content_length = parse_optional_uint(iterator->second);
    }
    if (const auto iterator = response.headers.find("accept-ranges"); iterator != response.headers.end()) {
        metadata.accepts_ranges = to_lower(iterator->second) == "bytes";
    }
    if (const auto iterator = response.headers.find("etag"); iterator != response.headers.end()) {
        metadata.etag = iterator->second;
    }
    if (const auto iterator = response.headers.find("last-modified"); iterator != response.headers.end()) {
        metadata.last_modified = iterator->second;
    }

    return metadata;
}

}  // namespace oceandl
