#include "oceandl/utils.hpp"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <cstdlib>
#include <optional>
#include <sstream>

#include <fmt/format.h>

namespace oceandl {

namespace {

std::optional<std::filesystem::path> home_directory_path() {
    const auto getenv_path = [](const char* name) -> std::optional<std::filesystem::path> {
        const char* value = std::getenv(name);
        if (value == nullptr || std::string_view(value).empty()) {
            return std::nullopt;
        }
        return std::filesystem::path(value);
    };

    if (const auto home = getenv_path("HOME")) {
        return home;
    }

#ifdef _WIN32
    if (const auto user_profile = getenv_path("USERPROFILE")) {
        return user_profile;
    }

    const auto home_drive = getenv_path("HOMEDRIVE");
    const auto home_path = getenv_path("HOMEPATH");
    if (home_drive.has_value() && home_path.has_value()) {
        return *home_drive / *home_path;
    }
#endif

    return std::nullopt;
}

}  // namespace

std::string trim(std::string_view value) {
    auto begin = value.find_first_not_of(" \t\r\n");
    if (begin == std::string_view::npos) {
        return {};
    }
    auto end = value.find_last_not_of(" \t\r\n");
    return std::string(value.substr(begin, end - begin + 1));
}

std::string to_lower(std::string_view value) {
    std::string result(value);
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return result;
}

std::string trim_trailing_slash(std::string_view value) {
    std::string result = trim(value);
    while (!result.empty() && result.back() == '/') {
        result.pop_back();
    }
    return result;
}

std::filesystem::path expand_user(const std::filesystem::path& path) {
    const auto raw = path.string();
    if (raw.empty() || raw[0] != '~') {
        return path;
    }

    const auto home = home_directory_path();
    if (!home.has_value()) {
        return path;
    }

    if (raw == "~") {
        return *home;
    }
    if (raw.rfind("~/", 0) == 0) {
        return *home / raw.substr(2);
#ifdef _WIN32
    }
    if (raw.rfind("~\\", 0) == 0) {
        return *home / raw.substr(2);
#endif
    }
    return path;
}

void ensure_directory(const std::filesystem::path& path) {
    std::filesystem::create_directories(path);
}

void safe_remove(const std::filesystem::path& path) {
    std::error_code error;
    std::filesystem::remove(path, error);
}

void safe_replace_file(const std::filesystem::path& source, const std::filesystem::path& destination) {
    if (!std::filesystem::exists(destination)) {
        std::filesystem::rename(source, destination);
        return;
    }

    const auto backup =
        destination.parent_path() / (destination.filename().string() + ".replace-backup");
    safe_remove(backup);
    std::filesystem::rename(destination, backup);

    try {
        std::filesystem::rename(source, destination);
    } catch (...) {
        std::error_code restore_error;
        if (!std::filesystem::exists(destination)) {
            std::filesystem::rename(backup, destination, restore_error);
        }
        if (!restore_error) {
            safe_remove(backup);
        }
        throw;
    }

    safe_remove(backup);
}

bool is_http_url(std::string_view value) {
    const auto normalized = to_lower(trim(value));
    if (normalized.size() <= std::string_view("http://").size()) {
        return false;
    }

    const bool has_supported_scheme =
        normalized.rfind("http://", 0) == 0 || normalized.rfind("https://", 0) == 0;
    if (!has_supported_scheme) {
        return false;
    }

    return normalized.find_first_of(" \t\r\n") == std::string::npos;
}

std::string format_bytes(std::uint64_t size_in_bytes) {
    static constexpr const char* kUnits[] = {"B", "KB", "MB", "GB", "TB"};
    double size = static_cast<double>(size_in_bytes);

    for (const char* unit : kUnits) {
        if (size < 1024.0 || std::string_view(unit) == "TB") {
            if (std::string_view(unit) == "B") {
                return fmt::format("{:.0f} {}", size, unit);
            }
            return fmt::format("{:.2f} {}", size, unit);
        }
        size /= 1024.0;
    }

    return fmt::format("{} B", size_in_bytes);
}

std::optional<std::uint64_t> parse_optional_uint(std::string_view value) {
    const auto stripped = trim(value);
    if (stripped.empty()) {
        return std::nullopt;
    }

    std::uint64_t parsed = 0;
    const auto* begin = stripped.data();
    const auto* end = begin + stripped.size();
    const auto [pointer, error] = std::from_chars(begin, end, parsed);
    if (error != std::errc() || pointer != end) {
        return std::nullopt;
    }
    return parsed;
}

std::string join_strings(const std::vector<std::string>& values, std::string_view separator) {
    std::ostringstream stream;
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (index != 0) {
            stream << separator;
        }
        stream << values[index];
    }
    return stream.str();
}

}  // namespace oceandl
