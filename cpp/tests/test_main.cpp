#include <chrono>
#include <cerrno>
#include <curl/curl.h>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#ifndef _WIN32
#include <unistd.h>
#endif

#include "oceandl/catalog.hpp"
#include "oceandl/cli.hpp"
#include "oceandl/config.hpp"
#include "oceandl/downloader.hpp"
#include "oceandl/http_client.hpp"
#include "oceandl/models.hpp"
#include "oceandl/providers/base.hpp"
#include "oceandl/providers/psl.hpp"
#include "oceandl/providers/registry.hpp"
#include "oceandl/reporter.hpp"
#include "oceandl/utils.hpp"
#include "oceandl/validation.hpp"
#include "oceandl/version.hpp"
#include "../src/download_command.hpp"

using namespace oceandl;

namespace {

class TempDir {
  public:
    TempDir() {
        path_ = std::filesystem::temp_directory_path()
            / ("oceandl-test-" + std::to_string(std::rand()) + "-" + std::to_string(std::rand()));
        std::filesystem::create_directories(path_);
    }

    ~TempDir() {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }

    const std::filesystem::path& path() const {
        return path_;
    }

  private:
    std::filesystem::path path_;
};

class ScopedTargetLock {
  public:
    explicit ScopedTargetLock(const std::filesystem::path& path) : lock_path_(path) {
        std::filesystem::create_directories(lock_path_.parent_path());
        std::error_code error;
        const bool created = std::filesystem::create_directory(lock_path_, error);
        if (!created || error) {
            throw std::runtime_error("failed to acquire test lock directory");
        }

#ifndef _WIN32
        std::ofstream owner(lock_path_ / "owner", std::ios::binary | std::ios::trunc);
        if (!owner) {
            throw std::runtime_error("failed to open test lock metadata");
        }
        owner << "pid " << static_cast<long long>(::getpid()) << "\n";
        if (!owner) {
            throw std::runtime_error("failed to write test lock metadata");
        }
#endif
    }

    ~ScopedTargetLock() {
        std::error_code error;
        std::filesystem::remove_all(lock_path_, error);
    }

    ScopedTargetLock(const ScopedTargetLock&) = delete;
    ScopedTargetLock& operator=(const ScopedTargetLock&) = delete;

  private:
    std::filesystem::path lock_path_;
};

int current_calendar_year() {
    using namespace std::chrono;
    return static_cast<int>(year_month_day(floor<days>(system_clock::now())).year());
}

std::vector<char> valid_netcdf_bytes(std::size_t size = 32) {
    if (size < 8) {
        throw std::invalid_argument("size must be at least 8");
    }
    std::vector<char> bytes(size, '\0');
    bytes[0] = 'C';
    bytes[1] = 'D';
    bytes[2] = 'F';
    bytes[3] = 1;
    return bytes;
}

class FakeHttpClient final : public IHttpClient {
  public:
    std::function<HttpResponse(const HttpRequest&)> on_head;
    std::function<HttpResponse(const HttpRequest&, ResponseHandler&)> on_get;

    HttpResponse head(const HttpRequest& request) override {
        return on_head(request);
    }

    HttpResponse get(const HttpRequest& request, ResponseHandler& handler) override {
        return on_get(request, handler);
    }
};

class FixtureProvider final : public DatasetProvider {
  public:
    explicit FixtureProvider(RemoteFileMetadata metadata = {}) : metadata_(std::move(metadata)) {}

    ProviderInfo provider_info() const override {
        return {
            .id = "fixture",
            .name = "Fixture Provider",
            .description = "Fixture provider for tests.",
            .transport = "http",
        };
    }

    std::vector<RemoteDownloadTarget> build_targets(
        const DatasetInfo& dataset,
        const DownloadRequest& request
    ) const override {
        (void)request;
        return {
            {
                .dataset_id = dataset.id,
                .provider_key = provider_info().id,
                .year = std::nullopt,
                .file_name = "fixture.nc",
                .url = "https://example.test/datasets/fixture.nc",
            }
        };
    }

    RemoteFileMetadata fetch_remote_metadata(
        IHttpClient& client,
        const RemoteDownloadTarget& target,
        double timeout_seconds
    ) const override {
        (void)client;
        (void)target;
        (void)timeout_seconds;
        return metadata_;
    }

  private:
    RemoteFileMetadata metadata_;
};

DatasetInfo fixture_dataset(std::string provider_key = "fixture") {
    DatasetInfo dataset{
        .id = "fixture",
        .display_name = "Fixture Dataset",
        .description = "Fixture dataset for downloader tests.",
        .provider_key = std::move(provider_key),
        .base_url = "https://example.test/datasets",
        .filename_pattern = "fixture.nc",
        .file_mode = FileMode::SingleFile,
        .start_year = std::nullopt,
        .end_year = std::nullopt,
    };
    dataset.normalize_and_validate();
    return dataset;
}

class MetadataFallbackFixtureProvider final : public DatasetProvider {
  public:
    ProviderInfo provider_info() const override {
        return {
            .id = "fixture_head",
            .name = "Fixture Head Fallback Provider",
            .description = "Provider using the default HEAD metadata path.",
            .transport = "http",
        };
    }

    std::vector<RemoteDownloadTarget> build_targets(
        const DatasetInfo& dataset,
        const DownloadRequest& request
    ) const override {
        (void)request;
        return {
            {
                .dataset_id = dataset.id,
                .provider_key = provider_info().id,
                .year = std::nullopt,
                .file_name = "fixture.nc",
                .url = "https://example.test/datasets/fixture.nc",
            }
        };
    }
};

DownloadRequest fixture_request(const std::filesystem::path& output_dir) {
    DownloadRequest request{
        .dataset = "fixture",
        .start_year = std::nullopt,
        .end_year = std::nullopt,
        .output_dir = output_dir,
        .overwrite = false,
        .resume = true,
        .timeout = 60.0,
        .chunk_size = 1024,
        .retry_count = 0,
    };
    request.normalize_and_validate();
    return request;
}

RemoteFileMetadata fixture_remote_metadata(
    std::optional<std::uint64_t> content_length = std::nullopt,
    std::optional<bool> accepts_ranges = std::nullopt,
    std::optional<std::string> etag = std::nullopt,
    std::optional<std::string> last_modified = std::nullopt
) {
    return RemoteFileMetadata{
        .content_length = content_length,
        .accepts_ranges = accepts_ranges,
        .etag = std::move(etag),
        .last_modified = std::move(last_modified),
    };
}

bool expect(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        return false;
    }
    return true;
}

bool expect_contains(
    const std::string& haystack,
    const std::string& needle,
    const std::string& message
) {
    return expect(haystack.find(needle) != std::string::npos, message);
}

void write_partial_metadata_file(
    const std::filesystem::path& path,
    std::optional<std::uint64_t> content_length,
    std::optional<std::string> etag = std::nullopt,
    std::optional<std::string> last_modified = std::nullopt
) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output << "content_length ";
    if (content_length.has_value()) {
        output << *content_length;
    } else {
        output << "-";
    }
    output << '\n';
    output << "etag " << std::quoted(etag.value_or("")) << '\n';
    output << "last_modified " << std::quoted(last_modified.value_or("")) << '\n';
}

bool request_has_header_prefix(const HttpRequest& request, const std::string& prefix) {
    for (const auto& header : request.headers) {
        if (header.rfind(prefix, 0) == 0) {
            return true;
        }
    }
    return false;
}

std::string request_header_value(const HttpRequest& request, const std::string& prefix) {
    for (const auto& header : request.headers) {
        if (header.rfind(prefix, 0) == 0) {
            return header.substr(prefix.size());
        }
    }
    return {};
}

bool path_has_lock_artifact(const std::filesystem::path& path) {
    return std::filesystem::exists(path);
}

