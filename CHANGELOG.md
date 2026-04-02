# Changelog

All notable changes to this project will be documented in this file.

The format is based on Keep a Changelog and this project follows Semantic Versioning.

## [Unreleased]

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
