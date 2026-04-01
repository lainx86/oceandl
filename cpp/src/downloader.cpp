#include "oceandl/downloader.hpp"

#include <vector>

#include "target_download_executor.hpp"
#include "oceandl/utils.hpp"

namespace oceandl {

Downloader::Downloader(IHttpClient& http_client, const Reporter& reporter)
    : http_client_(&http_client), reporter_(&reporter) {}

DownloadSummary Downloader::download(
    const DownloadRequest& request,
    const DatasetInfo& dataset,
    const DatasetProvider& provider
) const {
    ensure_directory(request.output_dir);

    std::vector<DownloadResult> results;
    const auto remote_targets = provider.build_targets(dataset, request);
    const auto targets = make_download_targets(request.output_dir, remote_targets);
    results.reserve(targets.size());

    TargetDownloadExecutor executor(
        request,
        dataset,
        provider,
        *http_client_,
        *reporter_
    );

    for (const auto& target : targets) {
        results.push_back(executor.run(target));
    }

    return {
        .request = request,
        .provider = provider.provider_info(),
        .dataset = dataset,
        .results = std::move(results),
    };
}

}  // namespace oceandl
