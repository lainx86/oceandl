#include "oceandl/validation.hpp"

#include <array>
#include <fstream>

namespace oceandl {

namespace {

constexpr std::array<unsigned char, 8> kNetcdfHdf5Magic = {
    0x89, 'H', 'D', 'F', '\r', '\n', 0x1a, '\n'
};

}  // namespace

FileValidationResult validate_netcdf_file(
    const std::filesystem::path& path,
    std::optional<std::uintmax_t> expected_size
) {
    if (!std::filesystem::exists(path) || !std::filesystem::is_regular_file(path)) {
        return {
            .valid = false,
            .actual_size = std::nullopt,
            .expected_size = expected_size,
            .reason = "file not found."
        };
    }

    std::error_code size_error;
    const auto actual_size = std::filesystem::file_size(path, size_error);
    if (size_error) {
        return {
            .valid = false,
            .actual_size = std::nullopt,
            .expected_size = expected_size,
            .reason = size_error.message()
        };
    }

    if (expected_size.has_value() && actual_size != *expected_size) {
        return {
            .valid = false,
            .actual_size = actual_size,
            .expected_size = expected_size,
            .reason = "file size does not match (" + std::to_string(actual_size) + " != "
                + std::to_string(*expected_size) + " bytes)."
        };
    }

    if (actual_size < 4) {
        return {
            .valid = false,
            .actual_size = actual_size,
            .expected_size = expected_size,
            .reason = "file is too small to be a NetCDF payload."
        };
    }

    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return {
            .valid = false,
            .actual_size = actual_size,
            .expected_size = expected_size,
            .reason = "failed to open file."
        };
    }

    std::array<unsigned char, 8> header{};
    input.read(reinterpret_cast<char*>(header.data()), static_cast<std::streamsize>(header.size()));
    if (!input && input.gcount() < 4) {
        return {
            .valid = false,
            .actual_size = actual_size,
            .expected_size = expected_size,
            .reason = "failed to read file header."
        };
    }

    const bool classic_netcdf = header[0] == 'C' && header[1] == 'D' && header[2] == 'F'
        && (header[3] == 1 || header[3] == 2 || header[3] == 5);
    const bool hdf5 = header == kNetcdfHdf5Magic;
    if (!classic_netcdf && !hdf5) {
        return {
            .valid = false,
            .actual_size = actual_size,
            .expected_size = expected_size,
            .reason = "file signature does not match NetCDF/HDF5."
        };
    }

    return {
        .valid = true,
        .actual_size = actual_size,
        .expected_size = expected_size,
        .reason = {}
    };
}

FileValidationResult validate_dataset_file(
    const DatasetInfo& dataset,
    const std::filesystem::path& path,
    std::optional<std::uintmax_t> expected_size
) {
    switch (dataset.payload_format) {
        case DatasetPayloadFormat::Netcdf:
            return validate_netcdf_file(path, expected_size);
    }

    return {
        .valid = false,
        .actual_size = std::nullopt,
        .expected_size = expected_size,
        .reason = "unrecognized dataset validation format."
    };
}

}  // namespace oceandl
