#include "cli_support.hpp"

#include <algorithm>
#include <filesystem>
#include <optional>
#include <sstream>
#include <stdexcept>

#include <fmt/format.h>

#include "oceandl/downloader.hpp"
#include "oceandl/http_client.hpp"
#include "oceandl/utils.hpp"
#include "oceandl/version.hpp"

namespace oceandl {

namespace {

struct DownloadCommandOptions {
    std::optional<std::string> dataset_argument;
    std::optional<std::string> dataset_option;
    std::optional<int> start_year;
    std::optional<int> end_year;
    std::optional<std::filesystem::path> output_dir;
    std::optional<bool> overwrite;
    std::optional<bool> resume;
    std::optional<double> timeout;
    std::optional<std::uint64_t> chunk_size;
    std::optional<int> retries;
    bool help = false;
};

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

std::string dataset_mode_label(const DatasetInfo& dataset) {
    return dataset.requires_years() ? "per-year" : "single-file";
}

std::string yes_no(bool value) {
    return value ? "ya" : "tidak";
}

std::size_t resumed_count(const DownloadSummary& summary) {
    return static_cast<std::size_t>(std::count_if(summary.results.begin(), summary.results.end(), [](const auto& item) {
        return item.resumed;
    }));
}

std::uint64_t total_downloaded_bytes(const DownloadSummary& summary) {
    std::uint64_t total = 0;
    for (const auto& item : summary.results) {
        total += item.bytes_downloaded;
    }
    return total;
}

std::string take_value(const std::vector<std::string>& args, std::size_t& index, const std::string& option) {
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
        throw CliError("Nilai tidak valid untuk " + option + ": " + value, 2);
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
        return parsed;
    } catch (...) {
        throw CliError("Nilai tidak valid untuk " + option + ": " + value, 2);
    }
}

std::uint64_t parse_uint64_or_throw(const std::string& value, const std::string& option) {
    if (const auto parsed = parse_optional_uint(value)) {
        return *parsed;
    }
    throw CliError("Nilai tidak valid untuk " + option + ": " + value, 2);
}

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
            throw CliError("Argumen berlebih pada command download.", 2);
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
                "Gunakan salah satu dari argumen dataset atau --dataset, bukan dua nilai berbeda.",
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

void print_providers(
    const ProviderRegistry& provider_registry,
    const DatasetRegistry& dataset_registry,
    const Reporter& reporter
) {
    const auto providers = provider_registry.list();
    reporter.section("Provider", fmt::format("{} provider tersedia", providers.size()), true);

    for (std::size_t index = 0; index < providers.size(); ++index) {
        const auto& provider = providers[index];
        std::vector<std::string> dataset_ids;
        for (const auto& dataset : dataset_registry.list()) {
            if (dataset.provider_key == provider->provider_info().id) {
                dataset_ids.push_back(dataset.id);
            }
        }

        reporter.print(
            fmt::format(
                "  {} [{}]",
                provider->provider_info().name,
                provider->provider_info().id
            ),
            true
        );
        print_key_value_rows(
            {
                {"transport", provider->provider_info().transport},
                {"deskripsi", provider->provider_info().description},
                {"datasets", dataset_ids.empty() ? "-" : join_strings(dataset_ids, ", ")},
            },
            reporter
        );

        if (index + 1 < providers.size()) {
            reporter.blank_line(true);
        }
    }
}

void print_datasets(const DatasetRegistry& dataset_registry, const Reporter& reporter) {
    const auto datasets = dataset_registry.list();
    reporter.section(
        "Dataset",
        fmt::format(
            "{} dataset tersedia | default: {}",
            datasets.size(),
            dataset_registry.default_dataset()
        ),
        true
    );

    for (std::size_t index = 0; index < datasets.size(); ++index) {
        const auto& dataset = datasets[index];
        reporter.print(
            fmt::format(
                "  {}{}",
                dataset.id,
                dataset.id == dataset_registry.default_dataset() ? " [default]" : ""
            ),
            true
        );
        print_key_value_rows(
            {
                {"nama", dataset.display_name},
                {"provider", dataset.provider_key},
                {"tahun", dataset.year_range_label()},
                {"mode", dataset_mode_label(dataset)},
                {"deskripsi", dataset.description},
            },
            reporter
        );

        if (index + 1 < datasets.size()) {
            reporter.blank_line(true);
        }
    }
}

void print_info(
    const DatasetInfo& dataset,
    const DatasetProvider& provider,
    const Reporter& reporter
) {
    reporter.section(
        fmt::format("Dataset {}", dataset.id),
        dataset.display_name,
        true
    );
    print_key_value_rows(
        {
            {"deskripsi", dataset.description},
            {
                "provider",
                fmt::format(
                    "{} ({})",
                    provider.provider_info().name,
                    provider.provider_info().id
                ),
            },
            {"mode", dataset_mode_label(dataset)},
            {"base URL", dataset.base_url},
            {"pola file", dataset.filename_pattern},
            {"tahun", dataset.year_range_label()},
            {"contoh", dataset.example_file_name()},
        },
        reporter
    );
}

void print_download_parameters(
    const DownloadRequest& request,
    const DatasetInfo& dataset,
    const DatasetProvider& provider,
    const Reporter& reporter
) {
    reporter.section("Rencana download", dataset.display_name, true);
    print_key_value_rows(
        {
            {"dataset", fmt::format("{} ({})", dataset.id, dataset.display_name)},
            {"provider", provider.provider_info().name},
            {"tahun", dataset.requested_year_label(request.start_year, request.end_year)},
            {
                "total file",
                std::to_string(dataset.requested_file_count(request.start_year, request.end_year)),
            },
            {"output", (request.output_dir / dataset.id).string()},
            {"overwrite", yes_no(request.overwrite)},
            {"resume", yes_no(request.resume)},
            {"timeout", fmt::format("{:.1f} detik", request.timeout)},
            {"chunk size", format_bytes(request.chunk_size)},
            {"retries", std::to_string(request.retry_count)},
            {"base URL", dataset.base_url},
        },
        reporter
    );
}

void print_download_summary(const DownloadSummary& summary, const Reporter& reporter) {
    reporter.section(
        "Ringkasan download",
        fmt::format("dataset {} selesai diproses", summary.dataset.id),
        true
    );
    print_key_value_rows(
        {
            {"total target", std::to_string(summary.total_requested())},
            {"downloaded", std::to_string(summary.downloaded_count())},
            {"skipped", std::to_string(summary.skipped_count())},
            {"failed", std::to_string(summary.failed_count())},
            {"resumed", std::to_string(resumed_count(summary))},
            {"bytes diterima", format_bytes(total_downloaded_bytes(summary))},
        },
        reporter
    );

    const auto failures = summary.failures();
    if (!failures.empty()) {
        reporter.blank_line(true);
        reporter.section("Target gagal", {}, true);
        for (const auto& failure : failures) {
            reporter.print(
                fmt::format("  {} -> {}", failure.target.file_name, failure.error_message),
                true
            );
        }
    }

    reporter.blank_line(true);
    if (summary.failed_count() == 0) {
        reporter.success("Semua target selesai tanpa error.");
    } else {
        reporter.warning(
            fmt::format("{} target gagal dan perlu dicek ulang.", summary.failed_count())
        );
    }
}

void print_providers_help(const Reporter& reporter) {
    reporter.section("Perintah providers", "Lihat provider yang tersedia.", true);
    reporter.blank_line(true);
    reporter.section("Cara pakai", {}, true);
    reporter.print("  oceandl providers", true);
}

void print_datasets_help(const Reporter& reporter) {
    reporter.section("Perintah datasets", "Lihat katalog dataset bawaan.", true);
    reporter.blank_line(true);
    reporter.section("Cara pakai", {}, true);
    reporter.print("  oceandl datasets", true);
}

void print_info_help(const Reporter& reporter) {
    reporter.section("Perintah info", "Tampilkan metadata detail sebuah dataset.", true);
    reporter.blank_line(true);
    reporter.section("Cara pakai", {}, true);
    reporter.print("  oceandl info <dataset>", true);
    reporter.blank_line(true);
    reporter.section("Contoh", {}, true);
    reporter.print("  oceandl info oisst", true);
}

void print_download_help(const Reporter& reporter) {
    reporter.section("Perintah download", "Unduh dataset memakai config dan flag saat ini.", true);
    reporter.blank_line(true);
    reporter.section("Cara pakai", {}, true);
    reporter.print(
        "  oceandl download [dataset] [--start-year YEAR --end-year YEAR] [options]",
        true
    );

    reporter.blank_line(true);
    reporter.section("Opsi", {}, true);
    print_key_value_rows(
        {
            {"--dataset ID", "Pilih dataset tanpa argumen posisi"},
            {"--start-year YEAR", "Tahun awal untuk dataset per-year"},
            {"--end-year YEAR", "Tahun akhir untuk dataset per-year"},
            {"--output-dir PATH", "Override direktori output"},
            {"--overwrite", "Paksa unduh ulang file final yang valid"},
            {"--no-overwrite", "Nonaktifkan overwrite"},
            {"--resume", "Aktifkan resume partial download"},
            {"--no-resume", "Matikan resume"},
            {"--timeout DETIK", "Timeout request HTTP"},
            {"--chunk-size BYTES", "Ukuran buffer terima HTTP yang diminta ke libcurl"},
            {"--retries N", "Jumlah retry setelah kegagalan transient"},
            {"--help, -h", "Tampilkan bantuan perintah download"},
        },
        reporter
    );

    reporter.blank_line(true);
    reporter.section("Contoh", {}, true);
    reporter.print("  oceandl download gpcp", true);
    reporter.print("  oceandl download oisst --start-year 2024 --end-year 2025", true);
}

}  // namespace

