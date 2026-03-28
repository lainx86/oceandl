#include "oceandl/providers/psl.hpp"

namespace oceandl {

ProviderInfo PSLProvider::provider_info() const {
    return {
        .id = "psl",
        .name = "NOAA PSL",
        .description = "NOAA Physical Sciences Laboratory static dataset downloads.",
        .transport = "http",
    };
}

std::vector<RemoteDownloadTarget> PSLProvider::build_targets(
    const DatasetInfo& dataset,
    const DownloadRequest& request
) const {
    validate_dataset(dataset);
    dataset.validate_requested_years(request.start_year, request.end_year);

    std::vector<RemoteDownloadTarget> targets;

    if (!dataset.requires_years()) {
        const auto file_name = dataset.file_name();
        targets.push_back(
            {
                .dataset_id = dataset.id,
                .provider_key = provider_info().id,
                .year = std::nullopt,
                .file_name = file_name,
                .url = dataset.base_url + "/" + file_name,
            }
        );
        return targets;
    }

    for (const int year : request.years()) {
        const auto file_name = dataset.file_name(year);
        targets.push_back(
            {
                .dataset_id = dataset.id,
                .provider_key = provider_info().id,
                .year = year,
                .file_name = file_name,
                .url = dataset.base_url + "/" + file_name,
            }
        );
    }

    return targets;
}

}  // namespace oceandl
