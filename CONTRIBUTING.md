# Contributing to oceandl

## Scope

`oceandl` is a C++ CLI for downloading ocean/climate datasets safely.
Contributions should prioritize correctness, safety, and predictable CLI behavior.

## Development setup

Requirements:

- CMake >= 3.24
- C++20 compiler
- libcurl
- fmt
- tomlplusplus

Build and test:

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Windows PowerShell:

```powershell
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build --build-config Release --output-on-failure
```

## Pull request checklist

- Add or update tests for behavioral changes.
- Keep CLI output and exit codes stable unless intentionally changed.
- Avoid introducing new dependencies unless clearly justified.
- Update `README.md` when user-facing behavior changes.
- Add an entry to `CHANGELOG.md` for user-visible changes.

## Code guidelines

- Prefer small, focused changes.
- Keep failure modes explicit and actionable.
- Preserve cross-platform behavior (Linux/macOS/Windows).
- Favor safe defaults over permissive behavior for download integrity.

## Commit and PR style

- Use clear commit messages describing intent and impact.
- In PR description, include:
  - Problem statement
  - Approach
  - Test evidence (`ctest` output or equivalent)
  - Backward-compatibility notes
