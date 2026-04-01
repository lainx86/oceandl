#pragma once

#include <cstdint>
#include "oceandl/http_client.hpp"
#include "oceandl/models.hpp"
#include "oceandl/providers/base.hpp"
#include "oceandl/reporter.hpp"

namespace oceandl {

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
};

}  // namespace oceandl
