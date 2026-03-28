#pragma once

#include <filesystem>
#include <optional>
#include <string>

#include "oceandl/models.hpp"

namespace oceandl {

struct FileValidationResult {
    bool valid = false;
    std::optional<std::uintmax_t> actual_size;
    std::optional<std::uintmax_t> expected_size;
    std::string reason;
};

FileValidationResult validate_netcdf_file(
    const std::filesystem::path& path,
    std::optional<std::uintmax_t> expected_size = std::nullopt
);
FileValidationResult validate_dataset_file(
    const DatasetInfo& dataset,
    const std::filesystem::path& path,
    std::optional<std::uintmax_t> expected_size = std::nullopt
);

}  // namespace oceandl
