#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "oceandl/config.hpp"
#include "oceandl/process_runner.hpp"
#include "oceandl/reporter.hpp"

namespace oceandl {

struct CopernicusMarineResolutionOptions {
    std::optional<std::filesystem::path> cli_executable;
};

struct CopernicusMarineCommand {
    std::filesystem::path executable;
    std::vector<std::string> prefix_args;
    std::string source;
    CopernicusMarineRunner runner = CopernicusMarineRunner::System;
};

std::filesystem::path default_copernicusmarine_managed_executable();
std::optional<std::filesystem::path> find_executable_in_path(const std::string& name);
std::optional<CopernicusMarineCommand> resolve_copernicusmarine_command(
    const AppConfig& config,
    const CopernicusMarineResolutionOptions& options = {}
);
ProcessCommand build_copernicusmarine_process(
    const CopernicusMarineCommand& command,
    const std::vector<std::string>& arguments
);
void print_copernicusmarine_help(const Reporter& reporter);
int handle_copernicusmarine_command(
    const std::vector<std::string>& args,
    const AppConfig& config,
    const std::filesystem::path& config_path,
    Reporter& reporter
);

}  // namespace oceandl
