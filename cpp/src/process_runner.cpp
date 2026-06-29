#include "oceandl/process_runner.hpp"

#include <string>
#include <utility>

#ifdef _WIN32
#include <windows.h>

#include <array>
#include <thread>
#else
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace oceandl {

namespace {

#ifdef _WIN32

std::wstring utf8_to_wide(const std::string& value) {
    if (value.empty()) {
        return {};
    }

    int length = MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        value.data(),
        static_cast<int>(value.size()),
        nullptr,
        0
    );
    if (length == 0) {
        length = MultiByteToWideChar(
            CP_ACP,
            0,
            value.data(),
            static_cast<int>(value.size()),
            nullptr,
            0
        );
    }
    if (length == 0) {
        return std::wstring(value.begin(), value.end());
    }

    std::wstring result(static_cast<std::size_t>(length), L'\0');
    const UINT code_page = MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        value.data(),
        static_cast<int>(value.size()),
        result.data(),
        length
    ) != 0 ? CP_UTF8 : CP_ACP;
    (void)MultiByteToWideChar(
        code_page,
        code_page == CP_UTF8 ? MB_ERR_INVALID_CHARS : 0,
        value.data(),
        static_cast<int>(value.size()),
        result.data(),
        length
    );
    return result;
}

std::wstring quote_windows_arg(const std::wstring& arg) {
    if (arg.empty()) {
        return L"\"\"";
    }

    bool needs_quotes = false;
    for (const wchar_t ch : arg) {
        if (ch == L' ' || ch == L'\t' || ch == L'\n' || ch == L'\v' || ch == L'"') {
            needs_quotes = true;
            break;
        }
    }
    if (!needs_quotes) {
        return arg;
    }

    std::wstring result = L"\"";
    std::size_t backslashes = 0;
    for (const wchar_t ch : arg) {
        if (ch == L'\\') {
            ++backslashes;
            continue;
        }
        if (ch == L'"') {
            result.append(backslashes * 2 + 1, L'\\');
            result.push_back(ch);
            backslashes = 0;
            continue;
        }
        result.append(backslashes, L'\\');
        backslashes = 0;
        result.push_back(ch);
    }
    result.append(backslashes * 2, L'\\');
    result.push_back(L'"');
    return result;
}

std::wstring make_windows_command_line(const ProcessCommand& command) {
    std::vector<std::wstring> args;
    args.reserve(command.arguments.size() + 1);
    args.push_back(command.executable.wstring());
    for (const auto& arg : command.arguments) {
        args.push_back(utf8_to_wide(arg));
    }

    std::wstring command_line;
    for (std::size_t index = 0; index < args.size(); ++index) {
        if (index != 0) {
            command_line.push_back(L' ');
        }
        command_line += quote_windows_arg(args[index]);
    }
    return command_line;
}

std::string format_windows_error(DWORD code) {
    LPWSTR buffer = nullptr;
    const DWORD length = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM
            | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        code,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPWSTR>(&buffer),
        0,
        nullptr
    );
    if (length == 0 || buffer == nullptr) {
        return "Windows error " + std::to_string(static_cast<unsigned long>(code));
    }

    std::wstring wide_message(buffer, length);
    LocalFree(buffer);
    std::string message(wide_message.begin(), wide_message.end());
    while (!message.empty() && (message.back() == '\n' || message.back() == '\r')) {
        message.pop_back();
    }
    return message;
}

std::string read_windows_pipe(HANDLE handle) {
    std::string output;
    std::array<char, 4096> buffer{};
    DWORD bytes_read = 0;
    while (ReadFile(
        handle,
        buffer.data(),
        static_cast<DWORD>(buffer.size()),
        &bytes_read,
        nullptr
    ) && bytes_read > 0) {
        output.append(buffer.data(), bytes_read);
    }
    CloseHandle(handle);
    return output;
}

#else

std::vector<char*> make_argv(
    const std::string& executable,
    const std::vector<std::string>& arguments
) {
    std::vector<char*> argv;
    argv.reserve(arguments.size() + 2);
    argv.push_back(const_cast<char*>(executable.c_str()));
    for (const auto& argument : arguments) {
        argv.push_back(const_cast<char*>(argument.c_str()));
    }
    argv.push_back(nullptr);
    return argv;
}

int wait_for_child(pid_t pid) {
    int status = 0;
    while (waitpid(pid, &status, 0) == -1) {
        if (errno != EINTR) {
            return 1;
        }
    }

    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    if (WIFSIGNALED(status)) {
        return 128 + WTERMSIG(status);
    }
    return 1;
}

