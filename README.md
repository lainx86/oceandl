# oceandl

`oceandl` is a lightweight C++ CLI for downloading ocean/climate NetCDF datasets from NOAA PSL. The project is still in alpha: the current focus is safe download flow, practical resume behavior, and a codebase that stays simple to extend.

## Alpha status

`oceandl` should currently be read as:

- alpha
- source-first
- intended for technical early adopters and contributors

What that means in practice:

- The recommended installation path today is Arch Linux via AUR on Arch-based systems, or the Windows `x64` portable archive from GitHub Releases on Windows.
- GitHub Releases may publish CI-built archives for convenience, but they are still alpha artifacts, not a promise of "stable binary-first" support.
- Today that public release path is intentionally narrow: the formal source archive used by AUR, plus maintainer-built Linux `x64` and Windows `x64` portable archives.
- The only first-stage package-manager target maintained in this repository today is the Arch `makepkg` package spec described below.
- Winget is intentionally not published yet; Windows starts with manual download from GitHub Releases first.
- Other operating systems and package ecosystems are not maintainer-owned targets right now.
- Backward compatibility and platform support should be treated as improving, not frozen.

## Features

- Native C++ CLI with a `CMake` build
- Dataset registry separated from provider implementation
- Simple provider abstraction, currently with one built-in provider: NOAA PSL
- `per_year` and `single_file` dataset downloads
- Default dataset `oisst`
- Platform-aware default output root (`~/data/oceandl` on Linux, `%LOCALAPPDATA%\\oceandl\\data` on Windows)
- `oceandl --version`, `providers`, `datasets`, `info`, and `download`
- `oceandl --help` and `<command> --help`
- Resume from `.part` files when the server supports `Range`
- Safe fallback to a full re-download when resume is unavailable
- Skip valid final files
- Lightweight validation based on file size and NetCDF/HDF5 signatures
- Simple TOML config with strict type validation
- `--verbose` and `--quiet` modes
- `help`, `datasets`, `providers`, and `info` still work even when the local config is broken
- Safe retry policy with bounded exponential backoff

Built-in datasets:

- `oisst` - NOAA OISST Daily Mean
- `gpcp` - GPCP Monthly Precipitation
- `air` - NCEP Reanalysis Air Temperature
- `mslp` - NCEP Reanalysis Mean Sea Level Pressure
- `uwnd_surface` - NCEP Reanalysis Surface Zonal Wind
- `vwnd_surface` - NCEP Reanalysis Surface Meridional Wind
- `rhum_surface` - NCEP Reanalysis Surface Relative Humidity
- `pr_wtr` - NCEP Reanalysis Precipitable Water
- `hgt_pressure` - NCEP Reanalysis 2 Geopotential Height
- `omega_pressure` - NCEP Reanalysis 2 Vertical Velocity

## Install on Arch / AUR

For Arch users, this is the default install path.

Install with an AUR helper:

```bash
yay -S oceandl
```

Or install manually from the AUR package repo:

```bash
git clone https://aur.archlinux.org/oceandl.git
cd oceandl
makepkg -si
```

If your goal is simply to use the tool, you can stop there. You do not need to build `oceandl` manually from source first.

## Install on Windows from GitHub Releases

Windows support target for end users is currently:

- `x64`
- portable zip archive with a self-contained `oceandl.exe`
- manual download from GitHub Releases
- not Winget yet

Download `oceandl-windows-x64.zip` from the latest GitHub Release, then extract it somewhere stable such as:

```text
%LOCALAPPDATA%\Programs\oceandl
```

This is a CLI program. Do not expect a useful result from double-clicking `oceandl.exe` in Explorer.
Open PowerShell or Windows Terminal in the extracted folder and run it from there.

Run it like this:

```powershell
.\oceandl-windows-x64\bin\oceandl.exe --help
.\oceandl-windows-x64\bin\oceandl.exe datasets
```