bool test_downloader_cleans_up_lock_artifact_after_successful_download() {
    TempDir temp_dir;
    auto dataset = fixture_dataset();
    auto request = fixture_request(temp_dir.path());
    const auto payload = valid_netcdf_bytes();

    int get_calls = 0;
    FakeHttpClient client;
    client.on_head = [&](const HttpRequest&) { return HttpResponse{}; };
    client.on_get = [&](const HttpRequest&, ResponseHandler& handler) {
        ++get_calls;
        handler.on_response_start(
            HttpResponse{
                .status_code = 200,
                .headers = {{"content-length", std::to_string(payload.size())}},
                .bytes_transferred = 0,
            }
        );
        handler.on_chunk(std::string_view(payload.data(), payload.size()));
        return HttpResponse{
            .status_code = 200,
            .headers = {{"content-length", std::to_string(payload.size())}},
            .bytes_transferred = payload.size(),
        };
    };

    std::ostringstream out;
    std::ostringstream err;
    Reporter reporter(out, err, Verbosity::Quiet);
    Downloader downloader(client, reporter);
    FixtureProvider provider(fixture_remote_metadata(payload.size(), true));

    const auto summary = downloader.download(request, dataset, provider);
    const auto final_path = temp_dir.path() / dataset.id / "fixture.nc";
    const auto lock_path = temp_dir.path() / dataset.id / "fixture.nc.lock";
    return expect(summary.downloaded_count() == 1, "summary downloaded count after success")
        && expect(get_calls == 1, "get called once for successful download")
        && expect(std::filesystem::exists(final_path), "output exists after successful download")
        && expect(!path_has_lock_artifact(lock_path), "lock artifact removed after successful download");
}

bool test_registry_contains_expected_datasets() {
    const auto registry = build_default_dataset_registry(default_app_config());
    const auto& oisst = registry.get("oisst");
    const auto& gpcp = registry.get("gpcp");
    const auto& mslp = registry.get("mslp");
    const auto& uwnd_surface = registry.get("uwnd_surface");
    const auto& hgt_pressure = registry.get("hgt_pressure");

    return expect(registry.list().size() == 10, "dataset count includes recommended additions")
        && expect(oisst.file_name_for_year(2024) == "sst.day.mean.2024.nc", "oisst file name")
        && expect(oisst.requires_years(), "oisst requires years")
        && expect(gpcp.file_name() == "precip.mon.mean.nc", "gpcp file name")
        && expect(!gpcp.requires_years(), "gpcp single file")
        && expect(mslp.file_name() == "mslp.mon.mean.nc", "mslp file name")
        && expect(
            uwnd_surface.base_url
                == "https://downloads.psl.noaa.gov/Datasets/ncep.reanalysis.derived/surface",
            "uwnd_surface base url"
        )
        && expect(hgt_pressure.file_name() == "hgt.mon.mean.nc", "hgt_pressure file name");
}

bool test_download_request_validation() {
    TempDir temp_dir;
    try {
        DownloadRequest invalid{
            .dataset = "oisst",
            .start_year = 2025,
            .end_year = 2024,
            .output_dir = temp_dir.path(),
            .overwrite = false,
            .resume = true,
            .timeout = 60.0,
            .chunk_size = 1024,
            .retry_count = 0,
        };
        invalid.normalize_and_validate();
        return expect(false, "reversed year range should fail");
    } catch (const std::exception&) {
        return true;
    }
}

bool test_download_request_rejects_excessive_retry_count() {
    TempDir temp_dir;
    try {
        DownloadRequest invalid{
            .dataset = "oisst",
            .start_year = 2024,
            .end_year = 2024,
            .output_dir = temp_dir.path(),
            .overwrite = false,
            .resume = true,
            .timeout = 60.0,
            .chunk_size = 1024,
            .retry_count = kMaxRetryCount + 1,
        };
        invalid.normalize_and_validate();
        return expect(false, "excessive retry_count should fail");
    } catch (const std::exception& error) {
        return expect_contains(
            error.what(),
            "retry_count must be between 0 and",
            "retry_count upper bound message"
        );
    }
}

bool test_download_command_parses_flags() {
    const auto options = parse_download_options(
        {
            "oisst",
            "--start-year",
            "2024",
            "--end-year",
            "2025",
            "--output-dir",
            "data/out",
            "--overwrite",
            "--no-resume",
            "--timeout",
            "90",
            "--chunk-size",
            "4096",
            "--retries",
            "5",
        }
    );

    return expect(options.dataset_argument == std::optional<std::string>("oisst"), "download parser positional dataset")
        && expect(options.start_year == std::optional<int>(2024), "download parser start year")
        && expect(options.end_year == std::optional<int>(2025), "download parser end year")
        && expect(options.output_dir == std::optional<std::filesystem::path>("data/out"), "download parser output dir")
        && expect(options.overwrite == std::optional<bool>(true), "download parser overwrite")
        && expect(options.resume == std::optional<bool>(false), "download parser resume")
        && expect(options.timeout == std::optional<double>(90.0), "download parser timeout")
        && expect(options.chunk_size == std::optional<std::uint64_t>(4096), "download parser chunk size")
        && expect(options.retries == std::optional<int>(5), "download parser retries");
}

bool test_download_command_resolves_dataset_from_option_or_config() {
    const auto config = default_app_config();

    DownloadCommandOptions from_option_options;
    from_option_options.dataset_option = std::string("GPCP");

    const auto from_option = resolve_dataset_name(from_option_options, config);
    const auto from_config = resolve_dataset_name(DownloadCommandOptions{}, config);

    return expect(from_option == "gpcp", "download command resolves --dataset")
        && expect(from_config == config.default_dataset, "download command falls back to config default");
}

bool test_download_command_rejects_conflicting_dataset_values() {
    try {
        DownloadCommandOptions options;
        options.dataset_argument = std::string("oisst");
        options.dataset_option = std::string("gpcp");

        (void)resolve_dataset_name(
            options,
            default_app_config()
        );
        return expect(false, "conflicting dataset values should fail");
    } catch (const std::exception&) {
        return true;
    }
}

bool test_psl_provider_targets() {
    TempDir temp_dir;
    const auto registry = build_default_dataset_registry(default_app_config());
    const auto& dataset = registry.get("oisst");
    DownloadRequest request{
        .dataset = "oisst",
        .start_year = 2024,
        .end_year = 2025,
        .output_dir = temp_dir.path(),
        .overwrite = false,
        .resume = true,
        .timeout = 60.0,
        .chunk_size = 1024,
        .retry_count = 0,
    };
    request.normalize_and_validate();

    PSLProvider provider;
    const auto remote_targets = provider.build_targets(dataset, request);
    const auto targets = make_download_targets(request.output_dir, remote_targets);
    return expect(targets.size() == 2, "psl target count")
        && expect(targets[0].file_name == "sst.day.mean.2024.nc", "psl target file")
        && expect(
            targets[0].temp_path() == temp_dir.path() / "oisst" / "sst.day.mean.2024.nc.part",
            "psl temp path"
        );
}

bool test_download_target_layout_is_centralized() {
    const RemoteDownloadTarget remote_target{
        .dataset_id = "fixture",
        .provider_key = "fixture",
        .year = 2024,
        .file_name = "fixture.2024.nc",
        .url = "https://example.test/datasets/fixture.2024.nc",
    };

    const auto target = make_download_target("/tmp/oceandl-out", remote_target);
    return expect(target.output_path == std::filesystem::path("/tmp/oceandl-out/fixture/fixture.2024.nc"), "centralized output layout")
        && expect(target.remote_target().url == remote_target.url, "remote target preserved")
        && expect(target.remote_target().file_name == remote_target.file_name, "remote target filename preserved");
}

bool test_validation_accepts_minimal_netcdf() {
    TempDir temp_dir;
    const auto path = temp_dir.path() / "fixture.nc";
    const auto payload = valid_netcdf_bytes();
    std::ofstream output(path, std::ios::binary);
    output.write(payload.data(), static_cast<std::streamsize>(payload.size()));
    output.close();

    const auto validation = validate_netcdf_file(path, payload.size());
    return expect(validation.valid, "netcdf validation");
}

bool test_validation_rejects_invalid_netcdf_version_byte() {
    TempDir temp_dir;
    const auto path = temp_dir.path() / "invalid.nc";
    auto payload = valid_netcdf_bytes();
    payload[3] = 9;

    std::ofstream output(path, std::ios::binary);
    output.write(payload.data(), static_cast<std::streamsize>(payload.size()));
    output.close();

    const auto validation = validate_netcdf_file(path, payload.size());
    return expect(!validation.valid, "invalid netcdf version byte rejected")
        && expect_contains(validation.reason, "file signature does not match", "invalid netcdf version reason");
}

