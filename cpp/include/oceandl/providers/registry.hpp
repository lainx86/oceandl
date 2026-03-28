#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "oceandl/models.hpp"
#include "oceandl/providers/base.hpp"

namespace oceandl {

class ProviderRegistry {
  public:
    explicit ProviderRegistry(std::vector<std::shared_ptr<DatasetProvider>> providers);

    std::vector<std::shared_ptr<DatasetProvider>> list() const;
    std::vector<ProviderInfo> list_info() const;
    std::shared_ptr<DatasetProvider> get(const std::string& provider_key) const;
    std::shared_ptr<DatasetProvider> get_for_dataset(const DatasetInfo& dataset) const;

  private:
    std::map<std::string, std::shared_ptr<DatasetProvider>> providers_;
};

ProviderRegistry build_default_provider_registry();

}  // namespace oceandl
