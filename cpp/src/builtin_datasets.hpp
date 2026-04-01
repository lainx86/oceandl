#pragma once

#include <map>
#include <optional>
#include <string_view>
#include <vector>

#include "oceandl/models.hpp"

namespace oceandl {

struct BuiltinDatasetSpec {
    std::string_view id;
    std::string_view display_name;
    std::string_view description;
    std::string_view provider_key;
    std::string_view default_path;
    std::string_view filename_pattern;
    FileMode file_mode = FileMode::PerYear;
    DatasetPayloadFormat payload_format = DatasetPayloadFormat::Netcdf;
    std::optional<int> start_year;
    std::optional<int> end_year;
};

const std::vector<BuiltinDatasetSpec>& builtin_dataset_specs();
std::map<std::string, std::string> builtin_provider_base_urls();
std::map<std::string, std::string> builtin_dataset_base_urls();

}  // namespace oceandl