bool test_dataset_validation_uses_dataset_metadata() {
    TempDir temp_dir;
    auto dataset = fixture_dataset();
    const auto path = temp_dir.path() / "fixture.nc";
    const auto payload = valid_netcdf_bytes();
    std::ofstream output(path, std::ios::binary);
    output.write(payload.data(), static_cast<std::streamsize>(payload.size()));
    output.close();

    const auto validation = validate_dataset_file(dataset, path, payload.size());
    return expect(validation.valid, "dataset validation uses dataset payload format");
}

bool test_downloader_skips_existing_valid_file() {
    TempDir temp_dir;
    auto dataset = fixture_dataset();
    auto request = fixture_request(temp_dir.path());

    const auto payload = valid_netcdf_bytes();
    const auto output_path = temp_dir.path() / dataset.id / "fixture.nc";
    std::filesystem::create_directories(output_path.parent_path());
    std::ofstream output(output_path, std::ios::binary);
    output.write(payload.data(), static_cast<std::streamsize>(payload.size()));
    output.close();

    int get_calls = 0;
    FakeHttpClient client;
    client.on_head = [&](const HttpRequest&) {
        return HttpResponse{
            .status_code = 200,
            .headers = {
                {"content-length", std::to_string(payload.size())},
                {"accept-ranges", "bytes"},
            },
            .bytes_transferred = 0,
        };
    };
    client.on_get = [&](const HttpRequest&, ResponseHandler&) {
        ++get_calls;
        return HttpResponse{};
    };

    std::ostringstream out;
    std::ostringstream err;
    Reporter reporter(out, err, Verbosity::Quiet);
    Downloader downloader(client, reporter);
    FixtureProvider provider(fixture_remote_metadata(payload.size(), true));

    const auto summary = downloader.download(request, dataset, provider);
    const auto lock_path = temp_dir.path() / dataset.id / "fixture.nc.lock";
    return expect(summary.skipped_count() == 1, "summary skipped count")
        && expect(get_calls == 0, "skip avoids get")
        && expect(!path_has_lock_artifact(lock_path), "lock artifact removed after skip");
}

bool test_downloader_redownloads_existing_file_when_remote_size_unknown() {
    TempDir temp_dir;
    auto dataset = fixture_dataset();
    auto request = fixture_request(temp_dir.path());

    const auto payload = valid_netcdf_bytes();
    const auto output_path = temp_dir.path() / dataset.id / "fixture.nc";
    std::filesystem::create_directories(output_path.parent_path());
    std::ofstream output(output_path, std::ios::binary);
    output.write(payload.data(), static_cast<std::streamsize>(payload.size()));
    output.close();

    int get_calls = 0;
    FakeHttpClient client;
    client.on_head = [&](const HttpRequest&) {
        return HttpResponse{
            .status_code = 200,
            .headers = {{"etag", "\"fixture-v1\""}},
            .bytes_transferred = 0,
        };
    };
    client.on_get = [&](const HttpRequest&, ResponseHandler& handler) {
        ++get_calls;
        handler.on_response_start(
            HttpResponse{
                .status_code = 200,
                .headers = {{"content-length", std::to_string(payload.size())}},
                .bytes_transferred = 0,
            }
        );
        handler.on_chunk(std::string_view(payload.data(), payload.size()));
        return HttpResponse{
            .status_code = 200,
            .headers = {{"content-length", std::to_string(payload.size())}},
            .bytes_transferred = payload.size(),
        };
    };

    std::ostringstream out;
    std::ostringstream err;
    Reporter reporter(out, err, Verbosity::Quiet);
    Downloader downloader(client, reporter);
    FixtureProvider provider(fixture_remote_metadata(std::nullopt, true, "\"fixture-v1\""));

    const auto summary = downloader.download(request, dataset, provider);
    return expect(summary.downloaded_count() == 1, "downloaded when remote size unknown")
        && expect(get_calls == 1, "existing file redownloaded when remote size unknown");
}

bool test_downloader_rejects_partial_on_416_without_remote_size() {
    TempDir temp_dir;
    auto dataset = fixture_dataset();
    auto request = fixture_request(temp_dir.path());
    const auto payload = valid_netcdf_bytes();

    const auto part_path = temp_dir.path() / dataset.id / "fixture.nc.part";
    const auto meta_path = temp_dir.path() / dataset.id / "fixture.nc.part.meta";
    std::filesystem::create_directories(part_path.parent_path());
    std::ofstream output(part_path, std::ios::binary);
    output.write(payload.data(), static_cast<std::streamsize>(payload.size()));
    output.close();
    write_partial_metadata_file(meta_path, std::nullopt, "\"fixture-v1\"");

    FakeHttpClient client;
    client.on_head = [&](const HttpRequest&) {
        return HttpResponse{
            .status_code = 200,
            .headers = {{"etag", "\"fixture-v1\""}},
            .bytes_transferred = 0,
        };
    };
    client.on_get = [&](const HttpRequest&, ResponseHandler& handler) {
        handler.on_response_start(HttpResponse{.status_code = 416, .headers = {}, .bytes_transferred = 0});
        return HttpResponse{.status_code = 416, .headers = {}, .bytes_transferred = 0};
    };

    std::ostringstream out;
    std::ostringstream err;
    Reporter reporter(out, err, Verbosity::Quiet);
    Downloader downloader(client, reporter);
    FixtureProvider provider(fixture_remote_metadata(std::nullopt, true, "\"fixture-v1\""));

    const auto summary = downloader.download(request, dataset, provider);
    const auto final_path = temp_dir.path() / dataset.id / "fixture.nc";
    return expect(summary.failed_count() == 1, "summary failed count after unsafe 416")
        && expect(!std::filesystem::exists(final_path), "no promoted output after unsafe 416")
        && expect(!std::filesystem::exists(part_path), "part file removed after unsafe 416")
        && expect(!std::filesystem::exists(meta_path), "part metadata removed after unsafe 416");
}

bool test_downloader_discards_partial_when_ranges_unavailable() {
    TempDir temp_dir;
    auto dataset = fixture_dataset();
    auto request = fixture_request(temp_dir.path());
    const auto payload = valid_netcdf_bytes();

    const auto part_path = temp_dir.path() / dataset.id / "fixture.nc.part";
    std::filesystem::create_directories(part_path.parent_path());
    std::ofstream partial(part_path, std::ios::binary);
    partial.write(payload.data(), 16);
    partial.close();

    FakeHttpClient client;
    client.on_head = [&](const HttpRequest&) {
        return HttpResponse{
            .status_code = 200,
            .headers = {},
            .bytes_transferred = 0,
        };
    };
    client.on_get = [&](const HttpRequest&, ResponseHandler& handler) {
        handler.on_response_start(
            HttpResponse{
                .status_code = 200,
                .headers = {{"content-length", std::to_string(payload.size())}},
                .bytes_transferred = 0,
            }
        );
        handler.on_chunk(std::string_view(payload.data(), payload.size()));
        return HttpResponse{
            .status_code = 200,
            .headers = {{"content-length", std::to_string(payload.size())}},
            .bytes_transferred = payload.size(),
        };
    };

    std::ostringstream out;
    std::ostringstream err;
    Reporter reporter(out, err, Verbosity::Quiet);
    Downloader downloader(client, reporter);
    FixtureProvider provider(fixture_remote_metadata(payload.size(), false));

    const auto summary = downloader.download(request, dataset, provider);
    const auto final_path = temp_dir.path() / dataset.id / "fixture.nc";
    return expect(summary.downloaded_count() == 1, "download count after restart")
        && expect(std::filesystem::exists(final_path), "output exists after restart")
        && expect(!std::filesystem::exists(part_path), "part removed after restart");
}

