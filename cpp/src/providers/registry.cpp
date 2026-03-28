#include "oceandl/providers/registry.hpp"

#include <stdexcept>

#include "oceandl/providers/psl.hpp"
#include "oceandl/utils.hpp"

namespace oceandl {

ProviderRegistry::ProviderRegistry(std::vector<std::shared_ptr<DatasetProvider>> providers) {
    for (auto& provider : providers) {
        providers_[provider->provider_info().id] = provider;
    }
}

std::vector<std::shared_ptr<DatasetProvider>> ProviderRegistry::list() const {
    std::vector<std::shared_ptr<DatasetProvider>> result;
    for (const auto& [_, provider] : providers_) {
        result.push_back(provider);
    }
    return result;
}

std::vector<ProviderInfo> ProviderRegistry::list_info() const {
    std::vector<ProviderInfo> result;
    for (const auto& provider : list()) {
        result.push_back(provider->provider_info());
    }
    return result;
}

std::shared_ptr<DatasetProvider> ProviderRegistry::get(const std::string& provider_key) const {
    const auto normalized = to_lower(trim(provider_key));
    const auto iterator = providers_.find(normalized);
    if (iterator != providers_.end()) {
        return iterator->second;
    }

    std::vector<std::string> supported;
    for (const auto& [id, _] : providers_) {
        supported.push_back(id);
    }
    throw std::invalid_argument(
        "Provider '" + provider_key + "' belum didukung. Provider tersedia: "
        + join_strings(supported, ", ") + "."
    );
}

std::shared_ptr<DatasetProvider> ProviderRegistry::get_for_dataset(const DatasetInfo& dataset) const {
    return get(dataset.provider_key);
}

ProviderRegistry build_default_provider_registry() {
    return ProviderRegistry({std::make_shared<PSLProvider>()});
}

}  // namespace oceandl
