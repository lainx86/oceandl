# Changelog

All notable changes to this project will be documented in this file.

The format is based on Keep a Changelog and this project follows Semantic Versioning.

## [Unreleased]

## [0.2.6] - 2026-04-03

### Added

- `windows-latest` CI coverage for configure, build, `ctest`, and basic CLI smoke checks using MSVC with `vcpkg`.
- Windows release artifact packaging and verification for the portable `oceandl-windows-x64.zip` GitHub Release path.

### Changed

- Lock owner metadata is now written on Windows too, so stale lock recovery and lock-conflict handling no longer rely on weaker Windows-only heuristics.
- User-facing and maintainer docs now describe the Windows `x64` portable release path and the prerequisites that remain before a future Winget rollout.

## [0.2.5] - 2026-04-03

### Changed

- Maintainer-owned CI now runs the supported path inside Arch Linux containers on GitHub-hosted Linux runners, and non-Linux CI jobs were removed from the supported workflow.
- Contributor and user-facing docs now describe the project as Linux/AUR-focused instead of implying active macOS or Windows support.
- `problems/` is now intended to stay local to the maintainer workspace rather than tracked in the repository.
- The AUR verification workflow now reads the project version robustly from `CMakeLists.txt`.
- Release-source and AUR packaging scripts now handle GitHub Actions Arch container constraints correctly by marking the repo as a safe Git directory and running `makepkg` as an unprivileged user.

## [0.2.4] - 2026-04-03

### Changed

- The GitHub `Release` workflow now targets the maintained Linux `x64` release path plus the formal source archive, instead of blocking publication on Windows and macOS artifacts that are not required for AUR distribution.
- Public release docs now state explicitly that maintainer-owned GitHub Release assets are currently Linux `x64` and source-only.

## [0.2.3] - 2026-04-03

### Fixed

- macOS stale-lock recovery no longer relies on Linux-only `/proc`; it now inspects peer `oceandl` processes with `libproc`, which keeps Unix lock recovery behavior consistent across Linux and macOS.
- Windows GitHub Actions jobs now reuse runner-provided `vcpkg` roots when present, bootstrap them when needed, and only fall back to a clean temporary clone when no usable install exists.

## [0.2.2] - 2026-04-03

### Fixed

- GitHub Actions Windows jobs now resolve or bootstrap `vcpkg` instead of assuming `VCPKG_INSTALLATION_ROOT` is pre-set by the runner image.
- Lock-conflict coverage now follows the current lock-directory implementation, which avoids same-process `flock` behavior differences on BSD/macOS and keeps the test portable across CI platforms.

## [0.2.1] - 2026-04-03

### Changed

- Bounded retry policy (`retry_count` now restricted to `0..10`).
- Exponential retry backoff uses overflow-safe arithmetic.
- HTTP `4xx/5xx` response bodies are no longer written to `.part` files.
- Download completion now requires verifiable final size (`Content-Length` or `Content-Range`).
- Unix `.lock` artifacts are now cleaned up after successful download, skip, and stale-lock recovery.
- Release packaging now builds install-tree archives, publishes checksums, and defines a formal source release asset for package-manager consumers.
- Contributor and user-facing docs now describe strict warning checks, support matrix, release posture, and package-manager scope more explicitly.

### Added

- Regression tests for:
  - excessive retry count validation
  - `503 -> retry -> 206` resume safety without `.part` corruption
  - rejection when final size cannot be verified
- Hermetic localhost HTTP integration testing in CI for the real `libcurl` path.
- Maintainer warning policy support plus a CI strict-warnings job.
- Release archive verification scripts and published `SHA256SUMS`.
- Initial Arch `makepkg` / AUR-compatible package spec plus local verification helpers.
- Windows CI build/test coverage.
- Baseline open-source files (`CONTRIBUTING`, `CODE_OF_CONDUCT`, `SECURITY`, issue/PR templates).

## [0.2.0] - 2026-04-01

### Added

- Initial public alpha feature set for dataset download, resume, and CLI/config support.