bool test_downloader_discards_partial_when_etag_changes() {
    TempDir temp_dir;
    auto dataset = fixture_dataset();
    auto request = fixture_request(temp_dir.path());
    const auto payload = valid_netcdf_bytes();

    const auto part_path = temp_dir.path() / dataset.id / "fixture.nc.part";
    const auto meta_path = temp_dir.path() / dataset.id / "fixture.nc.part.meta";
    std::filesystem::create_directories(part_path.parent_path());
    std::ofstream partial(part_path, std::ios::binary);
    partial.write(payload.data(), 16);
    partial.close();
    write_partial_metadata_file(meta_path, payload.size(), "\"old-etag\"");

    bool saw_range = false;
    bool saw_if_range = false;
    FakeHttpClient client;
    client.on_head = [&](const HttpRequest&) {
        return HttpResponse{
            .status_code = 200,
            .headers = {
                {"content-length", std::to_string(payload.size())},
                {"accept-ranges", "bytes"},
                {"etag", "\"new-etag\""},
            },
            .bytes_transferred = 0,
        };
    };
    client.on_get = [&](const HttpRequest& request, ResponseHandler& handler) {
        saw_range = request_has_header_prefix(request, "Range: ");
        saw_if_range = request_has_header_prefix(request, "If-Range: ");
        handler.on_response_start(
            HttpResponse{
                .status_code = 200,
                .headers = {{"content-length", std::to_string(payload.size())}},
                .bytes_transferred = 0,
            }
        );
        handler.on_chunk(std::string_view(payload.data(), payload.size()));
        return HttpResponse{
            .status_code = 200,
            .headers = {{"content-length", std::to_string(payload.size())}},
            .bytes_transferred = payload.size(),
        };
    };

    std::ostringstream out;
    std::ostringstream err;
    Reporter reporter(out, err, Verbosity::Quiet);
    Downloader downloader(client, reporter);
    FixtureProvider provider(fixture_remote_metadata(payload.size(), true, "\"new-etag\""));

    const auto summary = downloader.download(request, dataset, provider);
    const auto final_path = temp_dir.path() / dataset.id / "fixture.nc";
    return expect(summary.downloaded_count() == 1, "download count after etag mismatch")
        && expect(std::filesystem::exists(final_path), "output exists after etag mismatch")
        && expect(!std::filesystem::exists(part_path), "part removed after etag mismatch")
        && expect(!std::filesystem::exists(meta_path), "part metadata removed after etag mismatch")
        && expect(!saw_range, "full download after etag mismatch")
        && expect(!saw_if_range, "if-range omitted after etag mismatch");
}

bool test_downloader_sends_if_range_when_resuming() {
    TempDir temp_dir;
    auto dataset = fixture_dataset();
    auto request = fixture_request(temp_dir.path());
    const auto payload = valid_netcdf_bytes();

    const auto part_path = temp_dir.path() / dataset.id / "fixture.nc.part";
    const auto meta_path = temp_dir.path() / dataset.id / "fixture.nc.part.meta";
    std::filesystem::create_directories(part_path.parent_path());
    std::ofstream partial(part_path, std::ios::binary);
    partial.write(payload.data(), 16);
    partial.close();
    write_partial_metadata_file(meta_path, payload.size(), "\"fixture-v1\"");

    HttpRequest last_get_request;
    FakeHttpClient client;
    client.on_head = [&](const HttpRequest&) {
        return HttpResponse{
            .status_code = 200,
            .headers = {
                {"content-length", std::to_string(payload.size())},
                {"accept-ranges", "bytes"},
                {"etag", "\"fixture-v1\""},
            },
            .bytes_transferred = 0,
        };
    };
    client.on_get = [&](const HttpRequest& request, ResponseHandler& handler) {
        last_get_request = request;
        handler.on_response_start(
            HttpResponse{
                .status_code = 206,
                .headers = {
                    {"content-length", std::to_string(payload.size() - 16)},
                    {"content-range", "bytes 16-31/32"},
                },
                .bytes_transferred = 0,
            }
        );
        handler.on_chunk(std::string_view(payload.data() + 16, payload.size() - 16));
        return HttpResponse{
            .status_code = 206,
            .headers = {
                {"content-length", std::to_string(payload.size() - 16)},
                {"content-range", "bytes 16-31/32"},
            },
            .bytes_transferred = payload.size() - 16,
        };
    };

    std::ostringstream out;
    std::ostringstream err;
    Reporter reporter(out, err, Verbosity::Quiet);
    Downloader downloader(client, reporter);
    FixtureProvider provider(fixture_remote_metadata(payload.size(), true, "\"fixture-v1\""));

    const auto summary = downloader.download(request, dataset, provider);
    const auto final_path = temp_dir.path() / dataset.id / "fixture.nc";
    const auto lock_path = temp_dir.path() / dataset.id / "fixture.nc.lock";
    return expect(summary.downloaded_count() == 1, "download count after resume")
        && expect(summary.results.at(0).resumed, "summary marks resumed download")
        && expect(std::filesystem::exists(final_path), "output exists after resume")
        && expect(!std::filesystem::exists(part_path), "part removed after resume")
        && expect(!std::filesystem::exists(meta_path), "part metadata removed after resume")
        && expect(!path_has_lock_artifact(lock_path), "lock artifact removed after resume")
        && expect(request_has_header_prefix(last_get_request, "Range: bytes=16-"), "range header sent")
        && expect(
            request_header_value(last_get_request, "If-Range: ") == "\"fixture-v1\"",
            "if-range header sent"
        );
}

bool test_downloader_fails_when_target_is_locked() {
    TempDir temp_dir;
    auto dataset = fixture_dataset();
    auto request = fixture_request(temp_dir.path());

    const auto lock_path = temp_dir.path() / dataset.id / "fixture.nc.lock";
    ScopedTargetLock lock(lock_path);

    int head_calls = 0;
    int get_calls = 0;
    FakeHttpClient client;
    client.on_head = [&](const HttpRequest&) {
        ++head_calls;
        return HttpResponse{};
    };
    client.on_get = [&](const HttpRequest&, ResponseHandler&) {
        ++get_calls;
        return HttpResponse{};
    };

    std::ostringstream out;
    std::ostringstream err;
    Reporter reporter(out, err, Verbosity::Quiet);
    Downloader downloader(client, reporter);
    FixtureProvider provider(fixture_remote_metadata(32, true));

    const auto summary = downloader.download(request, dataset, provider);
    return expect(summary.failed_count() == 1, "summary failed count when target locked")
        && expect(head_calls == 0, "no head request when target locked")
        && expect(get_calls == 0, "no get request when target locked")
        && expect_contains(summary.failures().at(0).error_message, "target is already being used by another process", "lock error message");
}

bool test_downloader_recovers_legacy_lock_directory_without_peer_process() {
    TempDir temp_dir;
    auto dataset = fixture_dataset();
    auto request = fixture_request(temp_dir.path());

    const auto lock_path = temp_dir.path() / dataset.id / "fixture.nc.lock";
    std::filesystem::create_directories(lock_path.parent_path());
    std::filesystem::create_directory(lock_path);

    const auto payload = valid_netcdf_bytes();
    int get_calls = 0;
    FakeHttpClient client;
    client.on_head = [&](const HttpRequest&) { return HttpResponse{}; };
    client.on_get = [&](const HttpRequest&, ResponseHandler& handler) {
        ++get_calls;
        handler.on_response_start(
            HttpResponse{
                .status_code = 200,
                .headers = {
                    {"content-length", std::to_string(payload.size())},
                },
                .bytes_transferred = 0,
            }
        );
        handler.on_chunk(std::string_view(payload.data(), payload.size()));
        return HttpResponse{
            .status_code = 200,
            .headers = {
                {"content-length", std::to_string(payload.size())},
            },
            .bytes_transferred = payload.size(),
        };
    };

    std::ostringstream out;
    std::ostringstream err;
    Reporter reporter(out, err, Verbosity::Quiet);
    Downloader downloader(client, reporter);
    FixtureProvider provider(fixture_remote_metadata(payload.size(), true));

    const auto summary = downloader.download(request, dataset, provider);
    const auto final_path = temp_dir.path() / dataset.id / "fixture.nc";
    return expect(summary.downloaded_count() == 1, "legacy lock without peer recovered")
        && expect(get_calls == 1, "get called after legacy lock cleanup")
        && expect(std::filesystem::exists(final_path), "output exists after legacy lock cleanup")
        && expect(!path_has_lock_artifact(lock_path), "lock artifact removed after legacy lock cleanup");
}

