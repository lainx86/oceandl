#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace oceandl {

std::string trim(std::string_view value);
std::string to_lower(std::string_view value);
std::string trim_trailing_slash(std::string_view value);
std::filesystem::path expand_user(const std::filesystem::path& path);
void ensure_directory(const std::filesystem::path& path);
void safe_remove(const std::filesystem::path& path);
void safe_replace_file(const std::filesystem::path& source, const std::filesystem::path& destination);
std::string format_bytes(std::uint64_t size_in_bytes);
std::optional<std::uint64_t> parse_optional_uint(std::string_view value);
std::string join_strings(const std::vector<std::string>& values, std::string_view separator);

}  // namespace oceandl
