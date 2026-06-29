#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace oceandl {

enum class CopernicusMarineRunner {
    System,
    Micromamba,
    Conda,
};

struct CopernicusMarineConfig {
    std::filesystem::path executable;
    CopernicusMarineRunner runner = CopernicusMarineRunner::System;
    std::string env;

    void normalize_and_validate();
};

struct AppConfig {
    std::string default_dataset = "oisst";
    std::filesystem::path default_output_dir;
    double timeout = 60.0;
    std::uint64_t chunk_size = 1024 * 1024;
    int retry_count = 3;
    bool overwrite = false;
    bool resume = true;
    std::string user_agent;
    std::map<std::string, std::string> provider_base_urls;
    std::map<std::string, std::string> dataset_base_urls;
    CopernicusMarineConfig copernicusmarine;

    void normalize_and_validate();
};

struct ConfigLoadResult {
    AppConfig config;
    std::filesystem::path path;
    std::vector<std::string> warnings;
    bool loaded_from_file = false;
};

std::filesystem::path default_config_path();
std::filesystem::path default_output_dir();
std::map<std::string, std::string> default_provider_base_urls();
AppConfig default_app_config();
std::string copernicusmarine_runner_name(CopernicusMarineRunner runner);
CopernicusMarineRunner parse_copernicusmarine_runner(std::string_view value);
ConfigLoadResult load_config_with_diagnostics(const std::filesystem::path& path);
AppConfig load_config(const std::filesystem::path& path);
void save_copernicusmarine_config(
    const std::filesystem::path& path,
    const CopernicusMarineConfig& config
);

}  // namespace oceandl
