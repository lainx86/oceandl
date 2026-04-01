#include "download_resume.hpp"

#include <filesystem>
#include <fstream>
#include <iomanip>

#include "oceandl/utils.hpp"

namespace oceandl {

namespace {

std::optional<std::string> trim_to_optional_string(const std::string& value) {
    const auto trimmed = trim(value);
    if (trimmed.empty()) {
        return std::nullopt;
    }
    return trimmed;
}

}  // namespace

bool has_remote_identity(const RemoteFileMetadata& remote_metadata) {
    return (remote_metadata.etag.has_value() && !trim(*remote_metadata.etag).empty())
        || (
            remote_metadata.last_modified.has_value()
            && !trim(*remote_metadata.last_modified).empty()
        );
}

std::optional<std::string> choose_if_range_value(const RemoteFileMetadata& remote_metadata) {
    if (remote_metadata.etag.has_value() && !remote_metadata.etag->empty()) {
        return remote_metadata.etag;
    }
    if (remote_metadata.last_modified.has_value() && !remote_metadata.last_modified->empty()) {
        return remote_metadata.last_modified;
    }
    return std::nullopt;
}

std::optional<std::uint64_t> parse_content_range_total(std::string_view value) {
    const auto slash = value.rfind('/');
    if (slash == std::string_view::npos) {
        return std::nullopt;
    }
    return parse_optional_uint(value.substr(slash + 1));
}

std::optional<std::uint64_t> expected_total_size(
    const HttpResponse& response,
    const RemoteFileMetadata& remote_metadata,
    std::uint64_t start_byte
) {
    if (const auto iterator = response.headers.find("content-range"); iterator != response.headers.end()) {
        if (const auto parsed = parse_content_range_total(iterator->second)) {
            return parsed;
        }
    }

    if (const auto iterator = response.headers.find("content-length"); iterator != response.headers.end()) {
        if (const auto parsed = parse_optional_uint(iterator->second)) {
            if (response.status_code == 206) {
                return start_byte + *parsed;
            }
            return parsed;
        }
    }

    return remote_metadata.content_length;
}

void remove_partial_state(const DownloadTarget& target) {
    safe_remove(target.temp_path());
    safe_remove(target.temp_metadata_path());
}

void write_partial_metadata(
    const std::filesystem::path& path,
    const RemoteFileMetadata& remote_metadata
) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        throw std::runtime_error("failed to write partial file metadata.");
    }

    output << "content_length ";
    if (remote_metadata.content_length.has_value()) {
        output << *remote_metadata.content_length;
    } else {
        output << "-";
    }
    output << '\n';
    output << "etag " << std::quoted(remote_metadata.etag.value_or("")) << '\n';
    output << "last_modified " << std::quoted(remote_metadata.last_modified.value_or("")) << '\n';

    if (!output) {
        throw std::runtime_error("failed to flush partial file metadata.");
    }
}

std::optional<PartialTransferMetadata> load_partial_metadata(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) {
        return std::nullopt;
    }

    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return std::nullopt;
    }

    PartialTransferMetadata metadata;
    bool saw_content_length = false;
    std::string key;
    while (input >> key) {
        if (key == "content_length") {
            saw_content_length = true;
            std::string value;
            if (!(input >> value)) {
                return std::nullopt;
            }
            if (value != "-") {
                const auto parsed = parse_optional_uint(value);
                if (!parsed.has_value()) {
                    return std::nullopt;
                }
                metadata.content_length = *parsed;
            }
            continue;
        }

        std::string value;
        if (!(input >> std::quoted(value))) {
            return std::nullopt;
        }

        if (key == "etag") {
            metadata.etag = trim_to_optional_string(value);
            continue;
        }
        if (key == "last_modified") {
            metadata.last_modified = trim_to_optional_string(value);
            continue;
        }
        return std::nullopt;
    }

    if (!input.eof() || !saw_content_length) {
        return std::nullopt;
    }

    return metadata;
}

bool partial_metadata_matches_remote(
    const PartialTransferMetadata& partial_metadata,
    const RemoteFileMetadata& remote_metadata
) {
    if (remote_metadata.content_length.has_value()) {
        if (!partial_metadata.content_length.has_value()) {
            return false;
        }
        if (*partial_metadata.content_length != *remote_metadata.content_length) {
            return false;
        }
    }

    if (remote_metadata.etag.has_value() && partial_metadata.etag != remote_metadata.etag) {
        return false;
    }
    if (
        remote_metadata.last_modified.has_value()
        && partial_metadata.last_modified != remote_metadata.last_modified
    ) {
        return false;
    }

    return has_remote_identity(remote_metadata);
}

}  // namespace oceandl
