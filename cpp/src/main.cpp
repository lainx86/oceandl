#include "oceandl/cli.hpp"

#include <iostream>
#include <string>
#include <vector>

int main(int argc, char** argv) {
    std::vector<std::string> args;
    args.reserve(static_cast<std::size_t>(argc > 0 ? argc - 1 : 0));
    for (int index = 1; index < argc; ++index) {
        args.emplace_back(argv[index]);
    }

    const oceandl::CliApp app;
    return app.run(args, std::cout, std::cerr);
}
