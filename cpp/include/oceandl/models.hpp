#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace oceandl {

enum class FileMode {
    PerYear,
    SingleFile,
};

enum class DatasetPayloadFormat {
    Netcdf,
};

struct ProviderInfo {
    std::string id;
    std::string name;
    std::string description;
    std::string transport = "http";
};

struct DatasetInfo {
    std::string id;
    std::string display_name;
    std::string description;
    std::string provider_key;
    std::string base_url;
    std::string filename_pattern;
    FileMode file_mode = FileMode::PerYear;
    DatasetPayloadFormat payload_format = DatasetPayloadFormat::Netcdf;
    std::optional<int> start_year;
    std::optional<int> end_year;

    void normalize_and_validate();
    bool requires_years() const;
    std::string file_name(std::optional<int> year = std::nullopt) const;
    std::string file_name_for_year(int year) const;
    std::string example_file_name() const;
    std::string year_range_label() const;
    std::string requested_year_label(
        std::optional<int> requested_start_year,
        std::optional<int> requested_end_year
    ) const;
    int requested_file_count(
        std::optional<int> requested_start_year,
        std::optional<int> requested_end_year
    ) const;
    void validate_year(int year) const;
    void validate_year_span(int requested_start_year, int requested_end_year) const;
    void validate_requested_years(
        std::optional<int> requested_start_year,
        std::optional<int> requested_end_year
    ) const;
};

struct DownloadRequest {
    std::string dataset = "oisst";
    std::optional<int> start_year;
    std::optional<int> end_year;
    std::filesystem::path output_dir;
    bool overwrite = false;
    bool resume = true;
    double timeout = 60.0;
    std::uint64_t chunk_size = 1024 * 1024;
    int retry_count = 3;

    void normalize_and_validate();
    std::vector<int> years() const;
};

struct RemoteDownloadTarget {
    std::string dataset_id;
    std::string provider_key;
    std::optional<int> year;
    std::string file_name;
    std::string url;
};

struct DownloadTarget {
    std::string dataset_id;
    std::string provider_key;
    std::optional<int> year;
    std::string file_name;
    std::string url;
    std::filesystem::path output_path;

    RemoteDownloadTarget remote_target() const;
    std::filesystem::path temp_path() const;
    std::filesystem::path temp_metadata_path() const;
    std::filesystem::path lock_path() const;
};

struct RemoteFileMetadata {
    std::optional<std::uint64_t> content_length;
    std::optional<bool> accepts_ranges;
    std::optional<std::string> etag;
    std::optional<std::string> last_modified;
};

enum class DownloadStatus {
    Downloaded,
    Skipped,
    Failed,
};

struct DownloadResult {
    DownloadTarget target;
    DownloadStatus status = DownloadStatus::Failed;
    std::uint64_t bytes_downloaded = 0;
    int attempts = 1;
    bool resumed = false;
    std::string error_message;
};

struct DownloadSummary {
    DownloadRequest request;
    ProviderInfo provider;
    DatasetInfo dataset;
    std::vector<DownloadResult> results;

    std::size_t total_requested() const;
    std::size_t downloaded_count() const;
    std::size_t skipped_count() const;
    std::size_t failed_count() const;
    std::vector<DownloadResult> failures() const;
};

std::string download_status_name(DownloadStatus status);
DownloadTarget make_download_target(
    const std::filesystem::path& output_root,
    const RemoteDownloadTarget& remote_target
);
std::vector<DownloadTarget> make_download_targets(
    const std::filesystem::path& output_root,
    const std::vector<RemoteDownloadTarget>& remote_targets
);

}  // namespace oceandl