CliError::CliError(std::string message, int exit_code)
    : std::runtime_error(std::move(message)), exit_code_(exit_code) {}

int CliError::exit_code() const {
    return exit_code_;
}

GlobalOptions parse_global_options(const std::vector<std::string>& args) {
    GlobalOptions options;
    bool verbose = false;
    bool quiet = false;

    for (std::size_t index = 0; index < args.size(); ++index) {
        const auto& token = args[index];
        if (token == "--version") {
            options.version = true;
        } else if (token == "--config") {
            options.config_path = take_value(args, index, token);
        } else if (token == "--verbose") {
            verbose = true;
        } else if (token == "--quiet") {
            quiet = true;
        } else {
            options.remaining.push_back(token);
        }
    }

    if (verbose && quiet) {
        throw CliError("Gunakan salah satu dari --verbose atau --quiet.", 2);
    }
    if (verbose) {
        options.verbosity = Verbosity::Verbose;
    } else if (quiet) {
        options.verbosity = Verbosity::Quiet;
    }

    return options;
}

CliRuntime load_cli_runtime(const std::filesystem::path& config_path) {
    auto config = load_config(config_path);
    return {
        .config = config,
        .dataset_registry = build_default_dataset_registry(config),
        .provider_registry = build_default_provider_registry(),
    };
}