Optional: add the extracted `bin\` directory to `PATH`, then you can run:

```powershell
oceandl.exe --help
oceandl.exe download --help
```

If your goal is simply to use the tool, you do not need Visual Studio, CMake, or a manual source build.

## Quick start

Typical first commands after install:

```bash
oceandl --help
oceandl datasets
oceandl info oisst
oceandl download oisst --start-year 2024 --end-year 2024
```

That will typically create output under:

```text
~/data/oceandl/
```

On Windows, the equivalent default data root is:

```text
%LOCALAPPDATA%\oceandl\data
```

## Command-line usage

Examples below assume `oceandl` or `oceandl.exe` is already on your `PATH`.
On Windows portable releases, either run `bin\\oceandl.exe` directly or add `bin\\` to `PATH`.
If you are running from the build tree instead, replace `oceandl` with `./build/oceandl` on Linux or `.\build\Release\oceandl.exe` on Windows.

### Command reference

| Command | Function | Use when |
| --- | --- | --- |
| `oceandl --help` | Show general CLI help | You want the top-level command list and global flags |
| `oceandl --version` | Print the current binary version | You want to confirm the installed version |
| `oceandl providers` | List available data providers | You want to know which backend serves the datasets |
| `oceandl datasets` | List the built-in dataset catalog | You want available dataset IDs before downloading |
| `oceandl info <dataset>` | Show detailed metadata for one dataset | You want mode, file naming, base URL, and year support |
| `oceandl download [dataset]` | Download one dataset | You want to fetch files using config defaults plus CLI overrides |
| `oceandl help <command>` | Show help for one command | You want command-specific syntax and options |

### Global flags

These flags must appear before the command name:

| Flag | Function |
| --- | --- |
| `--config PATH` | Load a specific `config.toml` instead of the default path |
| `--verbose` | Print more progress and process detail |
| `--quiet` | Hide non-critical output |
| `--help`, `-h` | Show top-level help |

Examples:

```bash
oceandl --help
oceandl --version
oceandl --config ./config.toml datasets
oceandl --verbose download oisst --start-year 2024 --end-year 2024
```

### Typical workflow

Inspect what the tool knows about providers and datasets:

```bash
oceandl providers
oceandl datasets
oceandl info oisst
oceandl info gpcp
```

Download a per-year dataset:

```bash
oceandl download oisst --start-year 2024 --end-year 2025
```

Download a single-file dataset:

```bash
oceandl download gpcp
oceandl download air
```

Use the default dataset from config instead of naming one on the command line:

```bash
oceandl download --start-year 2024 --end-year 2025
```

Send output to a specific directory:

```bash
oceandl download oisst --start-year 2024 --end-year 2025 --output-dir ./data
```

Ask the tool for command-specific help:

```bash
oceandl help download
oceandl help info
```

### Download command details

Command form:

```bash
oceandl download [dataset] [options]
```

Dataset selection:

- Prefer the positional dataset argument: `oceandl download oisst ...`
- The legacy alias `--dataset ID` is still supported: `oceandl download --dataset oisst ...`
- If you omit the dataset entirely, `oceandl` uses `default_dataset` from the config file

Dataset mode rules:

- Per-year datasets such as `oisst` require both `--start-year` and `--end-year`
- Single-file datasets such as `gpcp`, `air`, and `mslp` do not accept year flags

Important download options:

| Option | Function | Notes |
| --- | --- | --- |
| `--start-year YEAR` | Start year for a per-year dataset | Must be used together with `--end-year` |
| `--end-year YEAR` | End year for a per-year dataset | Must be used together with `--start-year` |
| `--output-dir PATH` | Override the output root for this run | Default comes from config |
| `--overwrite` | Re-download a file even if the final file is already valid | Use when you want to replace an existing final file |
| `--no-overwrite` | Force overwrite off for this run | Overrides config |
| `--resume` | Allow resume from `.part` files | Enabled by default |
| `--no-resume` | Disable partial resume and start clean | Useful for debugging or when you do not trust partial state |
| `--timeout SECONDS` | Set HTTP request timeout | Must be finite and greater than zero |
| `--chunk-size BYTES` | Set the requested `libcurl` receive buffer size | Minimum `1024`; default `1048576` |
| `--retries N` | Retry transient failures | Allowed range `0..10` |
| `--help`, `-h` | Show help for `download` | Does not start a download |

Examples:

```bash
oceandl download oisst --start-year 2020 --end-year 2022
oceandl download oisst --start-year 2024 --end-year 2025 --overwrite
oceandl download oisst --start-year 2024 --end-year 2025 --no-resume
oceandl download oisst --start-year 2020 --end-year 2022 --timeout 90 --chunk-size 1048576 --retries 5
oceandl --quiet download gpcp
```

`chunk_size` is used as the requested receive buffer size passed to `libcurl`.
`retry_count` is bounded to `0..10` to keep retries predictable and avoid pathological retry loops.

## Config file

Default locations:

```text
Windows : %APPDATA%\oceandl\config.toml
Linux   : ~/.config/oceandl/config.toml
```

Default data/output directory:

```text
Windows : %LOCALAPPDATA%\oceandl\data
Linux   : ~/data/oceandl
```

Example:

```toml
default_dataset = "oisst"
default_output_dir = "~/data/oceandl"
timeout = 60
chunk_size = 1048576
retry_count = 3
overwrite = false
resume = true

