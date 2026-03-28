#pragma once

#include <optional>
#include <string>
#include <vector>

#include "oceandl/http_client.hpp"
#include "oceandl/models.hpp"

namespace oceandl {

class DatasetProvider {
  public:
    virtual ~DatasetProvider() = default;

    virtual ProviderInfo provider_info() const = 0;

    bool supports_dataset(const DatasetInfo& dataset) const;
    void validate_dataset(const DatasetInfo& dataset) const;

    virtual std::vector<RemoteDownloadTarget> build_targets(
        const DatasetInfo& dataset,
        const DownloadRequest& request
    ) const = 0;

    virtual std::vector<std::string> build_download_headers(
        const RemoteDownloadTarget& target,
        std::uint64_t start_byte = 0,
        std::optional<std::string> if_range_value = std::nullopt
    ) const;

    virtual RemoteFileMetadata fetch_remote_metadata(
        IHttpClient& client,
        const RemoteDownloadTarget& target,
        double timeout_seconds
    ) const;
};

}  // namespace oceandl
