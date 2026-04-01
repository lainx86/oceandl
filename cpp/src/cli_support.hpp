#pragma once

#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

#include "oceandl/catalog.hpp"
#include "oceandl/config.hpp"
#include "oceandl/providers/registry.hpp"
#include "oceandl/reporter.hpp"

namespace oceandl {

class CliError : public std::runtime_error {
  public:
    CliError(std::string message, int exit_code);

    int exit_code() const;

  private:
    int exit_code_;
};

struct GlobalOptions {
    std::filesystem::path config_path = default_config_path();
    Verbosity verbosity = Verbosity::Normal;
    bool version = false;
    std::vector<std::string> remaining;
};

struct CliRuntime {
    AppConfig config;
    DatasetRegistry dataset_registry;
    ProviderRegistry provider_registry;
    std::filesystem::path config_path = default_config_path();
    std::vector<std::string> config_warnings;
    bool config_loaded_from_file = false;
};

enum class RuntimeRequirement {
    None,
    Optional,
    Required,
};

RuntimeRequirement classify_runtime_requirement(const std::vector<std::string>& args);
CliRuntime default_cli_runtime();
GlobalOptions parse_global_options(const std::vector<std::string>& args);
CliRuntime load_cli_runtime(const std::filesystem::path& config_path);
void print_help(const Reporter& reporter);
int run_command(
    const std::vector<std::string>& args,
    const CliRuntime& runtime,
    Reporter& reporter
);

}  // namespace oceandl