[dataset_base_urls]
oisst = "https://downloads.psl.noaa.gov/Datasets/noaa.oisst.v2.highres"
gpcp = "https://downloads.psl.noaa.gov/Datasets/gpcp"
air = "https://downloads.psl.noaa.gov/Datasets/ncep.reanalysis.derived/surface"
mslp = "https://downloads.psl.noaa.gov/Datasets/ncep.reanalysis.derived/surface"
uwnd_surface = "https://downloads.psl.noaa.gov/Datasets/ncep.reanalysis.derived/surface"
vwnd_surface = "https://downloads.psl.noaa.gov/Datasets/ncep.reanalysis.derived/surface"
rhum_surface = "https://downloads.psl.noaa.gov/Datasets/ncep.reanalysis.derived/surface"
pr_wtr = "https://downloads.psl.noaa.gov/Datasets/ncep.reanalysis.derived/surface"
hgt_pressure = "https://downloads.psl.noaa.gov/Datasets/ncep.reanalysis2.derived/pressure"
omega_pressure = "https://downloads.psl.noaa.gov/Datasets/ncep.reanalysis2.derived/pressure"
```

All CLI flags still override the config file.

Notes:

- Invalid config value types now fail fast instead of being silently ignored.
- Unknown config keys are ignored with a warning.
- URLs in `provider_base_urls` and `dataset_base_urls` must be `http://` or `https://`.
- `retry_count` must be an integer in the range `0..10`.

## Reliability policy

- Downloads are only accepted when the final size is verifiable from
  `Content-Length` or `Content-Range`.
- HTTP error responses (`4xx/5xx`) are never persisted into `.part` files.
- `.part` files are removed after integrity/payload validation failures.
- Resume only proceeds when remote identity checks (ETag or Last-Modified) are safe.
- A target-specific `.lock` artifact is temporary. After a successful download, skip, or stale-lock recovery, it is removed automatically.
- If a crash or forced interruption leaves a `.lock` artifact behind, rerun `oceandl`. The tool will either recover the stale lock or report that another process still owns the target.

## Troubleshooting

- `Could not resolve hostname`:
  Check DNS/network connectivity or configure the network/proxy environment correctly.
- `response did not provide a verifiable file size`:
  The remote endpoint did not provide `Content-Length`/`Content-Range`; this is rejected for safety.
- `retry_count must be between 0 and 10`:
  Reduce `retry_count` in CLI flags or `config.toml`.
