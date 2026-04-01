#include "oceandl/config.hpp"

#include <stdexcept>

#include <fmt/format.h>
#include <toml++/toml.hpp>

#include "builtin_datasets.hpp"
#include "oceandl/utils.hpp"
#include "oceandl/version.hpp"

namespace oceandl {

std::filesystem::path default_config_path() {
    return expand_user(std::filesystem::path("~/.config/oceandl/config.toml"));
}

std::filesystem::path default_output_dir() {
    return expand_user(std::filesystem::path("~/data/oceandl"));
}

std::map<std::string, std::string> default_provider_base_urls() {
    return builtin_provider_base_urls();
}

std::map<std::string, std::string> default_dataset_base_urls() {
    return builtin_dataset_base_urls();
}

AppConfig default_app_config() {
    AppConfig config;
    config.default_output_dir = default_output_dir();
    config.user_agent = fmt::format("oceandl/{}", kVersion);
    config.provider_base_urls = default_provider_base_urls();
    config.normalize_and_validate();
    return config;
}

void AppConfig::normalize_and_validate() {
    default_dataset = to_lower(trim(default_dataset));
    default_output_dir = expand_user(default_output_dir);
    user_agent = trim(user_agent);

    if (default_dataset.empty()) {
        throw std::invalid_argument("default_dataset tidak boleh kosong.");
    }
    if (timeout <= 0) {
        throw std::invalid_argument("timeout harus lebih besar dari 0.");
    }
    if (chunk_size < 1024) {
        throw std::invalid_argument("chunk_size minimal 1024 bytes.");
    }
    if (retry_count < 0) {
        throw std::invalid_argument("retry_count tidak boleh negatif.");
    }
    if (user_agent.empty()) {
        user_agent = fmt::format("oceandl/{}", kVersion);
    }

    auto normalize_url_map = [](const std::map<std::string, std::string>& source, const char* field_name) {
        std::map<std::string, std::string> normalized;
        for (const auto& [key, value] : source) {
            const auto normalized_key = to_lower(trim(key));
            const auto normalized_value = trim_trailing_slash(value);
            if (normalized_key.empty() || normalized_value.empty()) {
                throw std::invalid_argument(std::string(field_name) + " tidak boleh berisi key/url kosong.");
            }
            normalized[normalized_key] = normalized_value;
        }
        return normalized;
    };

    provider_base_urls = normalize_url_map(provider_base_urls, "provider_base_urls");
    dataset_base_urls = normalize_url_map(dataset_base_urls, "dataset_base_urls");
}

AppConfig load_config(const std::filesystem::path& path) {
    auto config = default_app_config();
    const auto config_path = expand_user(path);

    if (!std::filesystem::exists(config_path)) {
        return config;
    }

    try {
        const auto table = toml::parse_file(config_path.string());

        if (const auto value = table["default_dataset"].value<std::string>()) {
            config.default_dataset = *value;
        }
        if (const auto value = table["default_output_dir"].value<std::string>()) {
            config.default_output_dir = *value;
        }
        if (const auto value = table["timeout"].value<double>()) {
            config.timeout = *value;
        }
        if (const auto value = table["chunk_size"].value<std::int64_t>()) {
            config.chunk_size = static_cast<std::uint64_t>(*value);
        }
        if (const auto value = table["retry_count"].value<std::int64_t>()) {
            config.retry_count = static_cast<int>(*value);
        }
        if (const auto value = table["overwrite"].value<bool>()) {
            config.overwrite = *value;
        }
        if (const auto value = table["resume"].value<bool>()) {
            config.resume = *value;
        }
        if (const auto value = table["user_agent"].value<std::string>()) {
            config.user_agent = *value;
        }

        if (const auto* provider_base_urls = table["provider_base_urls"].as_table()) {
            for (const auto& [key, node] : *provider_base_urls) {
                if (const auto value = node.value<std::string>()) {
                    config.provider_base_urls[to_lower(trim(key.str()))] = *value;
                }
            }
        }

        if (const auto* dataset_base_urls = table["dataset_base_urls"].as_table()) {
            for (const auto& [key, node] : *dataset_base_urls) {
                if (const auto value = node.value<std::string>()) {
                    config.dataset_base_urls[to_lower(trim(key.str()))] = *value;
                }
            }
        }
    } catch (const toml::parse_error& error) {
        throw std::invalid_argument(std::string(error.description()));
    }

    config.normalize_and_validate();
    return config;
}

}  // namespace oceandl
