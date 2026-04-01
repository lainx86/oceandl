# Changelog

All notable changes to this project will be documented in this file.

The format is based on Keep a Changelog and this project follows Semantic Versioning.

## [Unreleased]

### Changed

- Bounded retry policy (`retry_count` now restricted to `0..10`).
- Exponential retry backoff uses overflow-safe arithmetic.
- HTTP `4xx/5xx` response bodies are no longer written to `.part` files.
- Download completion now requires verifiable final size (`Content-Length` or `Content-Range`).

### Added

- Regression tests for:
  - excessive retry count validation
  - `503 -> retry -> 206` resume safety without `.part` corruption
  - rejection when final size cannot be verified
- Windows CI build/test coverage.
- Baseline open-source files (`CONTRIBUTING`, `CODE_OF_CONDUCT`, `SECURITY`, issue/PR templates).

## [0.2.0] - 2026-04-01

### Added

- Initial public alpha feature set for dataset download, resume, and CLI/config support.
