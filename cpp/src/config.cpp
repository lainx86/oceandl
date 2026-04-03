#include "oceandl/config.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <optional>
#include <stdexcept>

#include <fmt/format.h>
#include <toml++/toml.hpp>

#include "builtin_datasets.hpp"
#include "oceandl/models.hpp"
#include "oceandl/utils.hpp"
#include "oceandl/version.hpp"

namespace oceandl {

namespace {

constexpr double kMaxTimeoutSeconds = 86400.0;

std::optional<std::filesystem::path> getenv_path(const char* name) {
    const char* value = std::getenv(name);
    if (value == nullptr || std::string_view(value).empty()) {
        return std::nullopt;
    }
    return std::filesystem::path(value);
}

std::filesystem::path default_config_dir() {
#ifdef _WIN32
    if (const auto app_data = getenv_path("APPDATA")) {
        return *app_data / "oceandl";
    }
    if (const auto local_app_data = getenv_path("LOCALAPPDATA")) {
        return *local_app_data / "oceandl";
    }
    return expand_user(std::filesystem::path("~/AppData/Roaming/oceandl"));
#elif defined(__APPLE__)
    return expand_user(std::filesystem::path("~/Library/Application Support/oceandl"));
#else
    if (const auto xdg_config_home = getenv_path("XDG_CONFIG_HOME")) {
        return *xdg_config_home / "oceandl";
    }
    return expand_user(std::filesystem::path("~/.config/oceandl"));
#endif
}

std::filesystem::path default_data_dir() {
#ifdef _WIN32
    if (const auto local_app_data = getenv_path("LOCALAPPDATA")) {
        return *local_app_data / "oceandl" / "data";
    }
    if (const auto app_data = getenv_path("APPDATA")) {
        return *app_data / "oceandl" / "data";
    }
    return expand_user(std::filesystem::path("~/AppData/Local/oceandl/data"));
#elif defined(__APPLE__)
    return expand_user(std::filesystem::path("~/Library/Application Support/oceandl/data"));
#else
    if (const auto xdg_data_home = getenv_path("XDG_DATA_HOME")) {
        return *xdg_data_home / "oceandl";
    }
    return expand_user(std::filesystem::path("~/data/oceandl"));
#endif
}

template <typename T>
std::optional<T> parse_scalar_or_throw(
    const toml::table& table,
    const char* key,
    const char* expected_type
) {
    if (const auto* node = table.get(key)) {
        if (const auto value = node->value<T>()) {
            return *value;
        }
        throw std::invalid_argument(fmt::format("config '{}' must be a {}.", key, expected_type));
    }
    return std::nullopt;
}

const toml::table* parse_table_or_throw(const toml::table& root, const char* key) {
    if (const auto* node = root.get(key)) {
        if (const auto* table = node->as_table()) {
            return table;
        }
        throw std::invalid_argument(fmt::format("config '{}' must be a TOML table.", key));
    }
    return nullptr;
}

std::map<std::string, std::string> parse_string_map_or_throw(
    const toml::table& root,
    const char* table_key
) {
    std::map<std::string, std::string> result;
    if (const auto* table = parse_table_or_throw(root, table_key)) {
        for (const auto& [key, node] : *table) {
            if (const auto value = node.value<std::string>()) {
                result[to_lower(trim(key.str()))] = *value;
                continue;
            }

            throw std::invalid_argument(
                fmt::format("config '{}.{}' must be a string.", table_key, key.str())
            );
        }
    }
    return result;
}

std::vector<std::string> collect_unknown_key_warnings(const toml::table& table) {
    static constexpr std::array<std::string_view, 9> kKnownKeys = {
        "default_dataset",
        "default_output_dir",
        "timeout",
        "chunk_size",
        "retry_count",
        "overwrite",
        "resume",
        "user_agent",
        "provider_base_urls",
    };
    static constexpr std::array<std::string_view, 1> kKnownLegacyKeys = {
        "dataset_base_urls",
    };

    std::vector<std::string> warnings;
    for (const auto& [key, _] : table) {
        const auto name = std::string_view(key.str());
        const bool known =
            std::find(kKnownKeys.begin(), kKnownKeys.end(), name) != kKnownKeys.end()
            || std::find(kKnownLegacyKeys.begin(), kKnownLegacyKeys.end(), name)
                != kKnownLegacyKeys.end();
        if (!known) {
            warnings.push_back(fmt::format("Ignoring unknown config key: {}", key.str()));
        }
    }
    return warnings;
}

}  // namespace

std::filesystem::path default_config_path() {
    return default_config_dir() / "config.toml";
}

std::filesystem::path default_output_dir() {
    return default_data_dir();
}

std::map<std::string, std::string> default_provider_base_urls() {
    return builtin_provider_base_urls();
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
        throw std::invalid_argument("default_dataset must not be empty.");
    }
    if (!std::isfinite(timeout) || timeout <= 0 || timeout > kMaxTimeoutSeconds) {
        throw std::invalid_argument(
            fmt::format(
                "timeout must be finite, greater than 0, and at most {:.0f} seconds.",
                kMaxTimeoutSeconds
            )
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
    if (user_agent.empty()) {
        user_agent = fmt::format("oceandl/{}", kVersion);
    }

    auto normalize_url_map = [](const std::map<std::string, std::string>& source, const char* field_name) {
        std::map<std::string, std::string> normalized;
        for (const auto& [key, value] : source) {
            const auto normalized_key = to_lower(trim(key));
            const auto normalized_value = trim_trailing_slash(value);
            if (normalized_key.empty() || normalized_value.empty()) {
                throw std::invalid_argument(
                    std::string(field_name) + " must not contain empty keys or URLs."
                );
            }
            if (!is_http_url(normalized_value)) {
                throw std::invalid_argument(
                    std::string(field_name) + "." + normalized_key
                    + " must be a valid http/https URL."
                );
            }
            normalized[normalized_key] = normalized_value;
        }
        return normalized;
    };

    provider_base_urls = normalize_url_map(provider_base_urls, "provider_base_urls");
    dataset_base_urls = normalize_url_map(dataset_base_urls, "dataset_base_urls");
}

ConfigLoadResult load_config_with_diagnostics(const std::filesystem::path& path) {
    ConfigLoadResult result;
    result.config = default_app_config();
    const auto config_path = expand_user(path);
    result.path = config_path;

    if (!std::filesystem::exists(config_path)) {
        return result;
    }

    try {
        const auto table = toml::parse_file(config_path.string());
        result.loaded_from_file = true;
        result.warnings = collect_unknown_key_warnings(table);

        if (const auto value = parse_scalar_or_throw<std::string>(table, "default_dataset", "string")) {
            result.config.default_dataset = *value;
        }
        if (const auto value =
                parse_scalar_or_throw<std::string>(table, "default_output_dir", "string")) {
            result.config.default_output_dir = *value;
        }
        if (const auto value = parse_scalar_or_throw<double>(table, "timeout", "number")) {
            result.config.timeout = *value;
        }
        if (const auto value = parse_scalar_or_throw<std::int64_t>(table, "chunk_size", "integer")) {
            if (*value < 0) {
                throw std::invalid_argument("config 'chunk_size' must not be negative.");
            }
            result.config.chunk_size = static_cast<std::uint64_t>(*value);
        }
        if (const auto value = parse_scalar_or_throw<std::int64_t>(table, "retry_count", "integer")) {
            if (*value < std::numeric_limits<int>::min() || *value > std::numeric_limits<int>::max()) {
                throw std::invalid_argument(
                    "config 'retry_count' is outside the supported integer range."
                );
            }
            result.config.retry_count = static_cast<int>(*value);
        }
        if (const auto value = parse_scalar_or_throw<bool>(table, "overwrite", "boolean")) {
            result.config.overwrite = *value;
        }
        if (const auto value = parse_scalar_or_throw<bool>(table, "resume", "boolean")) {
            result.config.resume = *value;
        }
        if (const auto value = parse_scalar_or_throw<std::string>(table, "user_agent", "string")) {
            result.config.user_agent = *value;
        }

        for (const auto& [key, value] : parse_string_map_or_throw(table, "provider_base_urls")) {
            result.config.provider_base_urls[key] = value;
        }
        for (const auto& [key, value] : parse_string_map_or_throw(table, "dataset_base_urls")) {
            result.config.dataset_base_urls[key] = value;
        }
    } catch (const toml::parse_error& error) {
        throw std::invalid_argument(
            fmt::format("failed to parse config {}: {}", config_path.string(), error.description())
        );
    }

    result.config.normalize_and_validate();
    return result;
}

AppConfig load_config(const std::filesystem::path& path) {
    return load_config_with_diagnostics(path).config;
}

}  // namespace oceandl
