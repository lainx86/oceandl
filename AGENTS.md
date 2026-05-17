# Repository Guidelines

## Project Structure & Module Organization
Core C++ sources live in `cpp/src/`, with public headers in `cpp/include/oceandl/`. The CLI entry point is `cpp/src/main.cpp`; most behavior belongs in `oceandl_lib`, including config, download flow, validation, and provider code under `cpp/src/providers/`. Tests live in `cpp/tests/`: `test_main.cpp` is the main unit-style harness, and `hermetic_http_integration.py` exercises the real binary against a localhost HTTP fixture. Packaging and release helpers live in `packaging/arch/oceandl/`, `scripts/`, and `cmake/`. Treat `build*` directories as generated output, not source.

## Build, Test, and Development Commands
Use the standard CMake flow from the repo root:

- `cmake -S . -B build` configures a local build.
- `cmake --build build --parallel` builds `oceandl` and tests.
- `ctest --test-dir build --output-on-failure` runs the default test suite.
- `ctest --test-dir build -R oceandl_integration_http_local --output-on-failure` runs the Linux-only localhost integration test.
- `cmake -S . -B build-strict -DCMAKE_BUILD_TYPE=Release -DOCEANDL_STRICT_WARNINGS=ON` enables the CI warning policy before `cmake --build` and `ctest`.

## Coding Style & Naming Conventions
Target C++20 and match the existing style: 4-space indentation, braces on the same line, and small focused functions. Use `PascalCase` for types (`CliApp`), `snake_case` for functions and variables (`resolve_dataset_name`), and lowercase dataset/provider IDs (`gpcp`, `psl`). Keep public declarations in `.hpp` headers and implementation in paired `.cpp` files. No formatter is checked in, so preserve the surrounding style and keep strict-warning builds clean.

## Testing Guidelines
Add or update tests for every behavioral change, especially around CLI output, exit codes, resume logic, and validation. In `cpp/tests/test_main.cpp`, follow the existing `bool test_*()` pattern and register new cases in the test list near the end of the file. Prefer hermetic fixtures and localhost integration over live network dependencies.

## Commit & Pull Request Guidelines
Recent commits use short, imperative summaries such as `Improve Windows download fallback and diagnostics` or `Sync AUR package for v0.2.8`. Keep commits focused and descriptive. PRs should fill out the template in `.github/pull_request_template.md`: state the problem and approach, mark changed areas, include validation output, and note release impact. Update `README.md` for user-facing changes and `CHANGELOG.md` for visible behavior changes.

## Security & Configuration Notes
Do not open public issues for vulnerabilities; follow `SECURITY.md` and report privately to `febysyarief.dev@gmail.com`. If you change platform support, dependency bootstrap, or release packaging, keep `README.md` and `RELEASING.md` aligned in the same PR.
