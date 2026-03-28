#pragma once

#include <map>
#include <string>
#include <vector>

#include "oceandl/config.hpp"
#include "oceandl/models.hpp"

namespace oceandl {

class DatasetRegistry {
  public:
    DatasetRegistry(std::vector<DatasetInfo> datasets, std::string default_dataset = "oisst");

    const std::string& default_dataset() const;
    std::vector<DatasetInfo> list() const;
    const DatasetInfo& get(const std::string& dataset_id) const;

  private:
    std::map<std::string, DatasetInfo> datasets_;
    std::string default_dataset_;
};

DatasetRegistry build_default_dataset_registry(const AppConfig& config);

}  // namespace oceandl
