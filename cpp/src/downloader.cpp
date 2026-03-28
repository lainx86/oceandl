#include "oceandl/downloader.hpp"

#include <curl/curl.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <thread>
#include <vector>

#include <fmt/format.h>

#include "download_lock.hpp"
#include "download_resume.hpp"
#include "oceandl/utils.hpp"
#include "oceandl/validation.hpp"

namespace oceandl {

namespace {

constexpr int kRetryableStatusCodes[] = {408, 429, 500, 502, 503, 504};

bool is_retryable_status(int status_code) {
    for (int candidate : kRetryableStatusCodes) {
        if (candidate == status_code) {
            return true;
        }
    }
    return false;
}

bool is_retryable_network_code(int code) {
    switch (code) {
        case CURLE_OPERATION_TIMEDOUT:
        case CURLE_COULDNT_RESOLVE_HOST:
        case CURLE_COULDNT_RESOLVE_PROXY:
        case CURLE_COULDNT_CONNECT:
        case CURLE_SEND_ERROR:
        case CURLE_RECV_ERROR:
        case CURLE_GOT_NOTHING:
        case CURLE_PARTIAL_FILE:
        case CURLE_HTTP2:
#ifdef CURLE_HTTP2_STREAM
        case CURLE_HTTP2_STREAM:
#endif
            return true;
        default:
            return false;
    }
}

class FileResponseHandler final : public ResponseHandler {
  public:
    FileResponseHandler(
        const DownloadTarget& target,
        bool attempted_resume,
        std::uint64_t start_byte,
        std::optional<std::uint64_t> remote_content_length,
        const Reporter& reporter
    )
        : target_(target),
          attempted_resume_(attempted_resume),
          initial_start_byte_(start_byte),
          remote_content_length_(remote_content_length),
          reporter_(reporter) {}

    void on_response_start(const HttpResponse& response) override {
        response_ = response;
        if (response.status_code == 416) {
            return;
        }

        const bool append = attempted_resume_ && response.status_code == 206;
        if (attempted_resume_ && response.status_code != 206) {
            reporter_.warning(
                fmt::format(
                    "Resume unsupported {} (server mengabaikan Range, mulai dari awal)",
                    target_.file_name
                )
            );
            safe_remove(target_.temp_path());
            restarted_from_zero_ = true;
        }

        active_start_byte_ = append ? initial_start_byte_ : 0;
        total_size_ = expected_total_size(
            response,
            {.content_length = remote_content_length_},
            active_start_byte_
        );
        resumed_transfer_ = append;

        std::ios::openmode mode = std::ios::binary | std::ios::out;
        mode |= append ? std::ios::app : std::ios::trunc;

        stream_.open(target_.temp_path(), mode);
        if (!stream_) {
            throw std::runtime_error("gagal membuka file sementara untuk ditulis.");
        }

        render_progress(true);
    }

    bool on_chunk(std::string_view chunk) override {
        if (!stream_) {
            return false;
        }
        stream_.write(chunk.data(), static_cast<std::streamsize>(chunk.size()));
        if (!stream_) {
            return false;
        }
        bytes_written_ += static_cast<std::uint64_t>(chunk.size());
        render_progress(false);
        return true;
    }

    const HttpResponse& response() const {
        return response_;
    }

    std::uint64_t bytes_written() const {
        return bytes_written_;
    }

    bool restarted_from_zero() const {
        return restarted_from_zero_;
    }

    void close() {
        render_progress(true);
        reporter_.finish_progress();
        if (stream_.is_open()) {
            stream_.close();
        }
    }

  private:
    void render_progress(bool force) {
        if (response_.status_code == 416) {
            return;
        }

        const auto now = std::chrono::steady_clock::now();
        if (!force && now - last_progress_rendered_at_ < std::chrono::milliseconds(80)) {
            return;
        }

        reporter_.progress(
            target_.file_name,
            active_start_byte_ + bytes_written_,
            total_size_,
            resumed_transfer_
        );
        last_progress_rendered_at_ = now;
    }