void print_help(const Reporter& reporter) {
    reporter.section(
        fmt::format("oceandl {}", kVersion),
        "CLI C++ untuk mengunduh dataset NetCDF dari NOAA PSL.",
        true
    );
    reporter.blank_line(true);
    reporter.section("Cara pakai", {}, true);
    print_key_value_rows(
        {
            {"utama", "oceandl [--config PATH] [--verbose|--quiet] <command> [options]"},
            {"versi", "oceandl --version"},
        },
        reporter
    );

    reporter.blank_line(true);
    reporter.section("Perintah", {}, true);
    print_key_value_rows(
        {
            {"providers", "Lihat provider yang tersedia"},
            {"datasets", "Lihat katalog dataset bawaan"},
            {"info <dataset>", "Tampilkan metadata detail sebuah dataset"},
            {"download [dataset]", "Unduh dataset memakai config dan flag saat ini"},
            {"help [command]", "Tampilkan bantuan umum atau bantuan command tertentu"},
        },
        reporter
    );

    reporter.blank_line(true);
    reporter.section("Opsi global", {}, true);
    print_key_value_rows(
        {
            {"--config PATH", "Gunakan file config tertentu"},
            {"--help, -h", "Tampilkan bantuan umum"},
            {"--verbose", "Tampilkan detail proses tambahan"},
            {"--quiet", "Sembunyikan output non-kritis"},
        },
        reporter
    );

    reporter.blank_line(true);
    reporter.section("Contoh", {}, true);
    reporter.print("  oceandl --help", true);
    reporter.print("  oceandl datasets", true);
    reporter.print("  oceandl download --help", true);
    reporter.print("  oceandl info oisst", true);
    reporter.print("  oceandl download oisst --start-year 2024 --end-year 2025", true);
}

