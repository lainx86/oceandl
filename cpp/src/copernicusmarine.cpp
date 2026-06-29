#include "oceandl/copernicusmarine.hpp"

#include "cli_support.hpp"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <optional>
#include <string_view>

#include <fmt/format.h>

#include "oceandl/utils.hpp"

#ifndef _WIN32
#include <unistd.h>
#endif

namespace oceandl {

namespace {

constexpr std::string_view kToolName = "copernicusmarine";

std::optional<std::filesystem::path> getenv_path_local(const char* name) {
    const char* value = std::getenv(name);
    if (value == nullptr || std::string_view(value).empty()) {
        return std::nullopt;
    }
    return expand_user(std::filesystem::path(value));
}

bool has_path_separator(const std::string& value) {
    return value.find('/') != std::string::npos
#ifdef _WIN32
        || value.find('\\') != std::string::npos
#endif
        ;
}

bool is_executable_file(const std::filesystem::path& path) {
    std::error_code error;
    if (!std::filesystem::is_regular_file(path, error) || error) {
        return false;
    }
#ifdef _WIN32
    return true;
#else
    return ::access(path.c_str(), X_OK) == 0;
#endif
}

std::vector<std::filesystem::path> candidate_executable_names(const std::string& name) {
#ifdef _WIN32
    if (name.size() >= 4 && to_lower(std::string_view(name).substr(name.size() - 4)) == ".exe") {
        return {name};
    }
    return {name, name + ".exe"};
#else
    return {name};
#endif
}

char path_separator() {
#ifdef _WIN32
    return ';';
#else
    return ':';
#endif
}

std::optional<std::filesystem::path> configured_system_command(
    const std::filesystem::path& executable
) {
    if (executable.empty()) {
        return std::nullopt;
    }

    return expand_user(executable);
}

CopernicusMarineCommand make_runner_command(
    CopernicusMarineRunner runner,
    const std::string& env,
    const std::string& source
) {
    if (runner == CopernicusMarineRunner::Micromamba) {
        return {
            .executable = find_executable_in_path("micromamba").value_or("micromamba"),
            .prefix_args = {"run", "-n", env, std::string(kToolName)},
            .source = source,
            .runner = runner,
        };
    }
    if (runner == CopernicusMarineRunner::Conda) {
        return {
            .executable = find_executable_in_path("conda").value_or("conda"),
            .prefix_args = {"run", "-n", env, std::string(kToolName)},
            .source = source,
            .runner = runner,
        };
    }

    return {
        .executable = find_executable_in_path(std::string(kToolName)).value_or(std::string(kToolName)),
        .prefix_args = {},
        .source = source,
        .runner = runner,
    };
}

std::string first_nonempty_line(std::string value) {
    value = trim(value);
    std::size_t begin = 0;
    while (begin < value.size()) {
        const auto end = value.find('\n', begin);
        const auto line = trim(
            std::string_view(value).substr(
                begin,
                end == std::string::npos ? std::string::npos : end - begin
            )
        );
        if (!line.empty()) {
            return line;
        }
        if (end == std::string::npos) {
            break;
        }
        begin = end + 1;
    }
    return {};
}

std::string command_summary(const CopernicusMarineCommand& command) {
    std::vector<std::string> parts{command.executable.string()};
    parts.insert(parts.end(), command.prefix_args.begin(), command.prefix_args.end());
    return join_strings(parts, " ");
}

void print_missing_tool_message(const Reporter& reporter) {
    reporter.error("Copernicus Marine Toolbox was not found.");
    reporter.print("", true);
    reporter.print("Run:", true);
    reporter.print("  oceandl cm setup", true);
    reporter.print("", true);
    reporter.print("Or install it manually, then configure OceanDL:", true);
    reporter.print("  oceandl cm setup --executable /path/to/copernicusmarine", true);
}

ProcessResult run_version_check(const CopernicusMarineCommand& command) {
    return run_process_capture(build_copernicusmarine_process(command, {"--version"}));
}

bool report_version_failure(
    const CopernicusMarineCommand& command,
    const ProcessResult& result,
    const Reporter& reporter
) {
    if (!result.started) {
        reporter.error(
            "Failed to start Copernicus Marine Toolbox from " + command.source + ": "
            + result.error_message
        );
        return false;
    }

    reporter.error(
        fmt::format(
            "Copernicus Marine Toolbox version check failed with exit code {}.",
            result.exit_code
        )
    );
    const auto stderr_line = first_nonempty_line(result.stderr_text);
    const auto stdout_line = first_nonempty_line(result.stdout_text);
    if (!stderr_line.empty()) {
        reporter.print("  " + stderr_line, true);
    } else if (!stdout_line.empty()) {
        reporter.print("  " + stdout_line, true);
    }
    return false;
}

void print_setup_help(const Reporter& reporter) {
    reporter.section(
        "cm setup command",
        "Configure the official Copernicus Marine Toolbox used by oceandl cm.",
        true
    );
    reporter.blank_line(true);
    reporter.section("Usage", {}, true);
    reporter.print("  oceandl cm setup", true);
    reporter.print("  oceandl cm setup --executable /path/to/copernicusmarine", true);
    reporter.print("  oceandl cm setup --runner micromamba --env copernicusmarine", true);
    reporter.print("  oceandl cm setup --runner conda --env copernicusmarine", true);
    reporter.blank_line(true);
    reporter.section("Options", {}, true);
    reporter.field("--executable PATH", "Use an existing copernicusmarine executable", 20, true);
    reporter.field("--runner MODE", "system, micromamba, or conda", 20, true);
    reporter.field("--env NAME", "Conda/micromamba environment name", 20, true);
    reporter.field("--system", "Detect copernicusmarine from PATH", 20, true);
}

struct CmInvocation {
    std::optional<std::filesystem::path> cli_executable;
    std::string subcommand;
    std::vector<std::string> forwarded_args;
    bool help = false;
};

std::string take_cm_value(
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

CmInvocation parse_cm_invocation(const std::vector<std::string>& args) {
    CmInvocation invocation;
    if (args.empty()) {
        invocation.help = true;
        return invocation;
    }

    for (std::size_t index = 0; index < args.size(); ++index) {
        const auto& token = args[index];
        if (token == "--help" || token == "-h" || token == "help") {
            invocation.help = true;
            return invocation;
        }
        if (token == "--executable") {
            invocation.cli_executable = take_cm_value(args, index, token);
            continue;
        }
        if (!token.empty() && token.front() == '-') {
            throw CliError("Unknown oceandl cm option: " + token, 2);
        }

        invocation.subcommand = token;
        invocation.forwarded_args.assign(args.begin() + static_cast<std::ptrdiff_t>(index + 1), args.end());
        return invocation;
    }

    invocation.help = true;
    return invocation;
}

bool is_passthrough_subcommand(const std::string& subcommand) {
    return subcommand == "login" || subcommand == "describe" || subcommand == "get"
        || subcommand == "subset";
}

struct SetupOptions {
    std::optional<std::filesystem::path> executable;
    std::optional<CopernicusMarineRunner> runner;
    std::string env;
    bool system = false;
    bool help = false;
};

SetupOptions parse_setup_options(const std::vector<std::string>& args) {
    SetupOptions options;
    for (std::size_t index = 0; index < args.size(); ++index) {
        const auto& token = args[index];
        if (token == "--help" || token == "-h") {
            options.help = true;
        } else if (token == "--executable") {
            options.executable = take_cm_value(args, index, token);
        } else if (token == "--runner") {
            try {
                options.runner = parse_copernicusmarine_runner(take_cm_value(args, index, token));
            } catch (const std::exception& error) {
                throw CliError(error.what(), 2);
            }
        } else if (token == "--env") {
            options.env = take_cm_value(args, index, token);
        } else if (token == "--system") {
            options.system = true;
        } else {
            throw CliError("Unknown cm setup option: " + token, 2);
        }
    }
    return options;
}

CopernicusMarineCommand command_from_setup_options(const SetupOptions& options) {
    const bool explicit_runner =
        options.runner.has_value() && *options.runner != CopernicusMarineRunner::System;
    const int selected_modes =
        (options.executable.has_value() ? 1 : 0) + (explicit_runner ? 1 : 0)
        + (options.system ? 1 : 0);
    if (selected_modes > 1) {
        throw CliError("Choose only one cm setup mode.", 2);
    }
    if (options.executable.has_value()) {
        if (!options.env.empty()) {
            throw CliError("--env is only valid with --runner micromamba or --runner conda.", 2);
        }
        return {
            .executable = expand_user(*options.executable),
            .prefix_args = {},
            .source = "--executable",
            .runner = CopernicusMarineRunner::System,
        };
    }

    if (explicit_runner) {
        const auto env = trim(options.env);
        if (env.empty()) {
            throw CliError("--env is required with --runner micromamba or --runner conda.", 2);
        }
        return make_runner_command(*options.runner, env, "--runner");
    }

    if (!options.env.empty()) {
        throw CliError("--env is only valid with --runner micromamba or --runner conda.", 2);
    }

    const auto executable = find_executable_in_path(std::string(kToolName));
    if (!executable.has_value()) {
        throw CliError("Copernicus Marine Toolbox was not found on PATH.", 1);
    }
    return {
        .executable = *executable,
        .prefix_args = {},
        .source = options.system ? "--system" : "PATH",
        .runner = CopernicusMarineRunner::System,
    };
}

CopernicusMarineConfig config_from_command(const CopernicusMarineCommand& command) {
    CopernicusMarineConfig config;
    config.runner = command.runner;
    if (command.runner == CopernicusMarineRunner::System) {
        config.executable = command.executable;
    } else if (command.prefix_args.size() >= 3) {
        config.env = command.prefix_args[2];
    }
    config.normalize_and_validate();
    return config;
}

int handle_doctor(
    const AppConfig& config,
    const CopernicusMarineResolutionOptions& options,
    const Reporter& reporter
) {
    const auto command = resolve_copernicusmarine_command(config, options);
    if (!command.has_value()) {
        print_missing_tool_message(reporter);
        return 1;
    }

    reporter.section("Copernicus Marine doctor", "Checking unofficial wrapper dependencies.", true);
    reporter.field("source", command->source, 8, true);
    reporter.field("runner", copernicusmarine_runner_name(command->runner), 8, true);
    reporter.field("command", command_summary(*command), 8, true);

    const auto result = run_version_check(*command);
    if (result.exit_code != 0 || !result.started) {
        (void)report_version_failure(*command, result, reporter);
        reporter.print("", true);
        reporter.print("Run:", true);
        reporter.print("  oceandl cm setup", true);
        return 1;
    }

    const auto version = first_nonempty_line(result.stdout_text).empty()
        ? first_nonempty_line(result.stderr_text)
        : first_nonempty_line(result.stdout_text);
    reporter.success(
        version.empty()
            ? "Copernicus Marine Toolbox is ready."
            : "Copernicus Marine Toolbox is ready: " + version
    );
    return 0;
}

int handle_setup(
    const std::vector<std::string>& args,
    const std::filesystem::path& config_path,
    const Reporter& reporter
) {
    const auto options = parse_setup_options(args);
    if (options.help) {
        print_setup_help(reporter);
        return 0;
    }

    CopernicusMarineCommand command;
    try {
        command = command_from_setup_options(options);
    } catch (const CliError& error) {
        if (error.exit_code() == 1) {
            print_missing_tool_message(reporter);
            return 1;
        }
        throw;
    }

    const auto result = run_version_check(command);
    if (result.exit_code != 0 || !result.started) {
        (void)report_version_failure(command, result, reporter);
        return 1;
    }

    const auto saved = config_from_command(command);
    save_copernicusmarine_config(config_path, saved);

    const auto version = first_nonempty_line(result.stdout_text).empty()
        ? first_nonempty_line(result.stderr_text)
        : first_nonempty_line(result.stdout_text);
    reporter.success(
        version.empty()
            ? "Saved Copernicus Marine Toolbox configuration."
            : "Verified Copernicus Marine Toolbox: " + version
    );
    reporter.field("config", expand_user(config_path).string(), 8, true);
    return 0;
}

int run_passthrough(
    const std::string& subcommand,
    const std::vector<std::string>& forwarded_args,
    const AppConfig& config,
    const CopernicusMarineResolutionOptions& options,
    const Reporter& reporter
) {
    const auto command = resolve_copernicusmarine_command(config, options);
    if (!command.has_value()) {
        print_missing_tool_message(reporter);
        return 1;
    }

    std::vector<std::string> arguments{subcommand};
    arguments.insert(arguments.end(), forwarded_args.begin(), forwarded_args.end());
    const auto result = run_process_inherit(build_copernicusmarine_process(*command, arguments));
    if (!result.started) {
        reporter.error("Failed to start Copernicus Marine Toolbox: " + result.error_message);
        return 1;
    }
    if (result.exit_code == 127) {
        reporter.error("Copernicus Marine Toolbox could not be started.");
        reporter.print("Run:", true);
        reporter.print("  oceandl cm setup", true);
    }
    return result.exit_code;
}

}  // namespace

std::filesystem::path default_copernicusmarine_managed_executable() {
#ifdef _WIN32
    std::filesystem::path root;
    if (const auto local_app_data = getenv_path_local("LOCALAPPDATA")) {
        root = *local_app_data / "oceandl";
    } else {
        root = expand_user(std::filesystem::path("~/AppData/Local/oceandl"));
    }
    return root / "tools" / "copernicusmarine" / "copernicusmarine.exe";
#elif defined(__APPLE__)
    return expand_user(
        std::filesystem::path("~/Library/Application Support/oceandl/tools/copernicusmarine/copernicusmarine")
    );
#else
    std::filesystem::path root;
    if (const auto xdg_data_home = getenv_path_local("XDG_DATA_HOME")) {
        root = *xdg_data_home / "oceandl";
    } else {
        root = expand_user(std::filesystem::path("~/.local/share/oceandl"));
    }
    return root / "tools" / "copernicusmarine" / "copernicusmarine";
#endif
}

std::optional<std::filesystem::path> find_executable_in_path(const std::string& name) {
    if (name.empty()) {
        return std::nullopt;
    }

    if (has_path_separator(name)) {
        const auto candidate = expand_user(std::filesystem::path(name));
        return is_executable_file(candidate) ? std::optional<std::filesystem::path>(candidate)
                                             : std::nullopt;
    }

    const char* raw_path = std::getenv("PATH");
    if (raw_path == nullptr) {
        return std::nullopt;
    }
    const std::string path_value(raw_path);

    std::size_t begin = 0;
    while (begin <= path_value.size()) {
        const auto end = path_value.find(path_separator(), begin);
        const auto segment = path_value.substr(
            begin,
            end == std::string::npos ? std::string::npos : end - begin
        );
        const auto directory = segment.empty() ? std::filesystem::path(".")
                                               : std::filesystem::path(segment);
        for (const auto& executable_name : candidate_executable_names(name)) {
            const auto candidate = directory / executable_name;
            if (is_executable_file(candidate)) {
                return candidate;
            }
        }

        if (end == std::string::npos) {
            break;
        }
        begin = end + 1;
    }

    return std::nullopt;
}

std::optional<CopernicusMarineCommand> resolve_copernicusmarine_command(
    const AppConfig& config,
    const CopernicusMarineResolutionOptions& options
) {
    if (options.cli_executable.has_value()) {
        return CopernicusMarineCommand{
            .executable = expand_user(*options.cli_executable),
            .prefix_args = {},
            .source = "--executable",
            .runner = CopernicusMarineRunner::System,
        };
    }

    if (const auto env_executable = getenv_path_local("OCEANDL_CM_BIN")) {
        return CopernicusMarineCommand{
            .executable = *env_executable,
            .prefix_args = {},
            .source = "OCEANDL_CM_BIN",
            .runner = CopernicusMarineRunner::System,
        };
    }

    if (config.copernicusmarine.runner == CopernicusMarineRunner::Micromamba
        || config.copernicusmarine.runner == CopernicusMarineRunner::Conda) {
        return make_runner_command(
            config.copernicusmarine.runner,
            config.copernicusmarine.env,
            "config"
        );
    }

    if (const auto configured = configured_system_command(config.copernicusmarine.executable)) {
        return CopernicusMarineCommand{
            .executable = *configured,
            .prefix_args = {},
            .source = "config",
            .runner = CopernicusMarineRunner::System,
        };
    }

    const auto managed = default_copernicusmarine_managed_executable();
    if (is_executable_file(managed)) {
        return CopernicusMarineCommand{
            .executable = managed,
            .prefix_args = {},
            .source = "managed",
            .runner = CopernicusMarineRunner::System,
        };
    }

    if (const auto path_executable = find_executable_in_path(std::string(kToolName))) {
        return CopernicusMarineCommand{
            .executable = *path_executable,
            .prefix_args = {},
            .source = "PATH",
            .runner = CopernicusMarineRunner::System,
        };
    }

    return std::nullopt;
}

ProcessCommand build_copernicusmarine_process(
    const CopernicusMarineCommand& command,
    const std::vector<std::string>& arguments
) {
    ProcessCommand process{
        .executable = command.executable,
        .arguments = command.prefix_args,
    };
    process.arguments.insert(process.arguments.end(), arguments.begin(), arguments.end());
    return process;
}

void print_copernicusmarine_help(const Reporter& reporter) {
    reporter.section(
        "cm command",
        "Unofficial wrapper around the official Copernicus Marine Toolbox.",
        true
    );
    reporter.blank_line(true);
    reporter.section("Usage", {}, true);
    reporter.print("  oceandl cm --help", true);
    reporter.print("  oceandl cm doctor", true);
    reporter.print("  oceandl cm setup [options]", true);
    reporter.print("  oceandl cm login [args...]", true);
    reporter.print("  oceandl cm describe [args...]", true);
    reporter.print("  oceandl cm get [args...]", true);
    reporter.print("  oceandl cm subset [args...]", true);
    reporter.blank_line(true);
    reporter.section("Wrapper options", {}, true);
    reporter.field("--executable PATH", "Use a Copernicus Marine executable for this command", 20, true);
    reporter.blank_line(true);
    reporter.section("Notes", {}, true);
    reporter.print("  oceandl cm forwards login, describe, get, and subset to copernicusmarine.", true);
    reporter.print("  Credentials are handled by the official copernicusmarine login command.", true);
}

int handle_copernicusmarine_command(
    const std::vector<std::string>& args,
    const AppConfig& config,
    const std::filesystem::path& config_path,
    Reporter& reporter
) {
    const auto invocation = parse_cm_invocation(args);
    if (invocation.help) {
        print_copernicusmarine_help(reporter);
        return 0;
    }

    CopernicusMarineResolutionOptions resolution_options{
        .cli_executable = invocation.cli_executable,
    };

    if (invocation.subcommand == "doctor") {
        if (!invocation.forwarded_args.empty()) {
            throw CliError("The cm doctor command does not accept arguments.", 2);
        }
        return handle_doctor(config, resolution_options, reporter);
    }
    if (invocation.subcommand == "setup") {
        if (invocation.cli_executable.has_value()) {
            throw CliError("Use oceandl cm setup --executable PATH instead.", 2);
        }
        return handle_setup(invocation.forwarded_args, config_path, reporter);
    }
    if (is_passthrough_subcommand(invocation.subcommand)) {
        return run_passthrough(
            invocation.subcommand,
            invocation.forwarded_args,
            config,
            resolution_options,
            reporter
        );
    }

    throw CliError("Unknown oceandl cm command: " + invocation.subcommand, 2);
}

}  // namespace oceandl
