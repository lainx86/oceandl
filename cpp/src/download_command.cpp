#include "download_command.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <vector>

#include <fmt/format.h>

#include "cli_support.hpp"
#include "oceandl/downloader.hpp"
#include "oceandl/http_client.hpp"
#include "oceandl/utils.hpp"

namespace oceandl {

namespace {

bool is_true_flag(const std::string& token, const std::string& flag) {
    return token == flag;
}

bool is_false_flag(const std::string& token, const std::string& flag) {
    return token == "--no-" + flag.substr(2);
}

bool is_help_token(const std::string& token) {
    return token == "--help" || token == "-h";
}

using KeyValueRows = std::vector<std::pair<std::string, std::string>>;

std::size_t widest_label(const KeyValueRows& rows) {
    std::size_t width = 0;
    for (const auto& [label, value] : rows) {
        (void)value;
        width = std::max(width, label.size());
    }
    return width;
}

void print_key_value_rows(const KeyValueRows& rows, const Reporter& reporter) {
    const auto label_width = widest_label(rows);
    for (const auto& [label, value] : rows) {
        reporter.field(label, value, label_width, true);
    }
}

std::string yes_no(bool value) {
    return value ? "yes" : "no";
}

std::size_t resumed_count(const DownloadSummary& summary) {
    return static_cast<std::size_t>(
        std::count_if(summary.results.begin(), summary.results.end(), [](const auto& item) {
            return item.resumed;
        })
    );
}

std::uint64_t total_downloaded_bytes(const DownloadSummary& summary) {
    std::uint64_t total = 0;
    for (const auto& item : summary.results) {
        total += item.bytes_downloaded;
    }
    return total;
}

std::string take_value(
    const std::vector<std::string>& args,
    std::size_t& index,
    const std::string& option
) {
    if (index + 1 >= args.size()) {
        throw CliError("Missing value for " + option + ".", 2);
    }
    ++index;
    return args[index];
}

int parse_int_or_throw(const std::string& value, const std::string& option) {
    const auto trimmed_value = trim(value);
    try {
        std::size_t parsed_size = 0;
        const auto parsed = std::stoi(trimmed_value, &parsed_size);
        if (parsed_size != trimmed_value.size()) {
            throw std::invalid_argument("trailing characters");
        }
        return parsed;
    } catch (...) {
        throw CliError("Invalid value for " + option + ": " + value, 2);
    }
}

double parse_double_or_throw(const std::string& value, const std::string& option) {
    const auto trimmed_value = trim(value);
    try {
        std::size_t parsed_size = 0;
        const auto parsed = std::stod(trimmed_value, &parsed_size);
        if (parsed_size != trimmed_value.size()) {
            throw std::invalid_argument("trailing characters");
        }
        if (!std::isfinite(parsed)) {
            throw std::invalid_argument("non-finite");
        }
        return parsed;
    } catch (...) {
        throw CliError("Invalid value for " + option + ": " + value, 2);
    }
}

std::uint64_t parse_uint64_or_throw(const std::string& value, const std::string& option) {
    if (const auto parsed = parse_optional_uint(value)) {
        return *parsed;
    }
    throw CliError("Invalid value for " + option + ": " + value, 2);
}

void print_download_parameters(
    const DownloadRequest& request,
    const DatasetInfo& dataset,
    const DatasetProvider& provider,
    const Reporter& reporter
) {
    reporter.section("Download plan", dataset.display_name, true);
    print_key_value_rows(
        {
            {"dataset", fmt::format("{} ({})", dataset.id, dataset.display_name)},
            {"provider", provider.provider_info().name},
            {"years", dataset.requested_year_label(request.start_year, request.end_year)},
            {
                "total files",
                std::to_string(dataset.requested_file_count(request.start_year, request.end_year)),
            },
            {"output", (request.output_dir / dataset.id).string()},
            {"overwrite", yes_no(request.overwrite)},
            {"resume", yes_no(request.resume)},
            {"timeout", fmt::format("{:.1f} seconds", request.timeout)},
            {"chunk size", format_bytes(request.chunk_size)},
            {"retries", std::to_string(request.retry_count)},
            {"base URL", dataset.base_url},
        },
        reporter
    );
}

void print_download_summary(const DownloadSummary& summary, const Reporter& reporter) {
    reporter.section(
        "Download summary",
        fmt::format("dataset {} processed", summary.dataset.id),
        true
    );
    print_key_value_rows(
        {
            {"total targets", std::to_string(summary.total_requested())},
            {"downloaded", std::to_string(summary.downloaded_count())},
            {"skipped", std::to_string(summary.skipped_count())},
            {"failed", std::to_string(summary.failed_count())},
            {"resumed", std::to_string(resumed_count(summary))},
            {"bytes received", format_bytes(total_downloaded_bytes(summary))},
        },
        reporter
    );

    const auto failures = summary.failures();
    if (!failures.empty()) {
        reporter.blank_line(true);
        reporter.section("Failed targets", {}, true);
        for (const auto& failure : failures) {
            reporter.print(
                fmt::format("  {} -> {}", failure.target.file_name, failure.error_message),
                true
            );
        }
    }

    reporter.blank_line(true);
    if (summary.failed_count() == 0) {
        reporter.success("All targets completed without errors.");
    } else {
        reporter.warning(fmt::format("{} target(s) failed and should be checked.", summary.failed_count()));
    }
}

}  // namespace

DownloadCommandOptions parse_download_options(const std::vector<std::string>& args) {
    DownloadCommandOptions options;

    for (std::size_t index = 0; index < args.size(); ++index) {
        const auto& token = args[index];
        if (is_help_token(token)) {
            options.help = true;
        } else if (token == "--dataset") {
            options.dataset_option = take_value(args, index, token);
        } else if (token == "--start-year") {
            options.start_year = parse_int_or_throw(take_value(args, index, token), token);
        } else if (token == "--end-year") {
            options.end_year = parse_int_or_throw(take_value(args, index, token), token);
        } else if (token == "--output-dir") {
            options.output_dir = take_value(args, index, token);
        } else if (is_true_flag(token, "--overwrite")) {
            options.overwrite = true;
        } else if (is_false_flag(token, "--overwrite")) {
            options.overwrite = false;
        } else if (is_true_flag(token, "--resume")) {
            options.resume = true;
        } else if (is_false_flag(token, "--resume")) {
            options.resume = false;
        } else if (token == "--timeout") {
            options.timeout = parse_double_or_throw(take_value(args, index, token), token);
        } else if (token == "--chunk-size") {
            options.chunk_size = parse_uint64_or_throw(take_value(args, index, token), token);
        } else if (token == "--retries") {
            options.retries = parse_int_or_throw(take_value(args, index, token), token);
        } else if (!token.empty() && token[0] == '-') {
            throw CliError("Unknown option: " + token, 2);
        } else if (!options.dataset_argument.has_value()) {
            options.dataset_argument = token;
        } else {
            throw CliError("Too many arguments for the download command.", 2);
        }
    }

    return options;
}

std::string resolve_dataset_name(
    const DownloadCommandOptions& options,
    const AppConfig& config
) {
    if (options.dataset_argument.has_value() && options.dataset_option.has_value()) {
        const auto left = to_lower(trim(*options.dataset_argument));
        const auto right = to_lower(trim(*options.dataset_option));
        if (left != right) {
            throw CliError(
                "Use either the positional dataset argument or --dataset, not two different values.",
                2
            );
        }
    }

    if (options.dataset_argument.has_value()) {
        return to_lower(trim(*options.dataset_argument));
    }
    if (options.dataset_option.has_value()) {
        return to_lower(trim(*options.dataset_option));
    }
    return config.default_dataset;
}

void print_download_help(const Reporter& reporter) {
    reporter.section(
        "download command",
        "Download a dataset using the current config and flags.",
        true
    );
    reporter.blank_line(true);
    reporter.section("Usage", {}, true);
    reporter.print(
        "  oceandl download [dataset] [--start-year YEAR --end-year YEAR] [options]",
        true
    );

    reporter.blank_line(true);
    reporter.section("Options", {}, true);
    print_key_value_rows(
        {
            {"--dataset ID", "Select a dataset without using the positional argument"},
            {"--start-year YEAR", "Start year for per-year datasets"},
            {"--end-year YEAR", "End year for per-year datasets"},
            {"--output-dir PATH", "Override the output directory"},
            {"--overwrite", "Force re-download of a valid final file"},
            {"--no-overwrite", "Disable overwrite"},
            {"--resume", "Enable partial download resume"},
            {"--no-resume", "Disable resume"},
            {"--timeout SECONDS", "HTTP request timeout"},
            {"--chunk-size BYTES", "Requested libcurl HTTP receive buffer size"},
            {"--retries N", "Retry count after transient failures (0-10)"},
            {"--help, -h", "Show help for the download command"},
        },
        reporter
    );

    reporter.blank_line(true);
    reporter.section("Examples", {}, true);
    reporter.print("  oceandl download gpcp", true);
    reporter.print("  oceandl download oisst --start-year 2024 --end-year 2025", true);
}

int run_download_command(
    const DownloadCommandOptions& options,
    const AppConfig& config,
    const DatasetRegistry& dataset_registry,
    const ProviderRegistry& provider_registry,
    Reporter& reporter
) {
    const auto dataset_name = resolve_dataset_name(options, config);
    const auto& dataset = dataset_registry.get(dataset_name);
    const auto provider = provider_registry.get_for_dataset(dataset);

    DownloadRequest request{
        .dataset = dataset.id,
        .start_year = options.start_year,
        .end_year = options.end_year,
        .output_dir = options.output_dir.value_or(config.default_output_dir),
        .overwrite = options.overwrite.value_or(config.overwrite),
        .resume = options.resume.value_or(config.resume),
        .timeout = options.timeout.value_or(config.timeout),
        .chunk_size = options.chunk_size.value_or(config.chunk_size),
        .retry_count = options.retries.value_or(config.retry_count),
    };
    request.normalize_and_validate();
    dataset.validate_requested_years(request.start_year, request.end_year);

    ensure_directory(request.output_dir);
    print_download_parameters(request, dataset, *provider, reporter);

    CurlHttpClient http_client(config.user_agent);
    Downloader downloader(http_client, reporter);
    const auto summary = downloader.download(request, dataset, *provider);
    print_download_summary(summary, reporter);
    return summary.failed_count() > 0 ? 1 : 0;
}

}  // namespace oceandl
