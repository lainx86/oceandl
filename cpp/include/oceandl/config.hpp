#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <string>

namespace oceandl {

struct AppConfig {
    std::string default_dataset = "oisst";
    std::filesystem::path default_output_dir;
    double timeout = 60.0;
    std::uint64_t chunk_size = 1024 * 1024;
    int retry_count = 3;
    bool overwrite = false;
    bool resume = true;
    std::string user_agent;
    std::map<std::string, std::string> dataset_base_urls;

    void normalize_and_validate();
};

std::filesystem::path default_config_path();
std::filesystem::path default_output_dir();
std::map<std::string, std::string> default_dataset_base_urls();
AppConfig default_app_config();
AppConfig load_config(const std::filesystem::path& path);

}  // namespace oceandl