#ifndef _WIN32
bool test_downloader_recovers_legacy_lock_file_without_peer_process() {
    TempDir temp_dir;
    auto dataset = fixture_dataset();
    auto request = fixture_request(temp_dir.path());

    const auto lock_path = temp_dir.path() / dataset.id / "fixture.nc.lock";
    std::filesystem::create_directories(lock_path.parent_path());
    {
        std::ofstream lock_file(lock_path, std::ios::binary | std::ios::trunc);
        lock_file << "pid 999999\n";
    }

    const auto payload = valid_netcdf_bytes();
    int get_calls = 0;
    FakeHttpClient client;
    client.on_head = [&](const HttpRequest&) { return HttpResponse{}; };
    client.on_get = [&](const HttpRequest&, ResponseHandler& handler) {
        ++get_calls;
        handler.on_response_start(
            HttpResponse{
                .status_code = 200,
                .headers = {
                    {"content-length", std::to_string(payload.size())},
                },
                .bytes_transferred = 0,
            }
        );
        handler.on_chunk(std::string_view(payload.data(), payload.size()));
        return HttpResponse{
            .status_code = 200,
            .headers = {
                {"content-length", std::to_string(payload.size())},
            },
            .bytes_transferred = payload.size(),
        };
    };

    std::ostringstream out;
    std::ostringstream err;
    Reporter reporter(out, err, Verbosity::Quiet);
    Downloader downloader(client, reporter);
    FixtureProvider provider(fixture_remote_metadata(payload.size(), true));

    const auto summary = downloader.download(request, dataset, provider);
    const auto final_path = temp_dir.path() / dataset.id / "fixture.nc";
    return expect(summary.downloaded_count() == 1, "legacy lock file without peer recovered")
        && expect(get_calls == 1, "get called after legacy lock file cleanup")
        && expect(std::filesystem::exists(final_path), "output exists after legacy lock file cleanup")
        && expect(!path_has_lock_artifact(lock_path), "legacy lock file removed after recovery");
}
#endif

bool test_safe_replace_file_restores_existing_output_on_failure() {
    TempDir temp_dir;
    const auto destination = temp_dir.path() / "fixture.nc";
    const auto missing_source = temp_dir.path() / "missing.nc";
    const auto backup = temp_dir.path() / "fixture.nc.replace-backup";

    {
        std::ofstream output(destination, std::ios::binary | std::ios::trunc);
        output << "old-payload";
    }

    bool threw = false;
    try {
        safe_replace_file(missing_source, destination);
    } catch (const std::exception&) {
        threw = true;
    }

    std::ifstream input(destination, std::ios::binary);
    std::string contents;
    input >> contents;

    return expect(threw, "safe replace throws when source missing")
        && expect(std::filesystem::exists(destination), "destination restored after replace failure")
        && expect(contents == "old-payload", "destination contents restored after replace failure")
        && expect(!std::filesystem::exists(backup), "backup removed after replace rollback");
}

bool test_downloader_retries_transient_network_error() {
    TempDir temp_dir;
    auto dataset = fixture_dataset();
    auto request = fixture_request(temp_dir.path());
    request.retry_count = 2;
    request.normalize_and_validate();
    const auto payload = valid_netcdf_bytes();

    int get_calls = 0;
    FakeHttpClient client;
    client.on_head = [&](const HttpRequest&) {
        return HttpResponse{};
    };
    client.on_get = [&](const HttpRequest&, ResponseHandler& handler) {
        ++get_calls;
        if (get_calls == 1) {
            throw NetworkError("timeout", CURLE_OPERATION_TIMEDOUT);
        }

        handler.on_response_start(
            HttpResponse{
                .status_code = 200,
                .headers = {{"content-length", std::to_string(payload.size())}},
                .bytes_transferred = 0,
            }
        );
        handler.on_chunk(std::string_view(payload.data(), payload.size()));
        return HttpResponse{
            .status_code = 200,
            .headers = {{"content-length", std::to_string(payload.size())}},
            .bytes_transferred = payload.size(),
        };
    };

    std::ostringstream out;
    std::ostringstream err;
    Reporter reporter(out, err, Verbosity::Quiet);
    Downloader downloader(client, reporter);
    FixtureProvider provider(fixture_remote_metadata(payload.size(), true));

    const auto summary = downloader.download(request, dataset, provider);
    return expect(summary.downloaded_count() == 1, "transient network error eventually succeeds")
        && expect(summary.results.at(0).attempts == 2, "transient network error retried once")
        && expect(get_calls == 2, "transient network error triggered second get");
}

bool test_downloader_retries_http_503_without_corrupting_partial() {
    TempDir temp_dir;
    auto dataset = fixture_dataset();
    auto request = fixture_request(temp_dir.path());
    request.retry_count = 1;
    request.normalize_and_validate();
    const auto payload = valid_netcdf_bytes();

    const auto part_path = temp_dir.path() / dataset.id / "fixture.nc.part";
    const auto meta_path = temp_dir.path() / dataset.id / "fixture.nc.part.meta";
    std::filesystem::create_directories(part_path.parent_path());
    std::ofstream partial(part_path, std::ios::binary);
    partial.write(payload.data(), 16);
    partial.close();
    write_partial_metadata_file(meta_path, payload.size(), "\"fixture-v1\"");

    std::vector<std::string> range_values;
    int get_calls = 0;
    FakeHttpClient client;
    client.on_head = [&](const HttpRequest&) {
        return HttpResponse{
            .status_code = 200,
            .headers = {
                {"content-length", std::to_string(payload.size())},
                {"accept-ranges", "bytes"},
                {"etag", "\"fixture-v1\""},
            },
            .bytes_transferred = 0,
        };
    };
    client.on_get = [&](const HttpRequest& request, ResponseHandler& handler) {
        ++get_calls;
        range_values.push_back(request_header_value(request, "Range: "));

        if (get_calls == 1) {
            constexpr std::string_view kErrorPayload = "service unavailable";
            handler.on_response_start(
                HttpResponse{
                    .status_code = 503,
                    .headers = {},
                    .bytes_transferred = 0,
                }
            );
            handler.on_chunk(kErrorPayload);
            return HttpResponse{
                .status_code = 503,
                .headers = {},
                .bytes_transferred = kErrorPayload.size(),
            };
        }

        handler.on_response_start(
            HttpResponse{
                .status_code = 206,
                .headers = {
                    {"content-length", std::to_string(payload.size() - 16)},
                    {"content-range", "bytes 16-31/32"},
                },
                .bytes_transferred = 0,
            }
        );
        handler.on_chunk(std::string_view(payload.data() + 16, payload.size() - 16));
        return HttpResponse{
            .status_code = 206,
            .headers = {
                {"content-length", std::to_string(payload.size() - 16)},
                {"content-range", "bytes 16-31/32"},
            },
            .bytes_transferred = payload.size() - 16,
        };
    };

    std::ostringstream out;
    std::ostringstream err;
    Reporter reporter(out, err, Verbosity::Quiet);
    Downloader downloader(client, reporter);
    FixtureProvider provider(fixture_remote_metadata(payload.size(), true, "\"fixture-v1\""));

    const auto summary = downloader.download(request, dataset, provider);
    const auto final_path = temp_dir.path() / dataset.id / "fixture.nc";
    const auto validation = validate_netcdf_file(final_path, payload.size());

    return expect(summary.downloaded_count() == 1, "http 503 retry eventually succeeds")
        && expect(summary.results.at(0).attempts == 2, "http 503 consumed one retry")
        && expect(get_calls == 2, "http 503 triggered a second get")
        && expect(range_values.size() == 2, "range values recorded for each attempt")
        && expect(range_values.at(0) == "bytes=16-", "first attempt resumed from original partial")
        && expect(range_values.at(1) == "bytes=16-", "retry reused uncorrupted partial offset")
        && expect(validation.valid, "final file remains valid netcdf after retry")
        && expect(!std::filesystem::exists(part_path), "part removed after successful retry")
        && expect(!std::filesystem::exists(meta_path), "part metadata removed after successful retry");
}

