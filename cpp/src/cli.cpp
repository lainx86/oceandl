#include "oceandl/cli.hpp"

#include "cli_support.hpp"

#include <iosfwd>
#include <string>
#include <vector>

#include "oceandl/version.hpp"

namespace oceandl {

int CliApp::run(const std::vector<std::string>& args, std::ostream& out, std::ostream& err) const {
    try {
        if (
            args.empty()
            || (
                args.size() == 1
                && (args.front() == "--help" || args.front() == "-h" || args.front() == "help")
            )
        ) {
            Reporter reporter(out, err);
            print_help(reporter);
            return 0;
        }

        const auto globals = parse_global_options(args);
        if (globals.version) {
            out << "oceandl " << kVersion << '\n';
            return 0;
        }

        Reporter reporter(out, err, globals.verbosity);
        CliRuntime runtime = default_cli_runtime();
        const auto runtime_requirement = classify_runtime_requirement(globals.remaining);
        bool used_default_runtime_due_to_config_error = false;

        if (runtime_requirement == RuntimeRequirement::Required) {
            runtime = load_cli_runtime(globals.config_path);
        } else if (runtime_requirement == RuntimeRequirement::Optional) {
            try {
                runtime = load_cli_runtime(globals.config_path);
            } catch (const std::exception& error) {
                used_default_runtime_due_to_config_error = true;
                reporter.warning(
                    "Config is invalid; using built-in metadata instead: "
                    + std::string(error.what())
                );
            }
        }

        if (reporter.is_verbose() && runtime_requirement != RuntimeRequirement::None) {
            if (runtime.config_loaded_from_file) {
                reporter.detail("Using config from " + runtime.config_path.string());
            } else if (used_default_runtime_due_to_config_error) {
                reporter.detail("Ignoring invalid config and using internal defaults");
            } else {
                reporter.detail("Config file not found; using internal defaults");
            }
        }
        for (const auto& warning : runtime.config_warnings) {
            reporter.warning(warning);
        }

        return run_command(globals.remaining, runtime, reporter);
    } catch (const CliError& error) {
        Reporter reporter(out, err);
        reporter.error(error.what());
        return error.exit_code();
    } catch (const std::exception& error) {
        Reporter reporter(out, err);
        reporter.error(error.what());
        return 2;
    }
}

}  // namespace oceandl
