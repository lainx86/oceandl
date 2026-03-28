#include "oceandl/cli.hpp"

#include "cli_support.hpp"

#include <filesystem>
#include <iosfwd>
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
        const auto runtime = load_cli_runtime(globals.config_path);
        if (reporter.is_verbose()) {
            if (std::filesystem::exists(globals.config_path)) {
                reporter.detail("Config menggunakan " + globals.config_path.string());
            } else {
                reporter.detail("Config tidak ditemukan, memakai default internal");
            }
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