bool test_downloader_rejects_download_without_verifiable_size() {
    TempDir temp_dir;
    auto dataset = fixture_dataset();
    auto request = fixture_request(temp_dir.path());
    const auto payload = valid_netcdf_bytes();

    FakeHttpClient client;
    client.on_head = [&](const HttpRequest&) {
        return HttpResponse{
            .status_code = 200,
            .headers = {{"etag", "\"fixture-v1\""}},
            .bytes_transferred = 0,
        };
    };
    client.on_get = [&](const HttpRequest&, ResponseHandler& handler) {
        handler.on_response_start(
            HttpResponse{
                .status_code = 200,
                .headers = {{"etag", "\"fixture-v1\""}},
                .bytes_transferred = 0,
            }
        );
        handler.on_chunk(std::string_view(payload.data(), payload.size()));
        return HttpResponse{
            .status_code = 200,
            .headers = {{"etag", "\"fixture-v1\""}},
            .bytes_transferred = payload.size(),
        };
    };

    std::ostringstream out;
    std::ostringstream err;
    Reporter reporter(out, err, Verbosity::Quiet);
    Downloader downloader(client, reporter);
    FixtureProvider provider(fixture_remote_metadata(std::nullopt, true, "\"fixture-v1\""));

    const auto summary = downloader.download(request, dataset, provider);
    const auto output_path = temp_dir.path() / dataset.id / "fixture.nc";
    const auto part_path = output_path.parent_path() / "fixture.nc.part";
    const auto meta_path = output_path.parent_path() / "fixture.nc.part.meta";
    return expect(summary.failed_count() == 1, "download without size metadata fails safely")
        && expect_contains(
            summary.failures().at(0).error_message,
            "verifiable file size",
            "missing-size failure message"
        )
        && expect(!std::filesystem::exists(output_path), "no final output when size is unverifiable")
        && expect(!std::filesystem::exists(part_path), "partial data removed when size is unverifiable")
        && expect(!std::filesystem::exists(meta_path), "partial metadata removed when size is unverifiable");
}

bool test_downloader_does_not_retry_permanent_network_error() {
    TempDir temp_dir;
    auto dataset = fixture_dataset();
    auto request = fixture_request(temp_dir.path());
    request.retry_count = 2;
    request.normalize_and_validate();

    int get_calls = 0;
    FakeHttpClient client;
    client.on_head = [&](const HttpRequest&) {
        return HttpResponse{};
    };
    client.on_get = [&](const HttpRequest&, ResponseHandler&) {
        ++get_calls;
        throw NetworkError("unsupported protocol", CURLE_UNSUPPORTED_PROTOCOL);
        return HttpResponse{};
    };

    std::ostringstream out;
    std::ostringstream err;
    Reporter reporter(out, err, Verbosity::Quiet);
    Downloader downloader(client, reporter);
    FixtureProvider provider(fixture_remote_metadata(32, true));

    const auto summary = downloader.download(request, dataset, provider);
    return expect(summary.failed_count() == 1, "permanent network error fails")
        && expect(summary.results.at(0).attempts == 1, "permanent network error not retried")
        && expect(get_calls == 1, "permanent network error only attempted once");
}

bool test_downloader_continues_when_head_metadata_is_blocked() {
    TempDir temp_dir;
    auto dataset = fixture_dataset("fixture_head");
    auto request = fixture_request(temp_dir.path());
    const auto payload = valid_netcdf_bytes();

    int head_calls = 0;
    int get_calls = 0;
    FakeHttpClient client;
    client.on_head = [&](const HttpRequest&) {
        ++head_calls;
        return HttpResponse{
            .status_code = 403,
            .headers = {},
            .bytes_transferred = 0,
        };
    };
    client.on_get = [&](const HttpRequest& request, ResponseHandler& handler) {
        ++get_calls;
        const bool has_range = request_has_header_prefix(request, "Range: ");
        if (has_range) {
            throw std::runtime_error("fallback GET should not attempt resume");
        }

        handler.on_response_start(
            HttpResponse{
                .status_code = 200,
                .headers = {{"content-length", std::to_string(payload.size())}},
                .bytes_transferred = 0,
            }
        );
        handler.on_chunk(std::string_view(payload.data(), payload.size()));
        return HttpResponse{
            .status_code = 200,
            .headers = {{"content-length", std::to_string(payload.size())}},
            .bytes_transferred = payload.size(),
        };
    };

    std::ostringstream out;
    std::ostringstream err;
    Reporter reporter(out, err, Verbosity::Quiet);
    Downloader downloader(client, reporter);
    MetadataFallbackFixtureProvider provider;

    const auto summary = downloader.download(request, dataset, provider);
    const auto final_path = temp_dir.path() / dataset.id / "fixture.nc";
    return expect(summary.downloaded_count() == 1, "download succeeds after HEAD fallback")
        && expect(std::filesystem::exists(final_path), "output exists after HEAD fallback")
        && expect(head_calls == 1, "head attempted once")
        && expect(get_calls == 1, "get attempted once after HEAD fallback");
}

bool test_downloader_continues_when_head_metadata_has_network_error() {
    TempDir temp_dir;
    auto dataset = fixture_dataset("fixture_head");
    auto request = fixture_request(temp_dir.path());
    const auto payload = valid_netcdf_bytes();

    int head_calls = 0;
    int get_calls = 0;
    FakeHttpClient client;
    client.on_head = [&](const HttpRequest&) {
        ++head_calls;
        throw NetworkError("head timeout", CURLE_OPERATION_TIMEDOUT);
        return HttpResponse{};
    };
    client.on_get = [&](const HttpRequest& request, ResponseHandler& handler) {
        ++get_calls;
        const bool has_range = request_has_header_prefix(request, "Range: ");
        if (has_range) {
            throw std::runtime_error("fallback GET should not attempt resume after head error");
        }

        handler.on_response_start(
            HttpResponse{
                .status_code = 200,
                .headers = {{"content-length", std::to_string(payload.size())}},
                .bytes_transferred = 0,
            }
        );
        handler.on_chunk(std::string_view(payload.data(), payload.size()));
        return HttpResponse{
            .status_code = 200,
            .headers = {{"content-length", std::to_string(payload.size())}},
            .bytes_transferred = payload.size(),
        };
    };

    std::ostringstream out;
    std::ostringstream err;
    Reporter reporter(out, err, Verbosity::Quiet);
    Downloader downloader(client, reporter);
    MetadataFallbackFixtureProvider provider;

    const auto summary = downloader.download(request, dataset, provider);
    const auto final_path = temp_dir.path() / dataset.id / "fixture.nc";
    return expect(summary.downloaded_count() == 1, "download succeeds after HEAD network fallback")
        && expect(std::filesystem::exists(final_path), "output exists after HEAD network fallback")
        && expect(head_calls == 1, "head attempted once before network fallback")
        && expect(get_calls == 1, "get attempted once after HEAD network fallback");
}

bool test_downloader_propagates_chunk_size_to_http_client() {
    TempDir temp_dir;
    auto dataset = fixture_dataset();
    auto request = fixture_request(temp_dir.path());
    request.chunk_size = 8192;
    request.normalize_and_validate();
    const auto payload = valid_netcdf_bytes();

    HttpRequest last_get_request;
    FakeHttpClient client;
    client.on_head = [&](const HttpRequest&) {
        return HttpResponse{
            .status_code = 200,
            .headers = {{"content-length", std::to_string(payload.size())}},
            .bytes_transferred = 0,
        };
    };
    client.on_get = [&](const HttpRequest& request, ResponseHandler& handler) {
        last_get_request = request;
        handler.on_response_start(
            HttpResponse{
                .status_code = 200,
                .headers = {{"content-length", std::to_string(payload.size())}},
                .bytes_transferred = 0,
            }
        );
        handler.on_chunk(std::string_view(payload.data(), payload.size()));
        return HttpResponse{
            .status_code = 200,
            .headers = {{"content-length", std::to_string(payload.size())}},
            .bytes_transferred = payload.size(),
        };
    };

    std::ostringstream out;
    std::ostringstream err;
    Reporter reporter(out, err, Verbosity::Quiet);
    Downloader downloader(client, reporter);
    FixtureProvider provider(fixture_remote_metadata(payload.size(), true));

    const auto summary = downloader.download(request, dataset, provider);
    return expect(summary.downloaded_count() == 1, "download succeeds with custom chunk size")
        && expect(
            last_get_request.buffer_size_bytes == request.chunk_size,
            "http request receives configured chunk size"
        );
}

