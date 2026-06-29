#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace oceandl {

struct ProcessCommand {
    std::filesystem::path executable;
    std::vector<std::string> arguments;
};

struct ProcessResult {
    int exit_code = 1;
    bool started = false;
    std::string error_message;
    std::string stdout_text;
    std::string stderr_text;
};

ProcessResult run_process_capture(const ProcessCommand& command);
ProcessResult run_process_inherit(const ProcessCommand& command);

}  // namespace oceandl
