#include "download_lock.hpp"

#include <array>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <tlhelp32.h>
#else
#include <fcntl.h>
#include <signal.h>
#include <sys/file.h>
#include <unistd.h>
#if defined(__APPLE__)
#include <libproc.h>
#include <mach-o/dyld.h>
#endif
#endif

#include <fmt/format.h>

#include "oceandl/utils.hpp"

namespace oceandl {

namespace {

constexpr auto kLegacyLockGracePeriod = std::chrono::minutes(30);

std::optional<int> load_lock_owner_pid(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) {
        return std::nullopt;
    }

    std::string label;
    while (input >> label) {
        if (label == "pid") {
            long long pid = 0;
            if (
                input >> pid && pid > 0
                && pid <= static_cast<long long>(std::numeric_limits<int>::max())
            ) {
                return static_cast<int>(pid);
            }
            return std::nullopt;
        }
        input.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }

    return std::nullopt;
}

std::string lock_conflict_message(
    const DownloadTarget& target,
    std::optional<int> owner_pid = std::nullopt
) {
    if (owner_pid.has_value()) {
        return fmt::format(
            "target is already being used by another process: {} (pid {})",
            target.file_name,
            *owner_pid
        );
    }
    return fmt::format("target is already being used by another process: {}", target.file_name);
}

bool path_was_touched_recently(
    const std::filesystem::path& path,
    std::chrono::minutes grace_period
) {
    std::error_code error;
    const auto last_write = std::filesystem::last_write_time(path, error);
    if (error) {
        return false;
    }

    const auto grace = std::chrono::duration_cast<std::filesystem::file_time_type::duration>(
        grace_period
    );
    return last_write + grace >= std::filesystem::file_time_type::clock::now();
}

int current_process_id() {
#ifdef _WIN32
    return static_cast<int>(::GetCurrentProcessId());
#else
    return static_cast<int>(::getpid());
#endif
}

bool process_is_alive(int pid) {
    if (pid <= 0) {
        return false;
    }
#ifdef _WIN32
    HANDLE process =
        ::OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, static_cast<DWORD>(pid));
    if (process == nullptr) {
        return false;
    }

    const auto wait_result = ::WaitForSingleObject(process, 0);
    ::CloseHandle(process);
    return wait_result == WAIT_TIMEOUT;
#else
    if (::kill(static_cast<pid_t>(pid), 0) == 0) {
        return true;
    }
    return errno == EPERM;
#endif
}

std::optional<std::string> current_executable_basename() {
#ifdef _WIN32
    std::vector<wchar_t> buffer(MAX_PATH, L'\0');
    while (true) {
        const auto copied = ::GetModuleFileNameW(
            nullptr,
            buffer.data(),
            static_cast<DWORD>(buffer.size())
        );
        if (copied == 0) {
            return std::nullopt;
        }
        if (copied < buffer.size() - 1) {
            return std::filesystem::path(buffer.data()).filename().string();
        }
        buffer.resize(buffer.size() * 2, L'\0');
        if (buffer.size() > 32768) {
            return std::nullopt;
        }
    }
#elif defined(__APPLE__)
    uint32_t size = 0;
    (void)_NSGetExecutablePath(nullptr, &size);
    if (size == 0) {
        return std::nullopt;
    }

    std::vector<char> buffer(static_cast<std::size_t>(size) + 1, '\0');
    if (_NSGetExecutablePath(buffer.data(), &size) != 0) {
        return std::nullopt;
    }
    return std::filesystem::path(buffer.data()).filename().string();
#else
    std::array<char, 4096> buffer{};
    const auto bytes = ::readlink("/proc/self/exe", buffer.data(), buffer.size() - 1);
    if (bytes <= 0) {
        return std::nullopt;
    }
    buffer[static_cast<std::size_t>(bytes)] = '\0';
    return std::filesystem::path(buffer.data()).filename().string();
#endif
}

bool has_running_peer_process() {
    const auto executable_name = current_executable_basename();
    if (!executable_name.has_value()) {
        return true;
    }

#ifdef _WIN32
    HANDLE snapshot = ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return true;
    }

    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    if (!::Process32FirstW(snapshot, &entry)) {
        ::CloseHandle(snapshot);
        return true;
    }

    const auto normalized_name = to_lower(*executable_name);
    const auto current_pid = static_cast<DWORD>(current_process_id());
    do {
        if (entry.th32ProcessID == 0 || entry.th32ProcessID == current_pid) {
            continue;
        }

        if (to_lower(std::filesystem::path(entry.szExeFile).filename().string()) == normalized_name) {
            ::CloseHandle(snapshot);
            return true;
        }
    } while (::Process32NextW(snapshot, &entry));

    ::CloseHandle(snapshot);
    return false;
#elif defined(__APPLE__)
    std::vector<pid_t> pids(256, 0);
    int bytes = 0;
    while (true) {
        bytes = ::proc_listpids(
            PROC_ALL_PIDS,
            0,
            pids.data(),
            static_cast<int>(pids.size() * sizeof(pid_t))
        );
        if (bytes < 0) {
            return true;
        }
        if (bytes < static_cast<int>(pids.size() * sizeof(pid_t))) {
            break;
        }
        pids.resize(pids.size() * 2, 0);
    }

    std::array<char, PROC_PIDPATHINFO_MAXSIZE> buffer{};
    const auto pid_count = static_cast<std::size_t>(bytes) / sizeof(pid_t);
    for (std::size_t index = 0; index < pid_count; ++index) {
        const auto pid = pids[index];
        if (
            pid <= 0
            || pid == static_cast<pid_t>(current_process_id())
        ) {
            continue;
        }

        buffer.fill('\0');
        const auto path_bytes = ::proc_pidpath(
            pid,
            buffer.data(),
            static_cast<uint32_t>(buffer.size())
        );
        if (path_bytes <= 0) {
            continue;
        }
        if (static_cast<std::size_t>(path_bytes) < buffer.size()) {
            buffer[static_cast<std::size_t>(path_bytes)] = '\0';
        } else {
            buffer.back() = '\0';
        }

        if (std::filesystem::path(buffer.data()).filename() == *executable_name) {
            return true;
        }
    }

    return false;