- `target is already being used by another process`:
  Another `oceandl` process is downloading the same target, or a stale lock still needs recovery; wait for the active process to finish and rerun the command.
- `Nothing useful happens when I double-click oceandl.exe`:
  `oceandl` is a terminal program, not a GUI app. Open PowerShell or Windows Terminal and run `.\bin\oceandl.exe --help` from the extracted folder.
- `Windows protected your PC`:
  This is SmartScreen. Verify the file came from the expected GitHub Release, check `SHA256SUMS`, then use the normal Windows trust flow if you want to run it.
- `'oceandl' is not recognized as an internal or external command`:
  Run `oceandl.exe` from the extracted `bin\` directory directly, or add that directory to `PATH`.

## Output layout

```text
~/data/oceandl/
  oisst/
    sst.day.mean.2024.nc
  gpcp/
    precip.mon.mean.nc
  air/
    air.mon.mean.nc
```

## Packaging and support notes

### Maintained path today

| Maintained path | CI coverage today | Recommended path | Notes |
| --- | --- | --- | --- |
| Arch Linux / Arch-based distributions | configure, build, `ctest`, CLI smoke, strict warnings, hermetic localhost HTTP integration, AUR `makepkg` verification | `yay -S oceandl` or the AUR package repo | GitHub Actions runs these checks inside an Arch Linux container on a GitHub-hosted Linux runner because GitHub does not provide a managed Arch runner. |
| Windows `x64` portable release path | configure, build, `ctest`, CLI smoke, portable release archive verification | download `oceandl-windows-x64.zip` from GitHub Releases | Current Windows target is manual-download portable `x64` only. The maintained release path is a self-contained CLI executable without extra runtime DLL files next to `oceandl.exe`. Winget is intentionally not published yet. |

Other environments may still build from source, but they are not maintainer-gated support targets and should not be described as such in public docs or release notes.

### Package manager path

The first official package-manager target is:

- Arch `makepkg` / AUR-compatible source package

Why this one first:

- Linux is the most thoroughly exercised platform in CI today.
- A single Arch source package is lower-maintenance than trying to support multiple package ecosystems at the same time.
- The package can build from a formal source release asset with a published SHA-256 checksum.

Authoritative files live in:

- `packaging/arch/oceandl/PKGBUILD`
- `packaging/arch/oceandl/.SRCINFO`

Current status:

- the package spec is maintained in this repository,
- the release workflow defines the formal source asset contract `oceandl-src-vX.Y.Z.tar.gz`,
- GitHub Releases currently publish `oceandl-linux-x64.tar.gz`, `oceandl-windows-x64.zip`, the source archive, and `SHA256SUMS`,
- the AUR package `oceandl` is published from that maintained package spec.

Windows note:

- manual GitHub Release download is the intended Windows distribution path for now,
- Winget is deferred until the portable Windows release path has stayed stable across multiple releases.

## Build from source

If you only want to use `oceandl`, you can skip this section. This path is mainly for maintainers, local debugging, and packaging work.

Expected build dependencies:

- CMake
- C++20 compiler
- `libcurl`
- `fmt`
- `tomlplusplus`
- Python 3 if you want the full hermetic localhost integration test on Linux
- for Windows source builds, a local `vcpkg` checkout plus the repo `vcpkg.json` manifest

Maintained bootstrap example (Arch Linux / Arch-based):

```bash
sudo pacman -Syu --needed \
  base-devel \
  cmake \
  ninja \
  curl \
  fmt \
  tomlplusplus \
  python
```

Windows source-build prerequisites:

- Visual Studio 2022 or Build Tools 2022 with MSVC `x64`
- a local `vcpkg` checkout

Windows configure/build/test example:

```powershell
$env:VCPKG_ROOT = "C:\src\vcpkg"
if (-not (Test-Path "$env:VCPKG_ROOT\vcpkg.exe")) {
  & "$env:VCPKG_ROOT\bootstrap-vcpkg.bat" -disableMetrics
}

