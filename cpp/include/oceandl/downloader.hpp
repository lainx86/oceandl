#pragma once

#include <cstdint>
#include <exception>
#include <optional>
#include <utility>

#include "oceandl/http_client.hpp"
#include "oceandl/models.hpp"
#include "oceandl/providers/base.hpp"
#include "oceandl/reporter.hpp"

namespace oceandl {

struct TransferPlan;

class Downloader {
  public:
    Downloader(IHttpClient& http_client, const Reporter& reporter);

    DownloadSummary download(
        const DownloadRequest& request,
        const DatasetInfo& dataset,
        const DatasetProvider& provider
    ) const;

  private:
    IHttpClient* http_client_;
    const Reporter* reporter_;

    DownloadResult download_target(
        const DownloadRequest& request,
        const DatasetInfo& dataset,
        const DatasetProvider& provider,
        const DownloadTarget& target
    ) const;

    std::pair<std::uint64_t, bool> stream_download(
        const DownloadRequest& request,
        const DatasetInfo& dataset,
        const DatasetProvider& provider,
        const DownloadTarget& target,
        const RemoteFileMetadata& remote_metadata,
        const TransferPlan& transfer_plan
    ) const;

    std::optional<DownloadResult> maybe_skip_existing_output(
        const DownloadRequest& request,
        const DatasetInfo& dataset,
        const DownloadTarget& target,
        const RemoteFileMetadata& remote_metadata
    ) const;

    TransferPlan prepare_transfer_plan(
        const DownloadRequest& request,
        const DatasetInfo& dataset,
        const DownloadTarget& target,
        const RemoteFileMetadata& remote_metadata
    ) const;

    bool is_retryable(const std::exception& error) const;
    bool should_remove_partial(const std::exception& error) const;
    std::string format_error(const std::exception& error) const;
};

}  // namespace oceandl
