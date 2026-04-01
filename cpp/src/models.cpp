#include "oceandl/models.hpp"

#include <algorithm>
#include <cmath>
#include <chrono>
#include <stdexcept>

#include "oceandl/utils.hpp"

namespace oceandl {

namespace {

constexpr int kMaxRequestedFiles = 500;
constexpr double kMaxTimeoutSeconds = 86400.0;

int current_calendar_year() {
    using namespace std::chrono;
    return static_cast<int>(year_month_day(floor<days>(system_clock::now())).year());
}

std::string render_year_pattern(const std::string& pattern, int year) {
    const auto token = std::string("{year}");
    const auto position = pattern.find(token);
    if (position == std::string::npos) {
        return pattern;
    }

    std::string rendered = pattern;
    rendered.replace(position, token.size(), std::to_string(year));
    return rendered;
}

}  // namespace

void DatasetInfo::normalize_and_validate() {
    id = to_lower(trim(id));
    provider_key = to_lower(trim(provider_key));
    display_name = trim(display_name);
    description = trim(description);
    base_url = trim_trailing_slash(base_url);
    filename_pattern = trim(filename_pattern);

    if (id.empty()) {
        throw std::invalid_argument("dataset id must not be empty.");
    }
    if (provider_key.empty()) {
        throw std::invalid_argument("provider_key must not be empty.");
    }
    if (base_url.empty()) {
        throw std::invalid_argument("base_url must not be empty.");
    }
    if (!is_http_url(base_url)) {
        throw std::invalid_argument("base_url must be a valid http/https URL.");
    }
    if (filename_pattern.empty()) {
        throw std::invalid_argument("filename_pattern must not be empty.");
    }

    const bool has_year_token = filename_pattern.find("{year}") != std::string::npos;
    if (file_mode == FileMode::PerYear && !has_year_token) {
        throw std::invalid_argument("Per-year datasets must use the '{year}' placeholder.");
    }
    if (file_mode == FileMode::SingleFile && has_year_token) {
        throw std::invalid_argument("Single-file datasets must not use the '{year}' placeholder.");
    }
}

bool DatasetInfo::requires_years() const {
    return file_mode == FileMode::PerYear;
}

std::string DatasetInfo::file_name(std::optional<int> year) const {
    if (requires_years()) {
        if (!year.has_value()) {
            throw std::invalid_argument("This dataset requires a year value.");
        }
        validate_year(*year);
        return render_year_pattern(filename_pattern, *year);
    }
    return filename_pattern;
}

std::string DatasetInfo::file_name_for_year(int year) const {
    return file_name(year);
}

std::string DatasetInfo::example_file_name() const {
    const int example_year = start_year.value_or(1981);
    return file_name(requires_years() ? std::optional<int>(example_year) : std::nullopt);
}

std::string DatasetInfo::year_range_label() const {
    if (!requires_years()) {
        return "single file";
    }
    if (!start_year.has_value() && !end_year.has_value()) {
        return "not specified";
    }
    if (start_year.has_value() && end_year.has_value()) {
        return std::to_string(*start_year) + " - " + std::to_string(*end_year);
    }
    if (start_year.has_value()) {
        return std::to_string(*start_year) + "+";
    }
    return "<= " + std::to_string(*end_year);
}

std::string DatasetInfo::requested_year_label(
    std::optional<int> requested_start_year,
    std::optional<int> requested_end_year
) const {
    if (!requires_years()) {
        return "n/a (single file)";
    }
    if (!requested_start_year.has_value() || !requested_end_year.has_value()) {
        return "not specified";
    }
    return std::to_string(*requested_start_year) + " - " + std::to_string(*requested_end_year);
}

int DatasetInfo::requested_file_count(
    std::optional<int> requested_start_year,
    std::optional<int> requested_end_year
) const {
    if (!requires_years()) {
        return 1;
    }
    if (!requested_start_year.has_value() || !requested_end_year.has_value()) {
        throw std::invalid_argument(
            "Dataset '" + id + "' requires both --start-year and --end-year."
        );
    }
    return *requested_end_year - *requested_start_year + 1;
}

void DatasetInfo::validate_year(int year) const {
    if (!requires_years()) {
        return;
    }
    if (start_year.has_value() && year < *start_year) {
        throw std::invalid_argument(
            "Dataset '" + id + "' only supports years starting at "
            + std::to_string(*start_year) + "."
        );
    }
    const auto max_supported_year = end_year.value_or(current_calendar_year());
    if (year > max_supported_year) {
        throw std::invalid_argument(
            "Dataset '" + id + "' only supports years through "
            + std::to_string(max_supported_year) + "."
        );
    }
}

void DatasetInfo::validate_year_span(int requested_start_year, int requested_end_year) const {
    if (!requires_years()) {
        return;
    }
    validate_year(requested_start_year);
    validate_year(requested_end_year);
}

void DatasetInfo::validate_requested_years(
    std::optional<int> requested_start_year,
    std::optional<int> requested_end_year
) const {
    if (!requires_years()) {
        if (requested_start_year.has_value() || requested_end_year.has_value()) {
            throw std::invalid_argument(
                "Dataset '" + id + "' does not accept --start-year or --end-year."
            );
        }
        return;
    }
    if (!requested_start_year.has_value() || !requested_end_year.has_value()) {
        throw std::invalid_argument(
            "Dataset '" + id + "' requires both --start-year and --end-year."
        );
    }
    validate_year_span(*requested_start_year, *requested_end_year);
    const auto file_count = requested_file_count(requested_start_year, requested_end_year);
    if (file_count > kMaxRequestedFiles) {
        throw std::invalid_argument(
            "Dataset '" + id + "' limits requests to at most "
            + std::to_string(kMaxRequestedFiles) + " file(s)."
        );
    }
}

void DownloadRequest::normalize_and_validate() {
    dataset = to_lower(trim(dataset));
    output_dir = expand_user(output_dir);

    if (dataset.empty()) {
        throw std::invalid_argument("dataset must not be empty.");
    }
    if (!std::isfinite(timeout) || timeout <= 0 || timeout > kMaxTimeoutSeconds) {
        throw std::invalid_argument(
            "timeout must be finite, greater than 0, and not excessively large."
        );
    }
    if (chunk_size < 1024) {
        throw std::invalid_argument("chunk_size must be at least 1024 bytes.");
    }
    if (retry_count < 0 || retry_count > kMaxRetryCount) {
        throw std::invalid_argument(
            "retry_count must be between 0 and " + std::to_string(kMaxRetryCount) + "."
        );
    }
    if (start_year.has_value() != end_year.has_value()) {
        throw std::invalid_argument("start_year and end_year must be provided together.");
    }
    if (start_year.has_value() && end_year.has_value() && *start_year > *end_year) {
        throw std::invalid_argument("start_year must be less than or equal to end_year.");
    }
}

std::vector<int> DownloadRequest::years() const {
    if (!start_year.has_value() || !end_year.has_value()) {
        throw std::invalid_argument("The requested year range is not set.");
    }

    std::vector<int> result;
    for (int year = *start_year; year <= *end_year; ++year) {
        result.push_back(year);
    }
    return result;
}

RemoteDownloadTarget DownloadTarget::remote_target() const {
    return {
        .dataset_id = dataset_id,
        .provider_key = provider_key,
        .year = year,
        .file_name = file_name,
        .url = url,
    };
}

DownloadTarget make_download_target(
    const std::filesystem::path& output_root,
    const RemoteDownloadTarget& remote_target
) {
    return {
        .dataset_id = remote_target.dataset_id,
        .provider_key = remote_target.provider_key,
        .year = remote_target.year,
        .file_name = remote_target.file_name,
        .url = remote_target.url,
        .output_path = output_root / remote_target.dataset_id / remote_target.file_name,
    };
}

std::vector<DownloadTarget> make_download_targets(
    const std::filesystem::path& output_root,
    const std::vector<RemoteDownloadTarget>& remote_targets
) {
    std::vector<DownloadTarget> targets;
    targets.reserve(remote_targets.size());
    for (const auto& remote_target : remote_targets) {
        targets.push_back(make_download_target(output_root, remote_target));
    }
    return targets;
}

std::filesystem::path DownloadTarget::temp_path() const {
    return output_path.parent_path() / (output_path.filename().string() + ".part");
}

std::filesystem::path DownloadTarget::temp_metadata_path() const {
    return output_path.parent_path() / (output_path.filename().string() + ".part.meta");
}

std::filesystem::path DownloadTarget::lock_path() const {
    return output_path.parent_path() / (output_path.filename().string() + ".lock");
}

std::size_t DownloadSummary::total_requested() const {
    return results.size();
}

std::size_t DownloadSummary::downloaded_count() const {
    return static_cast<std::size_t>(std::count_if(results.begin(), results.end(), [](const auto& item) {
        return item.status == DownloadStatus::Downloaded;
    }));
}

std::size_t DownloadSummary::skipped_count() const {
    return static_cast<std::size_t>(std::count_if(results.begin(), results.end(), [](const auto& item) {
        return item.status == DownloadStatus::Skipped;
    }));
}

std::size_t DownloadSummary::failed_count() const {
    return static_cast<std::size_t>(std::count_if(results.begin(), results.end(), [](const auto& item) {
        return item.status == DownloadStatus::Failed;
    }));
}

std::vector<DownloadResult> DownloadSummary::failures() const {
    std::vector<DownloadResult> result;
    for (const auto& item : results) {
        if (item.status == DownloadStatus::Failed) {
            result.push_back(item);
        }
    }
    return result;
}

std::string download_status_name(DownloadStatus status) {
    switch (status) {
        case DownloadStatus::Downloaded:
            return "downloaded";
        case DownloadStatus::Skipped:
            return "skipped";
        case DownloadStatus::Failed:
            return "failed";
    }
    return "failed";
}

}  // namespace oceandl