int run_command(
    const std::vector<std::string>& args,
    const CliRuntime& runtime,
    Reporter& reporter
) {
    if (args.empty()) {
        print_help(reporter);
        return 0;
    }

    const auto& command = args.front();
    const std::vector<std::string> command_args(args.begin() + 1, args.end());

    if (command == "help") {
        if (command_args.empty()) {
            print_help(reporter);
            return 0;
        }
        if (command_args.size() != 1) {
            throw CliError("Command help menerima nol atau satu nama perintah.", 2);
        }

        const auto& help_target = command_args.front();
        if (help_target == "providers") {
            print_providers_help(reporter);
            return 0;
        }
        if (help_target == "datasets") {
            print_datasets_help(reporter);
            return 0;
        }
        if (help_target == "info") {
            print_info_help(reporter);
            return 0;
        }
        if (help_target == "download") {
            print_download_help(reporter);
            return 0;
        }
        throw CliError("Bantuan untuk command tidak dikenal: " + help_target, 2);
    }

    if (command == "providers") {
        if (command_args.size() == 1 && is_help_token(command_args.front())) {
            print_providers_help(reporter);
            return 0;
        }
        if (!command_args.empty()) {
            throw CliError("Command providers tidak menerima argumen.", 2);
        }
        print_providers(runtime.provider_registry, runtime.dataset_registry, reporter);
        return 0;
    }

    if (command == "datasets") {
        if (command_args.size() == 1 && is_help_token(command_args.front())) {
            print_datasets_help(reporter);
            return 0;
        }
        if (!command_args.empty()) {
            throw CliError("Command datasets tidak menerima argumen.", 2);
        }
        print_datasets(runtime.dataset_registry, reporter);
        return 0;
    }

    if (command == "info") {
        if (command_args.size() == 1 && is_help_token(command_args.front())) {
            print_info_help(reporter);
            return 0;
        }
        if (command_args.size() != 1) {
            throw CliError("Command info membutuhkan satu dataset id.", 2);
        }
        const auto& dataset = runtime.dataset_registry.get(command_args[0]);
        const auto provider = runtime.provider_registry.get_for_dataset(dataset);
        print_info(dataset, *provider, reporter);
        return 0;
    }

    if (command == "download") {
        const auto options = parse_download_options(command_args);
        if (options.help) {
            print_download_help(reporter);
            return 0;
        }
        const auto dataset_name = resolve_dataset_name(options, runtime.config);
        const auto& dataset = runtime.dataset_registry.get(dataset_name);
        const auto provider = runtime.provider_registry.get_for_dataset(dataset);

        DownloadRequest request{
            .dataset = dataset.id,
            .start_year = options.start_year,
            .end_year = options.end_year,
            .output_dir = options.output_dir.value_or(runtime.config.default_output_dir),
            .overwrite = options.overwrite.value_or(runtime.config.overwrite),
            .resume = options.resume.value_or(runtime.config.resume),
            .timeout = options.timeout.value_or(runtime.config.timeout),
            .chunk_size = options.chunk_size.value_or(runtime.config.chunk_size),
            .retry_count = options.retries.value_or(runtime.config.retry_count),
        };
        request.normalize_and_validate();
        dataset.validate_requested_years(request.start_year, request.end_year);

        ensure_directory(request.output_dir);
        print_download_parameters(request, dataset, *provider, reporter);

        CurlHttpClient http_client(runtime.config.user_agent);
        Downloader downloader(http_client, reporter);
        const auto summary = downloader.download(request, dataset, *provider);
        print_download_summary(summary, reporter);
        return summary.failed_count() > 0 ? 1 : 0;
    }

    throw CliError("Unknown command: " + command, 2);
}

}  // namespace oceandl
