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

bool any_help_token(const std::vector<std::string>& args) {
    return std::any_of(args.begin(), args.end(), [](const std::string& token) {
        return is_help_token(token);
    });
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
    reporter.section("Providers", fmt::format("{} provider(s) available", providers.size()), true);

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
                {"description", provider->provider_info().description},
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
        "Datasets",
        fmt::format(
            "{} dataset(s) available | default: {}",
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
                {"name", dataset.display_name},
                {"provider", dataset.provider_key},
                {"years", dataset.year_range_label()},
                {"mode", dataset_mode_label(dataset)},
                {"description", dataset.description},
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
            {"description", dataset.description},
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
            {"file pattern", dataset.filename_pattern},
            {"years", dataset.year_range_label()},
            {"example", dataset.example_file_name()},
        },
        reporter
    );
}

void print_providers_help(const Reporter& reporter) {
    reporter.section("providers command", "List the available providers.", true);
    reporter.blank_line(true);
    reporter.section("Usage", {}, true);
    reporter.print("  oceandl providers", true);
}

void print_datasets_help(const Reporter& reporter) {
    reporter.section("datasets command", "List the built-in dataset catalog.", true);
    reporter.blank_line(true);
    reporter.section("Usage", {}, true);
    reporter.print("  oceandl datasets", true);
}

void print_info_help(const Reporter& reporter) {
    reporter.section("info command", "Show detailed metadata for a dataset.", true);
    reporter.blank_line(true);
    reporter.section("Usage", {}, true);
    reporter.print("  oceandl info <dataset>", true);
    reporter.blank_line(true);
    reporter.section("Example", {}, true);
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
        throw CliError("Use either --verbose or --quiet.", 2);
    }
    if (verbose) {
        options.verbosity = Verbosity::Verbose;
    } else if (quiet) {
        options.verbosity = Verbosity::Quiet;
    }

    return options;
}

RuntimeRequirement classify_runtime_requirement(const std::vector<std::string>& args) {
    if (args.empty()) {
        return RuntimeRequirement::None;
    }

    const auto& command = args.front();
    const std::vector<std::string> command_args(args.begin() + 1, args.end());
    if (command == "help") {
        return RuntimeRequirement::None;
    }
    if (command == "download") {
        return any_help_token(command_args) ? RuntimeRequirement::None : RuntimeRequirement::Required;
    }
    if (command == "providers" || command == "datasets" || command == "info") {
        if (command_args.size() == 1 && is_help_token(command_args.front())) {
            return RuntimeRequirement::None;
        }
        return RuntimeRequirement::Optional;
    }

    return RuntimeRequirement::None;
}

CliRuntime default_cli_runtime() {
    auto config = default_app_config();
    return {
        .config = config,
        .dataset_registry = build_default_dataset_registry(config),
        .provider_registry = build_default_provider_registry(),
        .config_path = default_config_path(),
        .config_warnings = {},
        .config_loaded_from_file = false,
    };
}

CliRuntime load_cli_runtime(const std::filesystem::path& config_path) {
    const auto loaded = load_config_with_diagnostics(config_path);
    return {
        .config = loaded.config,
        .dataset_registry = build_default_dataset_registry(loaded.config),
        .provider_registry = build_default_provider_registry(),
        .config_path = loaded.path,
        .config_warnings = loaded.warnings,
        .config_loaded_from_file = loaded.loaded_from_file,
    };
}

void print_help(const Reporter& reporter) {
    reporter.section(
        fmt::format("oceandl {}", kVersion),
        "C++ CLI for downloading NetCDF datasets from NOAA PSL.",
        true
    );
    reporter.blank_line(true);
    reporter.section("Usage", {}, true);
    print_key_value_rows(
        {
            {"main", "oceandl [--config PATH] [--verbose|--quiet] <command> [options]"},
            {"version", "oceandl --version"},
        },
        reporter
    );

    reporter.blank_line(true);
    reporter.section("Commands", {}, true);
    print_key_value_rows(
        {
            {"providers", "List the available providers"},
            {"datasets", "List the built-in dataset catalog"},
            {"info <dataset>", "Show detailed metadata for a dataset"},
            {"download [dataset]", "Download a dataset using the current config and flags"},
            {"help [command]", "Show general help or command-specific help"},
        },
        reporter
    );

    reporter.blank_line(true);
    reporter.section("Global options", {}, true);
    print_key_value_rows(
        {
            {"--config PATH", "Use a specific config file"},
            {"--help, -h", "Show general help"},
            {"--verbose", "Show additional process details"},
            {"--quiet", "Hide non-critical output"},
        },
        reporter
    );

    reporter.blank_line(true);
    reporter.section("Examples", {}, true);
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
            throw CliError("The help command accepts zero or one command name.", 2);
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
        throw CliError("Help is not available for unknown command: " + help_target, 2);
    }

    if (command == "providers") {
        if (command_args.size() == 1 && is_help_token(command_args.front())) {
            print_providers_help(reporter);
            return 0;
        }
        if (!command_args.empty()) {
            throw CliError("The providers command does not accept arguments.", 2);
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
            throw CliError("The datasets command does not accept arguments.", 2);
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
            throw CliError("The info command requires exactly one dataset id.", 2);
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
