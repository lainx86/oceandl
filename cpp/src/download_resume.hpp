#pragma once

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

#include "oceandl/http_client.hpp"
#include "oceandl/models.hpp"

namespace oceandl {

class DownloadIntegrityError : public std::runtime_error {
  public:
    using std::runtime_error::runtime_error;
};

class DownloadPayloadError : public std::runtime_error {
  public:
    using std::runtime_error::runtime_error;
};

struct TransferPlan {
    std::uint64_t start_byte = 0;
    bool resumed = false;
    bool promote_existing_part = false;
    std::optional<std::string> if_range_value;
};

struct PartialTransferMetadata {
    std::optional<std::uint64_t> content_length;
    std::optional<std::string> etag;
    std::optional<std::string> last_modified;
};

bool has_remote_identity(const RemoteFileMetadata& remote_metadata);
std::optional<std::string> choose_if_range_value(const RemoteFileMetadata& remote_metadata);
std::optional<std::uint64_t> parse_content_range_total(std::string_view value);
std::optional<std::uint64_t> expected_total_size(
    const HttpResponse& response,
    const RemoteFileMetadata& remote_metadata,
    std::uint64_t start_byte
);
void remove_partial_state(const DownloadTarget& target);
void write_partial_metadata(
    const std::filesystem::path& path,
    const RemoteFileMetadata& remote_metadata
);
std::optional<PartialTransferMetadata> load_partial_metadata(const std::filesystem::path& path);
bool partial_metadata_matches_remote(
    const PartialTransferMetadata& partial_metadata,
    const RemoteFileMetadata& remote_metadata
);

}  // namespace oceandl