bool test_config_load_merges_dataset_urls() {
    TempDir temp_dir;
    const auto config_path = temp_dir.path() / "config.toml";
    std::ofstream config(config_path);
    config
        << "default_dataset = \"gpcp\"\n"
        << "timeout = 90\n"
        << "[provider_base_urls]\n"
        << "psl = \"https://mirror.example.test/\"\n"
        << "[dataset_base_urls]\n"
        << "gpcp = \"https://example.test/gpcp/\"\n";
    config.close();

    const auto loaded = load_config(config_path);
    return expect(loaded.default_dataset == "gpcp", "loaded config dataset")
        && expect(loaded.timeout == 90.0, "loaded config timeout")
        && expect(
            loaded.provider_base_urls.at("psl") == "https://mirror.example.test",
            "loaded provider base url trims slash"
        )
        && expect(
            loaded.dataset_base_urls.at("gpcp") == "https://example.test/gpcp",
            "loaded config trims slash"
        )
        && expect(loaded.provider_base_urls.contains("psl"), "default provider url merge");
}

bool test_config_load_rejects_invalid_scalar_type() {
    TempDir temp_dir;
    const auto config_path = temp_dir.path() / "config.toml";
    std::ofstream config(config_path);
    config << "timeout = \"fast\"\n";
    config.close();

    try {
        (void)load_config(config_path);
        return expect(false, "invalid timeout type should fail");
    } catch (const std::exception& error) {
        return expect_contains(error.what(), "config 'timeout' must be a number", "invalid timeout type message");
    }
}

bool test_config_load_rejects_excessive_retry_count() {
    TempDir temp_dir;
    const auto config_path = temp_dir.path() / "config.toml";
    std::ofstream config(config_path);
    config << "retry_count = " << (kMaxRetryCount + 1) << '\n';
    config.close();

    try {
        (void)load_config(config_path);
        return expect(false, "excessive retry_count in config should fail");
    } catch (const std::exception& error) {
        return expect_contains(
            error.what(),
            "retry_count must be between 0 and",
            "config excessive retry_count message"
        );
    }
}

bool test_config_load_rejects_invalid_url_value() {
    TempDir temp_dir;
    const auto config_path = temp_dir.path() / "config.toml";
    std::ofstream config(config_path);
    config << "[provider_base_urls]\npsl = \"not-a-url\"\n";
    config.close();

    try {
        (void)load_config(config_path);
        return expect(false, "invalid provider url should fail");
    } catch (const std::exception& error) {
        return expect_contains(error.what(), "provider_base_urls.psl must be a valid http/https URL", "invalid provider url message");
    }
}

bool test_config_load_reports_unknown_keys() {
    TempDir temp_dir;
    const auto config_path = temp_dir.path() / "config.toml";
    std::ofstream config(config_path);
    config << "default_dataset = \"gpcp\"\nunknown_key = \"value\"\n";
    config.close();

    const auto loaded = load_config_with_diagnostics(config_path);
    return expect(loaded.config.default_dataset == "gpcp", "known config value still loaded")
        && expect(loaded.loaded_from_file, "config report marks file as loaded")
        && expect(loaded.warnings.size() == 1, "unknown key emitted one warning")
        && expect_contains(loaded.warnings.front(), "unknown_key", "unknown key warning contains key name");
}

bool test_parse_retry_after_seconds_clamps_numeric_header() {
    const HeaderMap headers{
        {"retry-after", "600"},
    };

    const auto retry_after = parse_retry_after_seconds(headers);
    return expect(retry_after.has_value(), "retry-after parsed from header")
        && expect(*retry_after == 300, "retry-after clamped to supported maximum");
}

bool test_catalog_builds_dataset_urls_from_provider_base_url() {
    auto config = default_app_config();
    config.provider_base_urls["psl"] = "https://mirror.example.test/";
    config.normalize_and_validate();

    const auto registry = build_default_dataset_registry(config);
    return expect(
               registry.get("gpcp").base_url == "https://mirror.example.test/Datasets/gpcp",
               "catalog builds gpcp url from provider base url"
           )
        && expect(
            registry.get("oisst").base_url
                == "https://mirror.example.test/Datasets/noaa.oisst.v2.highres",
            "catalog builds oisst url from provider base url"
        );
}

bool test_catalog_prefers_legacy_dataset_url_override() {
    auto config = default_app_config();
    config.provider_base_urls["psl"] = "https://mirror.example.test";
    config.dataset_base_urls["gpcp"] = "https://legacy.example.test/custom-gpcp/";
    config.normalize_and_validate();

    const auto registry = build_default_dataset_registry(config);
    return expect(
        registry.get("gpcp").base_url == "https://legacy.example.test/custom-gpcp",
        "legacy dataset-specific override still wins"
    );
}

bool test_version_constant_present() {
    return expect(!std::string(kVersion).empty(), "version constant");
}

bool test_reporter_structures_plain_output_without_color() {
    std::ostringstream out;
    std::ostringstream err;
    Reporter reporter(out, err, Verbosity::Normal);

    reporter.section("Example");
    reporter.field("status", "ready", 8);
    reporter.progress("demo.nc", 512, 1024);
    reporter.finish_progress();
    reporter.success("done");
    reporter.error("failed");

    return expect_contains(out.str(), "== Example ==", "reporter section heading")
        && expect_contains(out.str(), "status", "reporter field label")
        && expect_contains(out.str(), "[ok] done", "reporter success prefix")
        && expect(out.str().find('\r') == std::string::npos, "reporter progress disabled on non-tty")
        && expect_contains(err.str(), "[err] failed", "reporter error prefix");
}

bool test_cli_shows_global_help_for_help_flag() {
    CliApp app;
    std::ostringstream out;
    std::ostringstream err;

    const auto exit_code = app.run({"--help"}, out, err);

    return expect(exit_code == 0, "cli exit code for global help")
        && expect_contains(out.str(), "== oceandl ", "global help header shown")
        && expect_contains(out.str(), "help [command]", "global help lists help command")
        && expect(err.str().empty(), "global help does not write errors");
}

bool test_cli_shows_download_help() {
    CliApp app;
    std::ostringstream out;
    std::ostringstream err;

    const auto exit_code = app.run({"download", "--help"}, out, err);

    return expect(exit_code == 0, "cli exit code for download help")
        && expect_contains(out.str(), "download command", "download help header shown")
        && expect_contains(out.str(), "--chunk-size BYTES", "download help lists chunk size option")
        && expect(err.str().empty(), "download help does not write errors");
}

bool test_cli_shows_help_even_when_config_is_invalid() {
    TempDir temp_dir;
    const auto config_path = temp_dir.path() / "config.toml";
    std::ofstream config(config_path);
    config << "timeout = \"fast\"\n";
    config.close();

    CliApp app;
    std::ostringstream out;
    std::ostringstream err;
    const auto exit_code = app.run({"--config", config_path.string(), "download", "--help"}, out, err);

    return expect(exit_code == 0, "help should succeed with invalid config")
        && expect_contains(out.str(), "download command", "download help still shown with invalid config")
        && expect(err.str().empty(), "invalid config should not block help");
}

bool test_cli_info_falls_back_when_config_is_invalid() {
    TempDir temp_dir;
    const auto config_path = temp_dir.path() / "config.toml";
    std::ofstream config(config_path);
    config << "[provider_base_urls]\npsl = 123\n";
    config.close();

    CliApp app;
    std::ostringstream out;
    std::ostringstream err;
    const auto exit_code = app.run({"--config", config_path.string(), "info", "gpcp"}, out, err);

    return expect(exit_code == 0, "info should fall back when config invalid")
        && expect_contains(out.str(), "Config is invalid; using built-in metadata instead", "fallback warning shown")
        && expect_contains(out.str(), "Dataset gpcp", "info output still shown")
        && expect(err.str().empty(), "fallback should not write hard error");
}

bool test_cli_rejects_invalid_year_suffix() {
    CliApp app;
    std::ostringstream out;
    std::ostringstream err;

    const auto exit_code = app.run(
        {
            "download",
            "oisst",
            "--start-year",
            "2024abc",
            "--end-year",
            "2024",
        },
        out,
        err
    );

    return expect(exit_code == 2, "cli exit code for invalid year suffix")
        && expect_contains(err.str(), "Invalid value for --start-year: 2024abc", "cli invalid year message");
}

