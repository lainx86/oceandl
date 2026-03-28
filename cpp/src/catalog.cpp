#include "oceandl/catalog.hpp"

#include <stdexcept>

#include "builtin_datasets.hpp"
#include "oceandl/utils.hpp"

namespace oceandl {

DatasetRegistry::DatasetRegistry(std::vector<DatasetInfo> datasets, std::string default_dataset)
    : default_dataset_(to_lower(trim(default_dataset))) {
    for (auto& dataset : datasets) {
        dataset.normalize_and_validate();
        datasets_[dataset.id] = dataset;
    }

    if (datasets_.find(default_dataset_) == datasets_.end()) {
        std::vector<std::string> supported;
        for (const auto& [id, _] : datasets_) {
            supported.push_back(id);
        }
        throw std::invalid_argument(
            "default_dataset '" + default_dataset + "' tidak ditemukan di registry. Dataset tersedia: "
            + join_strings(supported, ", ") + "."
        );
    }
}

const std::string& DatasetRegistry::default_dataset() const {
    return default_dataset_;
}

std::vector<DatasetInfo> DatasetRegistry::list() const {
    std::vector<DatasetInfo> result;
    for (const auto& [_, dataset] : datasets_) {
        result.push_back(dataset);
    }
    return result;
}

const DatasetInfo& DatasetRegistry::get(const std::string& dataset_id) const {
    const auto normalized = to_lower(trim(dataset_id));
    const auto iterator = datasets_.find(normalized);
    if (iterator != datasets_.end()) {
        return iterator->second;
    }

    std::vector<std::string> supported;
    for (const auto& [id, _] : datasets_) {
        supported.push_back(id);
    }

    throw std::invalid_argument(
        "Dataset '" + dataset_id + "' belum didukung. Dataset tersedia: "
        + join_strings(supported, ", ") + "."
    );
}

DatasetRegistry build_default_dataset_registry(const AppConfig& config) {
    std::vector<DatasetInfo> datasets;
    datasets.reserve(builtin_dataset_specs().size());
    for (const auto& spec : builtin_dataset_specs()) {
        datasets.push_back(
            {
                .id = std::string(spec.id),
                .display_name = std::string(spec.display_name),
                .description = std::string(spec.description),
                .provider_key = std::string(spec.provider_key),
                .base_url = config.dataset_base_urls.at(std::string(spec.id)),
                .filename_pattern = std::string(spec.filename_pattern),
                .file_mode = spec.file_mode,
                .payload_format = spec.payload_format,
                .start_year = spec.start_year,
                .end_year = spec.end_year,
            }
        );
    }

    return DatasetRegistry(std::move(datasets), config.default_dataset);
}

}  // namespace oceandl
