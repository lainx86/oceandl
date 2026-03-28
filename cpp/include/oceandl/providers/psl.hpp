#pragma once

#include "oceandl/providers/base.hpp"

namespace oceandl {

class PSLProvider final : public DatasetProvider {
  public:
    ProviderInfo provider_info() const override;

    std::vector<RemoteDownloadTarget> build_targets(
        const DatasetInfo& dataset,
        const DownloadRequest& request
    ) const override;
};

}  // namespace oceandl