bool test_cli_rejects_invalid_timeout_suffix() {
    CliApp app;
    std::ostringstream out;
    std::ostringstream err;

    const auto exit_code = app.run(
        {
            "download",
            "gpcp",
            "--timeout",
            "30s",
        },
        out,
        err
    );

    return expect(exit_code == 2, "cli exit code for invalid timeout suffix")
        && expect_contains(err.str(), "Invalid value for --timeout: 30s", "cli invalid timeout message");
}

bool test_cli_rejects_non_finite_timeout() {
    CliApp app;
    std::ostringstream out;
    std::ostringstream err;

    const auto exit_code = app.run({"download", "gpcp", "--timeout", "inf"}, out, err);

    return expect(exit_code == 2, "cli exit code for non-finite timeout")
        && expect_contains(err.str(), "Invalid value for --timeout: inf", "cli non-finite timeout message");
}

bool test_cli_rejects_excessive_retry_count() {
    CliApp app;
    std::ostringstream out;
    std::ostringstream err;

    const auto exit_code = app.run(
        {
            "download",
            "gpcp",
            "--retries",
            std::to_string(kMaxRetryCount + 1),
        },
        out,
        err
    );

    return expect(exit_code == 2, "cli exit code for excessive retry_count")
        && expect_contains(
            err.str(),
            "retry_count must be between 0 and",
            "cli excessive retry_count message"
        );
}

bool test_cli_rejects_year_flags_for_single_file_dataset() {
    TempDir temp_dir;
    CliApp app;
    std::ostringstream out;
    std::ostringstream err;

    const auto exit_code = app.run(
        {
            "download",
            "gpcp",
            "--start-year",
            "2024",
            "--end-year",
            "2024",
            "--output-dir",
            temp_dir.path().string(),
        },
        out,
        err
    );

    return expect(exit_code == 2, "cli exit code for single-file year flags")
        && expect_contains(err.str(), "does not accept --start-year or --end-year", "single-file year flag message");
}

bool test_dataset_rejects_future_and_excessive_year_ranges() {
    const auto registry = build_default_dataset_registry(default_app_config());
    const auto& dataset = registry.get("oisst");

    bool rejected_future = false;
    bool rejected_excessive = false;

    try {
        dataset.validate_requested_years(current_calendar_year() + 1, current_calendar_year() + 1);
    } catch (const std::exception&) {
        rejected_future = true;
    }

    try {
        dataset.validate_requested_years(1981, 1981 + 500);
    } catch (const std::exception&) {
        rejected_excessive = true;
    }

    return expect(rejected_future, "future year rejected")
        && expect(rejected_excessive, "excessive year span rejected");
}

}  // namespace

int main() {
    const std::vector<std::pair<std::string, std::function<bool()>>> tests = {
        {"registry_contains_expected_datasets", test_registry_contains_expected_datasets},
        {"download_request_validation", test_download_request_validation},
        {"download_request_rejects_excessive_retry_count", test_download_request_rejects_excessive_retry_count},
        {"download_command_parses_flags", test_download_command_parses_flags},
        {"download_command_resolves_dataset", test_download_command_resolves_dataset_from_option_or_config},
        {"download_command_rejects_conflict", test_download_command_rejects_conflicting_dataset_values},
        {"psl_provider_targets", test_psl_provider_targets},
        {"download_target_layout_is_centralized", test_download_target_layout_is_centralized},
        {"validation_accepts_minimal_netcdf", test_validation_accepts_minimal_netcdf},
        {"validation_rejects_invalid_netcdf_version_byte", test_validation_rejects_invalid_netcdf_version_byte},
        {"dataset_validation_uses_dataset_metadata", test_dataset_validation_uses_dataset_metadata},
        {"downloader_cleans_up_lock_artifact_after_successful_download", test_downloader_cleans_up_lock_artifact_after_successful_download},
        {"downloader_skips_existing_valid_file", test_downloader_skips_existing_valid_file},
        {"downloader_redownloads_existing_file_when_remote_size_unknown", test_downloader_redownloads_existing_file_when_remote_size_unknown},
        {"downloader_rejects_partial_on_416_without_remote_size", test_downloader_rejects_partial_on_416_without_remote_size},
        {"downloader_discards_partial_when_ranges_unavailable", test_downloader_discards_partial_when_ranges_unavailable},
        {"downloader_discards_partial_when_etag_changes", test_downloader_discards_partial_when_etag_changes},
        {"downloader_sends_if_range_when_resuming", test_downloader_sends_if_range_when_resuming},
        {"downloader_fails_when_target_is_locked", test_downloader_fails_when_target_is_locked},
        {"downloader_recovers_legacy_lock_directory_without_peer_process", test_downloader_recovers_legacy_lock_directory_without_peer_process},
#ifndef _WIN32
        {"downloader_recovers_legacy_lock_file_without_peer_process", test_downloader_recovers_legacy_lock_file_without_peer_process},
#endif
        {"safe_replace_file_restores_existing_output_on_failure", test_safe_replace_file_restores_existing_output_on_failure},
        {"downloader_retries_transient_network_error", test_downloader_retries_transient_network_error},
        {"downloader_retries_http_503_without_corrupting_partial", test_downloader_retries_http_503_without_corrupting_partial},
        {"downloader_rejects_download_without_verifiable_size", test_downloader_rejects_download_without_verifiable_size},
        {"downloader_does_not_retry_permanent_network_error", test_downloader_does_not_retry_permanent_network_error},
        {"downloader_continues_when_head_metadata_is_blocked", test_downloader_continues_when_head_metadata_is_blocked},
        {"downloader_continues_when_head_metadata_has_network_error", test_downloader_continues_when_head_metadata_has_network_error},
        {"downloader_propagates_chunk_size_to_http_client", test_downloader_propagates_chunk_size_to_http_client},
        {"config_load_merges_dataset_urls", test_config_load_merges_dataset_urls},
        {"config_load_rejects_invalid_scalar_type", test_config_load_rejects_invalid_scalar_type},
        {"config_load_rejects_excessive_retry_count", test_config_load_rejects_excessive_retry_count},
        {"config_load_rejects_invalid_url_value", test_config_load_rejects_invalid_url_value},
        {"config_load_reports_unknown_keys", test_config_load_reports_unknown_keys},
        {"parse_retry_after_seconds_clamps_numeric_header", test_parse_retry_after_seconds_clamps_numeric_header},
        {"catalog_builds_dataset_urls_from_provider_base_url", test_catalog_builds_dataset_urls_from_provider_base_url},
        {"catalog_prefers_legacy_dataset_url_override", test_catalog_prefers_legacy_dataset_url_override},
        {"version_constant_present", test_version_constant_present},
        {"reporter_structures_plain_output_without_color", test_reporter_structures_plain_output_without_color},
        {"cli_shows_global_help_for_help_flag", test_cli_shows_global_help_for_help_flag},
        {"cli_shows_download_help", test_cli_shows_download_help},
        {"cli_shows_help_even_when_config_is_invalid", test_cli_shows_help_even_when_config_is_invalid},
        {"cli_info_falls_back_when_config_is_invalid", test_cli_info_falls_back_when_config_is_invalid},
        {"cli_rejects_invalid_year_suffix", test_cli_rejects_invalid_year_suffix},
        {"cli_rejects_invalid_timeout_suffix", test_cli_rejects_invalid_timeout_suffix},
        {"cli_rejects_non_finite_timeout", test_cli_rejects_non_finite_timeout},
        {"cli_rejects_excessive_retry_count", test_cli_rejects_excessive_retry_count},
        {"cli_rejects_year_flags_for_single_file_dataset", test_cli_rejects_year_flags_for_single_file_dataset},
        {"dataset_rejects_future_and_excessive_year_ranges", test_dataset_rejects_future_and_excessive_year_ranges},
    };

    bool ok = true;
    for (const auto& [name, test] : tests) {
        if (!test()) {
            std::cerr << "Test failed: " << name << '\n';
            ok = false;
        }
    }
    if (!ok) {
        return 1;
    }

    std::cout << "All tests passed\n";
    return 0;
}