cmake -S . -B build -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake" `
  -DVCPKG_TARGET_TRIPLET=x64-windows-static
cmake --build build --config Release --parallel
ctest --test-dir build --build-config Release --output-on-failure
```

Linux build and test:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Linux install example:

```bash
cmake --install build --prefix /tmp/oceandl-install
```

Run from the build tree:

```bash
./build/oceandl --help
./build/oceandl datasets
./build/oceandl info gpcp
./build/oceandl download gpcp
```

Windows run-from-build-tree example:

```powershell
.\build\Release\oceandl.exe --help
.\build\Release\oceandl.exe datasets
.\build\Release\oceandl.exe info gpcp
.\build\Release\oceandl.exe download gpcp
```

## Source layout

- `cpp/src/cli.cpp`
  CLI entry point and runtime/config loading policy.
- `cpp/src/downloader.cpp`
  High-level download orchestration.
- `cpp/src/catalog.cpp`
  Dataset registry and dataset URL composition from config/provider data.
- `cpp/src/builtin_datasets.cpp`
  The built-in dataset catalog shipped with the binary.
- `cpp/src/providers/`
  Provider abstraction and the NOAA PSL implementation.
- `cpp/src/http_client.cpp`
  `libcurl`-based HTTP transport.
- `cpp/src/config.cpp`
  TOML config loader.
- `cpp/src/validation.cpp`
  Lightweight NetCDF validation.

## Release and distribution

- Public release posture remains alpha and source-first.
- Linux CI builds and tests the maintained Arch path inside an Arch Linux container on a GitHub-hosted Linux runner.
- Windows CI builds and tests the maintained Windows `x64` path on `windows-latest`.
- Tagging `v*` triggers the release workflow that builds the Linux and Windows release archives, smoke-tests the extracted artifacts, and uploads them to GitHub Releases.
- The maintained Windows release path now targets a self-contained `oceandl.exe`, not a release folder that depends on extra runtime DLL files next to the executable.
- Produced artifacts currently include `oceandl-linux-x64.tar.gz`, `oceandl-windows-x64.zip`, the formal source archive `oceandl-src-vX.Y.Z.tar.gz`, and a `SHA256SUMS` file for integrity verification.
- Current recommendation:
  - prefer AUR on Arch-based systems,
  - prefer the Windows portable release zip on Windows,
  - treat GitHub Release archives as convenience artifacts for the currently published Linux and Windows release paths,
  - treat the in-repo Arch `makepkg` package spec as the first official package-manager target,
  - do not treat the project as a stable binary-first or broad package-manager distribution yet.
- Winget is intentionally not published yet; the prerequisite is a stable Windows portable release path first.
- Detached release signatures are not published yet; SHA-256 checksums are the current baseline until maintainer-managed signing keys are in place.

Checksum verification examples:

```bash
sha256sum -c SHA256SUMS
```

## Security reporting

- Do not open public issues for suspected vulnerabilities.
- Report security findings privately to `febysyarief.dev@gmail.com`.
- See [SECURITY.md](SECURITY.md) for the reporting format and response targets.

## Open-source project files

- [CONTRIBUTING.md](CONTRIBUTING.md)
- [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md)
- [SECURITY.md](SECURITY.md)
- [CHANGELOG.md](CHANGELOG.md)
- [RELEASING.md](RELEASING.md)

## Adding a new dataset

1. Add the new dataset metadata in `cpp/src/builtin_datasets.cpp`.
2. Point `provider_key` to the relevant provider.
3. Choose `FileMode::PerYear` or `FileMode::SingleFile`.
4. If the URL pattern still matches an existing provider, no CLI/downloader changes are needed.
5. If the new source needs custom behavior, add a new provider in `cpp/src/providers/` and register it in `cpp/src/providers/registry.cpp`.

## License

MIT. See `LICENSE`.
