#pragma once

#include <exception>
#include <optional>
#include <utility>

#include "download_resume.hpp"
#include "oceandl/http_client.hpp"
#include "oceandl/models.hpp"
#include "oceandl/providers/base.hpp"
#include "oceandl/reporter.hpp"

namespace oceandl {

class TargetDownloadExecutor {
  public:
    TargetDownloadExecutor(
        const DownloadRequest& request,
        const DatasetInfo& dataset,
        const DatasetProvider& provider,
        IHttpClient& http_client,
        const Reporter& reporter
    );

    DownloadResult run(const DownloadTarget& target) const;

  private:
    const DownloadRequest* request_;
    const DatasetInfo* dataset_;
    const DatasetProvider* provider_;
    IHttpClient* http_client_;
    const Reporter* reporter_;

    std::pair<std::uint64_t, bool> stream_download(
        const DownloadTarget& target,
        const RemoteFileMetadata& remote_metadata,
        const TransferPlan& transfer_plan
    ) const;

    std::optional<DownloadResult> maybe_skip_existing_output(
        const DownloadTarget& target,
        const RemoteFileMetadata& remote_metadata
    ) const;

    TransferPlan prepare_transfer_plan(
        const DownloadTarget& target,
        const RemoteFileMetadata& remote_metadata
    ) const;

    bool is_retryable(const std::exception& error) const;
    bool should_remove_partial(const std::exception& error) const;
    DownloadResult make_failure_result(
        const DownloadTarget& target,
        int attempt,
        const std::exception& error
    ) const;
    std::string format_error(const std::exception& error) const;
};

}  // namespace oceandl
