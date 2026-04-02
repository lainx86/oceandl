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

User-facing bootstrap commands for Linux/macOS/Windows live in `README.md`.
If you change supported platforms, dependency bootstrap, CI runner assumptions, or release positioning, update `README.md` and `RELEASING.md` in the same change so public docs stay aligned with reality.

Build and test:

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

On Linux, if Python 3 is available, `ctest` also runs `oceandl_integration_http_local`.
That test starts a localhost-only HTTP server and exercises the real `oceandl` binary through `libcurl`.
Run it directly with:

```bash
ctest --test-dir build -R oceandl_integration_http_local --output-on-failure
```

Strict warning check used by CI:

```bash
cmake -S . -B build-strict -DCMAKE_BUILD_TYPE=Release -DOCEANDL_STRICT_WARNINGS=ON
cmake --build build-strict --parallel
ctest --test-dir build-strict --output-on-failure
```

`OCEANDL_STRICT_WARNINGS=ON` enables the maintainer warning policy for project targets:

- GCC/Clang/AppleClang: `-Wall -Wextra -Wpedantic -Werror`
- MSVC: `/W4 /WX`

Windows PowerShell:

```powershell
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build --build-config Release --output-on-failure
```

Windows strict warning check:

```powershell
cmake -S . -B build-strict -DCMAKE_BUILD_TYPE=Release -DOCEANDL_STRICT_WARNINGS=ON
cmake --build build-strict --config Release --parallel
ctest --test-dir build-strict --build-config Release --output-on-failure
```

## Security reports

If you believe you found a vulnerability, do **not** open a public issue or pull request.
Report it privately to `febysyarief.dev@gmail.com` and follow the process in `SECURITY.md`.
Security reports are currently triaged by Feby, with an acknowledgement target of 3 business days.

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
