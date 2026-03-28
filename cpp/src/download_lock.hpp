#pragma once

#include <stdexcept>

#include "oceandl/models.hpp"
#include "oceandl/reporter.hpp"

namespace oceandl {

class DownloadLockError : public std::runtime_error {
  public:
    using std::runtime_error::runtime_error;
};

class TargetFileLock {
  public:
    TargetFileLock(const DownloadTarget& target, const Reporter& reporter);
    ~TargetFileLock();

    TargetFileLock(const TargetFileLock&) = delete;
    TargetFileLock& operator=(const TargetFileLock&) = delete;

  private:
    std::filesystem::path lock_path_;
    bool acquired_ = false;
#ifndef _WIN32
    int fd_ = -1;
#endif
};

}  // namespace oceandl
