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

#ifndef _WIN32
#include <fcntl.h>
#include <signal.h>
#include <sys/file.h>
#include <unistd.h>
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
            "target sedang dipakai proses lain: {} (pid {})",
            target.file_name,
            *owner_pid
        );
    }
    return fmt::format("target sedang dipakai proses lain: {}", target.file_name);
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

#ifndef _WIN32
int current_process_id() {
    return static_cast<int>(::getpid());
}

bool process_is_alive(int pid) {
    if (pid <= 0) {
        return false;
    }
    if (::kill(static_cast<pid_t>(pid), 0) == 0) {
        return true;
    }
    return errno == EPERM;
}

std::optional<std::string> current_executable_basename() {
    std::array<char, 4096> buffer{};
    const auto bytes = ::readlink("/proc/self/exe", buffer.data(), buffer.size() - 1);
    if (bytes <= 0) {
        return std::nullopt;
    }
    buffer[static_cast<std::size_t>(bytes)] = '\0';
    return std::filesystem::path(buffer.data()).filename().string();
}

bool has_running_peer_process() {
    const auto executable_name = current_executable_basename();
    if (!executable_name.has_value()) {
        return true;
    }

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
}

void write_all(int fd, std::string_view payload) {
    std::size_t offset = 0;
    while (offset < payload.size()) {
        const auto written = ::write(
            fd,
            payload.data() + offset,
            payload.size() - offset
        );
        if (written < 0) {
            if (errno == EINTR) {
                continue;
            }
            throw std::runtime_error("gagal menulis metadata lock.");
        }
        offset += static_cast<std::size_t>(written);
    }
}

void write_lock_owner_metadata(int fd) {
    if (::ftruncate(fd, 0) != 0) {
        throw std::runtime_error("gagal mereset metadata lock.");
    }
    if (::lseek(fd, 0, SEEK_SET) < 0) {
        throw std::runtime_error("gagal memposisikan metadata lock.");
    }
    write_all(fd, fmt::format("pid {}\n", current_process_id()));
}
#endif

void recover_legacy_lock_directory(const DownloadTarget& target, const Reporter& reporter) {
    const auto lock_path = target.lock_path();
    std::error_code error;
    if (!std::filesystem::is_directory(lock_path, error) || error) {
        return;
    }

    const auto owner_pid = load_lock_owner_pid(lock_path / "owner");
#ifndef _WIN32
    if (owner_pid.has_value() && process_is_alive(*owner_pid)) {
        throw DownloadLockError(lock_conflict_message(target, owner_pid));
    }
#endif

    const bool looks_recent =
        path_was_touched_recently(lock_path, kLegacyLockGracePeriod)
        || path_was_touched_recently(target.temp_path(), kLegacyLockGracePeriod)
        || path_was_touched_recently(target.temp_metadata_path(), kLegacyLockGracePeriod);
    if (
        !owner_pid.has_value() && looks_recent
#ifndef _WIN32
        && has_running_peer_process()
#endif
    ) {
        throw DownloadLockError(lock_conflict_message(target));
    }

    std::filesystem::remove_all(lock_path, error);
    if (error || std::filesystem::exists(lock_path)) {
        throw std::runtime_error(
            fmt::format("gagal membersihkan stale lock lama: {}", target.file_name)
        );
    }

    reporter.warning(fmt::format("Menghapus stale lock lama {}", target.file_name));
}

}  // namespace

TargetFileLock::TargetFileLock(const DownloadTarget& target, const Reporter& reporter)
    : lock_path_(target.lock_path()) {
    recover_legacy_lock_directory(target, reporter);

#ifndef _WIN32
    fd_ = ::open(lock_path_.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0600);
    if (fd_ < 0) {
        throw std::runtime_error(
            fmt::format(
                "gagal membuka lock file {}: {}",
                target.file_name,
                std::strerror(errno)
            )
        );
    }

    if (::flock(fd_, LOCK_EX | LOCK_NB) != 0) {
        const int lock_error = errno;
        ::close(fd_);
        fd_ = -1;
        if (lock_error == EWOULDBLOCK || lock_error == EAGAIN) {
            throw DownloadLockError(
                lock_conflict_message(target, load_lock_owner_pid(lock_path_))
            );
        }
        throw std::runtime_error(
            fmt::format(
                "gagal mengunci target {}: {}",
                target.file_name,
                std::strerror(lock_error)
            )
        );
    }

    try {
        write_lock_owner_metadata(fd_);
    } catch (...) {
        ::close(fd_);
        fd_ = -1;
        throw;
    }
#else
    std::error_code error;
    const bool created = std::filesystem::create_directory(lock_path_, error);
    if (!created || error) {
        throw DownloadLockError(lock_conflict_message(target));
    }
#endif
    acquired_ = true;
}

TargetFileLock::~TargetFileLock() {
    if (!acquired_) {
        return;
    }

#ifndef _WIN32
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
#else
    std::error_code error;
    std::filesystem::remove(lock_path_, error);
#endif
}

}  // namespace oceandl
