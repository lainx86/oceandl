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

std::optional<std::string> content_range_header(const HeaderMap& headers) {
    if (const auto iterator = headers.find("content-range"); iterator != headers.end()) {
        return iterator->second;
    }
    return std::nullopt;
}

std::optional<std::uint64_t> content_length_header(const HeaderMap& headers) {
    if (const auto iterator = headers.find("content-length"); iterator != headers.end()) {
        return parse_optional_uint(iterator->second);
    }
    return std::nullopt;
}

bool has_content_length_header(const HeaderMap& headers) {
    return headers.find("content-length") != headers.end();
}

bool is_unsatisfied_content_range(std::string_view value) {
    const auto text = trim(value);
    const auto separator = text.find_first_of(" \t");
    if (separator == std::string::npos) {
        return false;
    }

    const auto unit = to_lower(trim(std::string_view(text).substr(0, separator)));
    if (unit != "bytes") {
        return false;
    }

    const auto range_spec = trim(std::string_view(text).substr(separator + 1));
    return range_spec.rfind("*/", 0) == 0;
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

std::optional<ContentRange> parse_content_range(std::string_view value) {
    const auto text = trim(value);
    const auto separator = text.find_first_of(" \t");
    if (separator == std::string::npos) {
        return std::nullopt;
    }

    const auto unit = to_lower(trim(std::string_view(text).substr(0, separator)));
    if (unit != "bytes") {
        return std::nullopt;
    }

    const auto range_spec = trim(std::string_view(text).substr(separator + 1));
    const auto slash = range_spec.find('/');
    if (slash == std::string::npos || range_spec.find('/', slash + 1) != std::string::npos) {
        return std::nullopt;
    }

    const auto byte_range = trim(std::string_view(range_spec).substr(0, slash));
    const auto total = parse_optional_uint(std::string_view(range_spec).substr(slash + 1));
    const auto dash = byte_range.find('-');
    if (
        !total.has_value()
        || byte_range.empty()
        || dash == std::string::npos
        || byte_range.find('-', dash + 1) != std::string::npos
    ) {
        return std::nullopt;
    }

    const auto start = parse_optional_uint(std::string_view(byte_range).substr(0, dash));
    const auto end = parse_optional_uint(std::string_view(byte_range).substr(dash + 1));
    if (!start.has_value() || !end.has_value()) {
        return std::nullopt;
    }

    return ContentRange{
        .start_byte = *start,
        .end_byte = *end,
        .total_size = *total,
    };
}

std::optional<std::uint64_t> parse_content_range_total(std::string_view value) {
    if (const auto content_range = parse_content_range(value)) {
        return content_range->total_size;
    }

    if (!is_unsatisfied_content_range(value)) {
        return std::nullopt;
    }

    const auto text = trim(value);
    const auto separator = text.find_first_of(" \t");
    const auto range_spec = trim(std::string_view(text).substr(separator + 1));
    return parse_optional_uint(std::string_view(range_spec).substr(2));
}

ResumeRangeValidation validate_resume_content_range(
    const HttpResponse& response,
    std::optional<std::uint64_t> remote_content_length,
    std::uint64_t requested_start_byte
) {
    const auto header = content_range_header(response.headers);
    if (!header.has_value()) {
        return {
            .valid = false,
            .content_range = std::nullopt,
            .reason = "HTTP 206 resume response is missing Content-Range.",
        };
    }

    if (is_unsatisfied_content_range(*header)) {
        return {
            .valid = false,
            .content_range = std::nullopt,
            .reason = "HTTP 206 resume response used unsatisfied Content-Range form.",
        };
    }

    const auto content_range = parse_content_range(*header);
    if (!content_range.has_value()) {
        return {
            .valid = false,
            .content_range = std::nullopt,
            .reason = "HTTP 206 resume response has a malformed Content-Range.",
        };
    }

    if (content_range->start_byte != requested_start_byte) {
        return {
            .valid = false,
            .content_range = content_range,
            .reason = "HTTP 206 Content-Range starts at byte "
                + std::to_string(content_range->start_byte)
                + " but resume requested byte " + std::to_string(requested_start_byte) + ".",
        };
    }

    if (content_range->end_byte < content_range->start_byte) {
        return {
            .valid = false,
            .content_range = content_range,
            .reason = "HTTP 206 Content-Range end is before its start.",
        };
    }

    if (content_range->total_size == 0) {
        return {
            .valid = false,
            .content_range = content_range,
            .reason = "HTTP 206 Content-Range total size is zero.",
        };
    }

    if (content_range->end_byte >= content_range->total_size) {
        return {
            .valid = false,
            .content_range = content_range,
            .reason = "HTTP 206 Content-Range extends beyond the reported total size.",
        };
    }

    if (remote_content_length.has_value() && *remote_content_length != content_range->total_size) {
        return {
            .valid = false,
            .content_range = content_range,
            .reason = "HTTP 206 Content-Range total size "
                + std::to_string(content_range->total_size)
                + " does not match remote metadata size "
                + std::to_string(*remote_content_length) + ".",
        };
    }

    if (has_content_length_header(response.headers)) {
        const auto content_length = content_length_header(response.headers);
        if (!content_length.has_value()) {
            return {
                .valid = false,
                .content_range = content_range,
                .reason = "HTTP 206 response has an invalid Content-Length.",
            };
        }

        const auto expected_length = content_range->end_byte - content_range->start_byte + 1;
        if (*content_length != expected_length) {
            return {
                .valid = false,
                .content_range = content_range,
                .reason = "HTTP 206 Content-Length "
                    + std::to_string(*content_length)
                    + " does not match Content-Range span "
                    + std::to_string(expected_length) + ".",
            };
        }
    }

    return {
        .valid = true,
        .content_range = content_range,
        .reason = {},
    };
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
