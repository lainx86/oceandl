#include "cli_support.hpp"

#include <algorithm>
#include <filesystem>
#include <stdexcept>

#include <fmt/format.h>

#include "download_command.hpp"
#include "oceandl/utils.hpp"
#include "oceandl/version.hpp"

namespace oceandl {

namespace {

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

std::string take_value(const std::vector<std::string>& args, std::size_t& index, const std::string& option) {
    if (index + 1 >= args.size()) {
        throw CliError("Missing value for " + option + ".", 2);
    }
    ++index;
    return args[index];
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
        return run_download_command(
            options,
            runtime.config,
            runtime.dataset_registry,
            runtime.provider_registry,
            reporter
        );
    }

    throw CliError("Unknown command: " + command, 2);
}

}  // namespace oceandl