void close_fd(int& fd) {
    if (fd >= 0) {
        (void)::close(fd);
        fd = -1;
    }
}

void set_nonblocking(int fd) {
    const int flags = ::fcntl(fd, F_GETFL, 0);
    if (flags >= 0) {
        (void)::fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    }
}

bool drain_pipe_output(int fd, std::string& output) {
    char buffer[4096];
    while (true) {
        const auto bytes_read = ::read(fd, buffer, sizeof(buffer));
        if (bytes_read > 0) {
            output.append(buffer, static_cast<std::size_t>(bytes_read));
            continue;
        }
        if (bytes_read == 0) {
            return true;
        }
        if (errno == EINTR) {
            continue;
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return false;
        }
        return true;
    }
}

void write_exec_error(const std::string& executable) {
    const std::string message =
        "failed to start " + executable + ": " + std::strerror(errno) + "\n";
    (void)::write(STDERR_FILENO, message.data(), message.size());
}

#endif

}  // namespace

#ifdef _WIN32

ProcessResult run_process_capture(const ProcessCommand& command) {
    ProcessResult result;
    if (command.executable.empty()) {
        result.error_message = "process executable is empty";
        return result;
    }

    SECURITY_ATTRIBUTES security_attributes{};
    security_attributes.nLength = sizeof(SECURITY_ATTRIBUTES);
    security_attributes.lpSecurityDescriptor = nullptr;
    security_attributes.bInheritHandle = TRUE;

    HANDLE stdout_read = nullptr;
    HANDLE stdout_write = nullptr;
    HANDLE stderr_read = nullptr;
    HANDLE stderr_write = nullptr;
    if (!CreatePipe(&stdout_read, &stdout_write, &security_attributes, 0)
        || !CreatePipe(&stderr_read, &stderr_write, &security_attributes, 0)) {
        result.error_message = format_windows_error(GetLastError());
        if (stdout_read != nullptr) {
            CloseHandle(stdout_read);
        }
        if (stdout_write != nullptr) {
            CloseHandle(stdout_write);
        }
        if (stderr_read != nullptr) {
            CloseHandle(stderr_read);
        }
        if (stderr_write != nullptr) {
            CloseHandle(stderr_write);
        }
        return result;
    }
    SetHandleInformation(stdout_read, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(stderr_read, HANDLE_FLAG_INHERIT, 0);

    auto command_line = make_windows_command_line(command);
    STARTUPINFOW startup_info{};
    startup_info.cb = sizeof(startup_info);
    startup_info.dwFlags = STARTF_USESTDHANDLES;
    startup_info.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    startup_info.hStdOutput = stdout_write;
    startup_info.hStdError = stderr_write;

    PROCESS_INFORMATION process_info{};
    const BOOL created = CreateProcessW(
        nullptr,
        command_line.data(),
        nullptr,
        nullptr,
        TRUE,
        0,
        nullptr,
        nullptr,
        &startup_info,
        &process_info
    );
    CloseHandle(stdout_write);
    CloseHandle(stderr_write);

    if (!created) {
        result.error_message = format_windows_error(GetLastError());
        CloseHandle(stdout_read);
        CloseHandle(stderr_read);
        return result;
    }

    result.started = true;
    std::thread stdout_thread([&]() {
        result.stdout_text = read_windows_pipe(stdout_read);
    });
    std::thread stderr_thread([&]() {
        result.stderr_text = read_windows_pipe(stderr_read);
    });

    WaitForSingleObject(process_info.hProcess, INFINITE);
    DWORD exit_code = 1;
    GetExitCodeProcess(process_info.hProcess, &exit_code);
    stdout_thread.join();
    stderr_thread.join();

    CloseHandle(process_info.hThread);
    CloseHandle(process_info.hProcess);
    result.exit_code = static_cast<int>(exit_code);
    return result;
}

ProcessResult run_process_inherit(const ProcessCommand& command) {
    ProcessResult result;
    if (command.executable.empty()) {
        result.error_message = "process executable is empty";
        return result;
    }

    auto command_line = make_windows_command_line(command);
    STARTUPINFOW startup_info{};
    startup_info.cb = sizeof(startup_info);
    startup_info.dwFlags = STARTF_USESTDHANDLES;
    startup_info.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    startup_info.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    startup_info.hStdError = GetStdHandle(STD_ERROR_HANDLE);

    PROCESS_INFORMATION process_info{};
    const BOOL created = CreateProcessW(
        nullptr,
        command_line.data(),
        nullptr,
        nullptr,
        TRUE,
        0,
        nullptr,
        nullptr,
        &startup_info,
        &process_info
    );
    if (!created) {
        result.error_message = format_windows_error(GetLastError());
        return result;
    }

    result.started = true;
    WaitForSingleObject(process_info.hProcess, INFINITE);
    DWORD exit_code = 1;
    GetExitCodeProcess(process_info.hProcess, &exit_code);
    CloseHandle(process_info.hThread);
    CloseHandle(process_info.hProcess);
    result.exit_code = static_cast<int>(exit_code);
    return result;
}

#else

ProcessResult run_process_capture(const ProcessCommand& command) {
    ProcessResult result;
    if (command.executable.empty()) {
        result.error_message = "process executable is empty";
        return result;
    }

    int stdout_pipe[2] = {-1, -1};
    int stderr_pipe[2] = {-1, -1};
    if (::pipe(stdout_pipe) == -1 || ::pipe(stderr_pipe) == -1) {
        result.error_message = std::strerror(errno);
        close_fd(stdout_pipe[0]);
        close_fd(stdout_pipe[1]);
        close_fd(stderr_pipe[0]);
        close_fd(stderr_pipe[1]);
        return result;
    }

    const auto executable = command.executable.string();
    const pid_t pid = ::fork();
    if (pid == -1) {
        result.error_message = std::strerror(errno);
        close_fd(stdout_pipe[0]);
        close_fd(stdout_pipe[1]);
        close_fd(stderr_pipe[0]);
        close_fd(stderr_pipe[1]);
        return result;
    }

    if (pid == 0) {
        close_fd(stdout_pipe[0]);
        close_fd(stderr_pipe[0]);
        (void)::dup2(stdout_pipe[1], STDOUT_FILENO);
        (void)::dup2(stderr_pipe[1], STDERR_FILENO);
        close_fd(stdout_pipe[1]);
        close_fd(stderr_pipe[1]);

        auto argv = make_argv(executable, command.arguments);
        ::execvp(executable.c_str(), argv.data());
        write_exec_error(executable);
        _exit(127);
    }

    result.started = true;
    close_fd(stdout_pipe[1]);
    close_fd(stderr_pipe[1]);

    int stdout_fd = stdout_pipe[0];
    int stderr_fd = stderr_pipe[0];
    set_nonblocking(stdout_fd);
    set_nonblocking(stderr_fd);
    while (stdout_fd >= 0 || stderr_fd >= 0) {
        pollfd fds[2]{};
        nfds_t count = 0;
        if (stdout_fd >= 0) {
            fds[count].fd = stdout_fd;
            fds[count].events = POLLIN | POLLHUP;
            ++count;
        }
        if (stderr_fd >= 0) {
            fds[count].fd = stderr_fd;
            fds[count].events = POLLIN | POLLHUP;
            ++count;
        }

        const int poll_result = ::poll(fds, count, -1);
        if (poll_result == -1) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }

        for (nfds_t index = 0; index < count; ++index) {
            if ((fds[index].revents & (POLLIN | POLLHUP | POLLERR)) == 0) {
                continue;
            }
            if (fds[index].fd == stdout_fd) {
                if (drain_pipe_output(stdout_fd, result.stdout_text)) {
                    close_fd(stdout_fd);
                }
            } else if (fds[index].fd == stderr_fd) {
                if (drain_pipe_output(stderr_fd, result.stderr_text)) {
                    close_fd(stderr_fd);
                }
            }
        }
    }

    close_fd(stdout_fd);
    close_fd(stderr_fd);
    result.exit_code = wait_for_child(pid);
    return result;
}

ProcessResult run_process_inherit(const ProcessCommand& command) {
    ProcessResult result;
    if (command.executable.empty()) {
        result.error_message = "process executable is empty";
        return result;
    }

    const auto executable = command.executable.string();
    const pid_t pid = ::fork();
    if (pid == -1) {
        result.error_message = std::strerror(errno);
        return result;
    }

    if (pid == 0) {
        auto argv = make_argv(executable, command.arguments);
        ::execvp(executable.c_str(), argv.data());
        write_exec_error(executable);
        _exit(127);
    }

    result.started = true;
    result.exit_code = wait_for_child(pid);
    return result;
}

#endif

}  // namespace oceandl