#else
    std::error_code error;
    for (const auto& entry : std::filesystem::directory_iterator("/proc", error)) {
        if (error) {
            return true;
        }

        const auto pid = parse_optional_uint(entry.path().filename().string());
        if (
            !pid.has_value()
            || *pid == static_cast<std::uint64_t>(current_process_id())
        ) {
            continue;
        }

        std::array<char, 4096> buffer{};
        const auto executable_path = entry.path() / "exe";
        const auto bytes = ::readlink(
            executable_path.c_str(),
            buffer.data(),
            buffer.size() - 1
        );
        if (bytes <= 0) {
            continue;
        }

        buffer[static_cast<std::size_t>(bytes)] = '\0';
        if (std::filesystem::path(buffer.data()).filename() == *executable_name) {
            return true;
        }
    }

    return false;
#endif
}

void write_lock_owner_metadata(const std::filesystem::path& path) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        throw std::runtime_error("failed to open lock metadata.");
    }
    output << fmt::format("pid {}\n", current_process_id());
    if (!output) {
        throw std::runtime_error("failed to write lock metadata.");
    }
}

std::filesystem::path lock_owner_path(const std::filesystem::path& lock_path) {
    return lock_path / "owner";
}

void recover_existing_lock_directory(const DownloadTarget& target, const Reporter& reporter) {
    const auto lock_path = target.lock_path();
    std::error_code error;
    if (!std::filesystem::is_directory(lock_path, error) || error) {
        return;
    }

    const auto owner_pid = load_lock_owner_pid(lock_owner_path(lock_path));
    if (owner_pid.has_value() && process_is_alive(*owner_pid)) {
        throw DownloadLockError(lock_conflict_message(target, owner_pid));
    }

    const bool looks_recent =
        path_was_touched_recently(lock_path, kLegacyLockGracePeriod)
        || path_was_touched_recently(target.temp_path(), kLegacyLockGracePeriod)
        || path_was_touched_recently(target.temp_metadata_path(), kLegacyLockGracePeriod);
    if (
        !owner_pid.has_value() && looks_recent
        && has_running_peer_process()
    ) {
        throw DownloadLockError(lock_conflict_message(target));
    }

    std::filesystem::remove_all(lock_path, error);
    if (error || std::filesystem::exists(lock_path)) {
        throw std::runtime_error(
            fmt::format("failed to clean up stale legacy lock: {}", target.file_name)
        );
    }

    reporter.warning(fmt::format("Removing stale lock {}", target.file_name));
}

#ifndef _WIN32
void recover_legacy_lock_file(const DownloadTarget& target, const Reporter& reporter) {
    const auto lock_path = target.lock_path();
    std::error_code error;
    if (!std::filesystem::is_regular_file(lock_path, error) || error) {
        return;
    }

    const int fd = ::open(lock_path.c_str(), O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        if (errno == ENOENT) {
            return;
        }
        throw std::runtime_error(
            fmt::format(
                "failed to inspect legacy lock file {}: {}",
                target.file_name,
                std::strerror(errno)
            )
        );
    }

    if (::flock(fd, LOCK_EX | LOCK_NB) != 0) {
        const int lock_error = errno;
        ::close(fd);
        if (lock_error == EWOULDBLOCK || lock_error == EAGAIN) {
            throw DownloadLockError(lock_conflict_message(target, load_lock_owner_pid(lock_path)));
        }
        throw std::runtime_error(
            fmt::format(
                "failed to inspect legacy lock file {}: {}",
                target.file_name,
                std::strerror(lock_error)
            )
        );
    }
    ::close(fd);

    std::filesystem::remove(lock_path, error);
    if (error || std::filesystem::exists(lock_path)) {
        throw std::runtime_error(
            fmt::format("failed to clean up stale legacy lock file: {}", target.file_name)
        );
    }

    reporter.warning(fmt::format("Removing stale legacy lock file {}", target.file_name));
}
#endif

}  // namespace

TargetFileLock::TargetFileLock(const DownloadTarget& target, const Reporter& reporter)
    : lock_path_(target.lock_path()) {
    recover_existing_lock_directory(target, reporter);

#ifndef _WIN32
    recover_legacy_lock_file(target, reporter);
#endif

    std::error_code error;
    const bool created = std::filesystem::create_directory(lock_path_, error);
    if (!created || error) {
        throw DownloadLockError(lock_conflict_message(target));
    }

    try {
        write_lock_owner_metadata(lock_owner_path(lock_path_));
    } catch (...) {
        std::error_code cleanup_error;
        std::filesystem::remove_all(lock_path_, cleanup_error);
        throw;
    }
    acquired_ = true;
}

TargetFileLock::~TargetFileLock() {
    if (!acquired_) {
        return;
    }

    std::error_code error;
    std::filesystem::remove_all(lock_path_, error);
}

}  // namespace oceandl
