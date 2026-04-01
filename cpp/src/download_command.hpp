#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "oceandl/catalog.hpp"
#include "oceandl/config.hpp"
#include "oceandl/providers/registry.hpp"
#include "oceandl/reporter.hpp"

namespace oceandl {

struct DownloadCommandOptions {
    std::optional<std::string> dataset_argument;
    std::optional<std::string> dataset_option;
    std::optional<int> start_year;
    std::optional<int> end_year;
    std::optional<std::filesystem::path> output_dir;
    std::optional<bool> overwrite;
    std::optional<bool> resume;
    std::optional<double> timeout;
    std::optional<std::uint64_t> chunk_size;
    std::optional<int> retries;
    bool help = false;
};

DownloadCommandOptions parse_download_options(const std::vector<std::string>& args);
std::string resolve_dataset_name(const DownloadCommandOptions& options, const AppConfig& config);
void print_download_help(const Reporter& reporter);
int run_download_command(
    const DownloadCommandOptions& options,
    const AppConfig& config,
    const DatasetRegistry& dataset_registry,
    const ProviderRegistry& provider_registry,
    Reporter& reporter
);

}  // namespace oceandl