    DownloadTarget target_;
    bool attempted_resume_ = false;
    std::uint64_t initial_start_byte_ = 0;
    std::uint64_t active_start_byte_ = 0;
    std::optional<std::uint64_t> remote_content_length_;
    std::optional<std::uint64_t> total_size_;
    const Reporter& reporter_;
    HttpResponse response_;
    std::ofstream stream_;
    std::uint64_t bytes_written_ = 0;
    bool restarted_from_zero_ = false;
    bool resumed_transfer_ = false;
    std::chrono::steady_clock::time_point last_progress_rendered_at_{};
};

}  // namespace

Downloader::Downloader(IHttpClient& http_client, const Reporter& reporter)
    : http_client_(&http_client), reporter_(&reporter) {}

DownloadSummary Downloader::download(
    const DownloadRequest& request,
    const DatasetInfo& dataset,
    const DatasetProvider& provider
) const {
    ensure_directory(request.output_dir);

    std::vector<DownloadResult> results;
    const auto remote_targets = provider.build_targets(dataset, request);
    const auto targets = make_download_targets(request.output_dir, remote_targets);
    results.reserve(targets.size());
    for (const auto& target : targets) {
        results.push_back(download_target(request, dataset, provider, target));
    }

    return {
        .request = request,
        .provider = provider.provider_info(),
        .dataset = dataset,
        .results = std::move(results),
    };
}

DownloadResult Downloader::download_target(
    const DownloadRequest& request,
    const DatasetInfo& dataset,
    const DatasetProvider& provider,
    const DownloadTarget& target
) const {
    ensure_directory(target.output_path.parent_path());
    const int total_attempts = request.retry_count + 1;
    std::unique_ptr<TargetFileLock> target_lock;

    for (int attempt = 1; attempt <= total_attempts; ++attempt) {
        try {
            if (!target_lock) {
                target_lock = std::make_unique<TargetFileLock>(target, *reporter_);
            }

            const auto remote_metadata =
                provider.fetch_remote_metadata(*http_client_, target.remote_target(), request.timeout);

            if (const auto skipped = maybe_skip_existing_output(request, dataset, target, remote_metadata)) {
                return *skipped;
            }

            const auto transfer_plan =
                prepare_transfer_plan(request, dataset, target, remote_metadata);
            if (transfer_plan.promote_existing_part) {
                safe_replace_file(target.temp_path(), target.output_path);
                remove_partial_state(target);
                reporter_->success(
                    fmt::format(
                        "Recovered {} dari file sementara yang valid (resume lengkap)",
                        target.file_name
                    )
                );
                return {
                    .target = target,
                    .status = DownloadStatus::Downloaded,
                    .bytes_downloaded = 0,
                    .attempts = attempt,
                    .resumed = true,
                    .error_message = {},
                };
            }

            if (request.resume && has_remote_identity(remote_metadata)) {
                write_partial_metadata(target.temp_metadata_path(), remote_metadata);
            }

            reporter_->detail(fmt::format("Mulai transfer {} -> {}", target.file_name, target.url));
            const auto [bytes_downloaded, resumed] =
                stream_download(request, dataset, provider, target, remote_metadata, transfer_plan);

            safe_replace_file(target.temp_path(), target.output_path);
            remove_partial_state(target);
            std::vector<std::string> notes{
                format_bytes(bytes_downloaded),
                fmt::format("attempt {}", attempt),
            };
            if (resumed) {
                notes.push_back("resumed");
            }
            reporter_->success(
                fmt::format(
                    "Downloaded {} ({})",
                    target.file_name,
                    join_strings(notes, " | ")
                )
            );
            return {
                .target = target,
                .status = DownloadStatus::Downloaded,
                .bytes_downloaded = bytes_downloaded,
                .attempts = attempt,
                .resumed = resumed,
                .error_message = {},
            };
        } catch (const std::exception& error) {
            if (should_remove_partial(error)) {
                remove_partial_state(target);
            }
            if (is_retryable(error) && attempt < total_attempts) {
                const auto wait_seconds = std::min(1 << (attempt - 1), 8);
                reporter_->warning(
                    fmt::format(
                        "Retry {} dalam {}s (attempt {}/{}): {}",
                        target.file_name,
                        wait_seconds,
                        attempt + 1,
                        total_attempts,
                        format_error(error)
                    )
                );
                std::this_thread::sleep_for(std::chrono::seconds(wait_seconds));
                continue;
            }

            reporter_->error(
                fmt::format(
                    "Failed {} after {} attempt(s): {}",
                    target.file_name,
                    attempt,
                    format_error(error)
                )
            );
            return {
                .target = target,
                .status = DownloadStatus::Failed,
                .bytes_downloaded = 0,
                .attempts = attempt,
                .resumed = false,
                .error_message = format_error(error),
            };
        }
    }

    return {
        .target = target,
        .status = DownloadStatus::Failed,
        .bytes_downloaded = 0,
        .attempts = total_attempts,
        .resumed = false,
        .error_message = "download berhenti tanpa hasil.",
    };
}

std::pair<std::uint64_t, bool> Downloader::stream_download(
    const DownloadRequest& request,
    const DatasetInfo& dataset,
    const DatasetProvider& provider,
    const DownloadTarget& target,
    const RemoteFileMetadata& remote_metadata,
    const TransferPlan& transfer_plan
) const {
    const bool attempted_resume = transfer_plan.resumed;
    const std::uint64_t start_byte = attempted_resume ? transfer_plan.start_byte : 0;

    FileResponseHandler handler(
        target,
        attempted_resume,
        start_byte,
        remote_metadata.content_length,
        *reporter_
    );
    const auto response = http_client_->get(
        {
            .url = target.url,
            .headers = provider.build_download_headers(
                target.remote_target(),
                start_byte,
                transfer_plan.if_range_value
            ),
            .timeout_seconds = request.timeout,
            .buffer_size_bytes = request.chunk_size,
        },
        handler
    );
    handler.close();

    if (attempted_resume && response.status_code == 416) {
        if (!remote_metadata.content_length.has_value()) {
            remove_partial_state(target);
            throw DownloadIntegrityError(
                "server menolak Range request dan ukuran remote tidak tersedia untuk verifikasi aman."
            );
        }
        const auto validation =
            validate_dataset_file(dataset, target.temp_path(), remote_metadata.content_length);
        if (validation.valid) {
            return {0, true};
        }
        remove_partial_state(target);
        throw DownloadIntegrityError(
            "server menolak Range request dan file sementara tidak valid."
        );
    }

    if (response.status_code >= 400) {
        throw HttpStatusError(response.status_code, target.url);
    }

    const bool resumed = attempted_resume && response.status_code == 206;
    const auto expected_size = expected_total_size(response, remote_metadata, start_byte);

    const auto actual_size = std::filesystem::file_size(target.temp_path());
    if (expected_size.has_value() && actual_size != *expected_size) {
        throw DownloadIntegrityError(
            fmt::format(
                "ukuran file tidak sesuai header ({} != {} bytes).",
                actual_size,
                *expected_size
            )
        );
    }

    const auto validation = validate_dataset_file(dataset, target.temp_path(), expected_size);
    if (!validation.valid) {
        throw DownloadPayloadError(
            validation.reason.empty()
                ? "payload selesai diunduh tetapi tidak lolos validasi."
                : validation.reason
        );
    }

    return {handler.bytes_written(), resumed};
}

std::optional<DownloadResult> Downloader::maybe_skip_existing_output(
    const DownloadRequest& request,
    const DatasetInfo& dataset,
    const DownloadTarget& target,
    const RemoteFileMetadata& remote_metadata
) const {
    if (!std::filesystem::exists(target.output_path)) {
        return std::nullopt;
    }

    if (!remote_metadata.content_length.has_value()) {
        reporter_->warning(
            fmt::format(
                "Cannot trust existing file {} tanpa ukuran remote; unduh ulang untuk verifikasi",
                target.file_name
            )
        );
        return std::nullopt;
    }

    const auto validation =
        validate_dataset_file(dataset, target.output_path, remote_metadata.content_length);
    if (validation.valid && !request.overwrite) {
        remove_partial_state(target);
        reporter_->info(fmt::format("Skip {} (file valid sudah ada)", target.file_name));
        return DownloadResult{
            .target = target,
            .status = DownloadStatus::Skipped,
            .bytes_downloaded = 0,
            .attempts = 0,
            .resumed = false,
            .error_message = {},
        };
    }

    if (validation.valid) {
        reporter_->info(
            fmt::format("Overwrite {} (file valid akan diunduh ulang)", target.file_name)
        );
    } else {
        reporter_->warning(
            fmt::format(
                "Invalid existing file {}: {}",
                target.file_name,
                validation.reason.empty() ? "validasi gagal" : validation.reason
            )
        );
    }
    return std::nullopt;
}

TransferPlan Downloader::prepare_transfer_plan(
    const DownloadRequest& request,
    const DatasetInfo& dataset,
    const DownloadTarget& target,
    const RemoteFileMetadata& remote_metadata
) const {
    if (!request.resume || !std::filesystem::exists(target.temp_path())) {
        if (!request.resume) {
            remove_partial_state(target);
        }
        return {};
    }

    std::error_code error;
    const auto current_size = std::filesystem::file_size(target.temp_path(), error);
    if (error || current_size == 0) {
        remove_partial_state(target);
        return {};
    }

    if (remote_metadata.content_length.has_value() && current_size > *remote_metadata.content_length) {
        reporter_->warning(
            fmt::format(
                "Discarding stale partial {} (ukuran file sementara melebihi ukuran remote)",
                target.file_name
            )
        );
        remove_partial_state(target);
        return {};
    }

    if (!has_remote_identity(remote_metadata)) {
        reporter_->warning(
            fmt::format(
                "Resume unavailable {} (metadata remote tidak cukup untuk verifikasi aman)",
                target.file_name
            )
        );
        remove_partial_state(target);
        return {};
    }

    const auto partial_metadata = load_partial_metadata(target.temp_metadata_path());
    if (!partial_metadata.has_value()) {
        reporter_->warning(
            fmt::format(
                "Discarding partial {} (metadata resume tidak ditemukan atau rusak)",
                target.file_name
            )
        );
        remove_partial_state(target);
        return {};
    }

    if (!partial_metadata_matches_remote(*partial_metadata, remote_metadata)) {
        reporter_->warning(
            fmt::format(
                "Discarding partial {} (metadata remote berubah sejak download sebelumnya)",
                target.file_name
            )
        );
        remove_partial_state(target);
        return {};
    }

    if (remote_metadata.content_length.has_value() && current_size == *remote_metadata.content_length) {
        const auto validation =
            validate_dataset_file(dataset, target.temp_path(), remote_metadata.content_length);
        if (validation.valid) {
            return {
                .start_byte = current_size,
                .resumed = true,
                .promote_existing_part = true,
                .if_range_value = choose_if_range_value(remote_metadata),
            };
        }
        remove_partial_state(target);
        return {};
    }

    if (remote_metadata.accepts_ranges.has_value() && !*remote_metadata.accepts_ranges) {
        reporter_->warning(
            fmt::format(
                "Resume unavailable {} (server tidak mendukung Range, restart aman)",
                target.file_name
            )
        );
        remove_partial_state(target);
        return {};
    }

    reporter_->detail(fmt::format("Resume {} dari byte {}", target.file_name, current_size));
    return {
        .start_byte = current_size,
        .resumed = true,
        .promote_existing_part = false,
        .if_range_value = choose_if_range_value(remote_metadata),
    };
}

bool Downloader::is_retryable(const std::exception& error) const {
    if (dynamic_cast<const DownloadIntegrityError*>(&error) != nullptr) {
        return true;
    }
    if (const auto* http_error = dynamic_cast<const HttpStatusError*>(&error)) {
        return is_retryable_status(http_error->status_code());
    }
    if (const auto* network_error = dynamic_cast<const NetworkError*>(&error)) {
        return is_retryable_network_code(network_error->code());
    }
    return false;
}

bool Downloader::should_remove_partial(const std::exception& error) const {
    return dynamic_cast<const DownloadIntegrityError*>(&error) != nullptr
        || dynamic_cast<const DownloadPayloadError*>(&error) != nullptr;
}

std::string Downloader::format_error(const std::exception& error) const {
    return error.what();
}

}  // namespace oceandl
