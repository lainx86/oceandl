# oceandl

`oceandl` is a lightweight C++ CLI for downloading ocean/climate NetCDF datasets from NOAA PSL. The project is still in alpha: the current focus is safe download flow, practical resume behavior, and a codebase that stays simple to extend.

## Alpha status

`oceandl` should currently be read as:

- alpha
- source-first
- intended for technical early adopters and contributors

What that means in practice:

- The recommended installation path today is still building from source on one of the CI-covered platforms.
- GitHub Releases may publish CI-built archives for convenience, but they are still alpha artifacts, not a promise of "stable binary-first" support.
- Today that public release path is intentionally narrow: Linux `x64` archives and the formal source archive are the maintained release artifacts.
- Package-manager distribution is still limited. There is no official Homebrew formula, Scoop/Winget package, or distro-hosted package feed yet.
- The only first-stage package-manager target maintained in this repository today is the Arch `makepkg` package spec described below.
- Backward compatibility and platform support should be treated as improving, not frozen.

## Features

- Native C++ CLI with a `CMake` build
- Dataset registry separated from provider implementation
- Simple provider abstraction, currently with one built-in provider: NOAA PSL
- `per_year` and `single_file` dataset downloads
- Default dataset `oisst`
- Platform-aware default output root (`~/data/oceandl` on Linux)
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

## Support matrix

The table below describes the platforms that are exercised in CI today and how users are expected to approach them.

| Platform | CI coverage today | Recommended path | Notes |
| --- | --- | --- | --- |
| Linux (`ubuntu-latest` reference in CI) | configure, build, `ctest`, CLI smoke, strict warnings, hermetic localhost HTTP integration | source build from distro packages | Most thoroughly exercised path today and the only platform with maintainer-owned release archives right now. |
| macOS (`macos-latest` reference in CI) | configure, build, `ctest`, CLI smoke | source build from Homebrew packages | CI-covered source-build path; no maintainer-owned release archive is published today. |
| Windows (`windows-latest` with MSVC + `vcpkg`) | configure, build, `ctest`, CLI smoke | source build from Visual Studio/Build Tools + `vcpkg` | CI-covered source-build path; no maintainer-owned release archive is published today. |

Only the CI-covered platforms above should be described as supported in public docs or release notes.

## Install dependencies

Expected build dependencies:

- CMake
- C++20 compiler
- `libcurl`
- `fmt`
- `tomlplusplus`

For Windows and reproducible dependency bootstrap, this repo also ships `vcpkg.json`.

Linux example (`ubuntu-latest` / Debian / Ubuntu, same package bootstrap used in CI):

```bash
sudo apt-get update
sudo apt-get install -y \
  build-essential \
  cmake \
  ninja-build \
  libcurl4-openssl-dev \
  libfmt-dev \
  libtomlplusplus-dev
```

Linux note for Arch-like distributions:

```bash
sudo pacman -S --needed base-devel cmake ninja curl fmt tomlplusplus
```

Package names differ across Linux distributions. If you are not on Debian/Ubuntu or Arch-like Linux, install the equivalent development packages for `libcurl`, `fmt`, and `tomlplusplus`, then use the source-build commands below.

macOS example (Homebrew):

```bash
brew update
brew install cmake ninja curl fmt tomlplusplus
```

Windows example (Developer PowerShell for Visual Studio 2022 or Build Tools 2022):

Prerequisites:

- MSVC toolchain with "Desktop development with C++"
- a local `vcpkg` checkout

```powershell
$env:VCPKG_INSTALLATION_ROOT = "C:\src\vcpkg"
& "$env:VCPKG_INSTALLATION_ROOT\vcpkg.exe" install `
  curl:x64-windows `
  fmt:x64-windows `
  tomlplusplus:x64-windows
```

## Build from source

Linux/macOS:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Windows PowerShell:

```powershell
cmake -S . -B build `
  -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_INSTALLATION_ROOT\scripts\buildsystems\vcpkg.cmake"
cmake --build build --config Release --parallel
ctest --test-dir build --build-config Release --output-on-failure
```

Install the binary to a custom prefix (Linux/macOS):

```bash
cmake --install build --prefix /tmp/oceandl-install
```

Windows install example:

```powershell
cmake --install build --config Release --prefix "$env:TEMP\oceandl-install"
```

Run the binary:

```bash
./build/oceandl --help
./build/oceandl --version
./build/oceandl datasets
./build/oceandl info gpcp
./build/oceandl download gpcp
```

## Package manager path

The first official package-manager target is:

- Arch `makepkg` / AUR-compatible source package

Why this one first:

- Linux is the most thoroughly exercised platform in CI today.
- A single source package is lower-maintenance than trying to launch Homebrew plus a Windows ecosystem at the same time.
- The package can build from a formal source release asset with a published SHA-256 checksum.

Authoritative files live in:

- `packaging/arch/oceandl/PKGBUILD`
- `packaging/arch/oceandl/.SRCINFO`

Current status:

- the package spec is maintained in this repository,
- the release workflow now defines the formal source asset contract `oceandl-src-vX.Y.Z.tar.gz`,
- GitHub Releases currently publish `oceandl-linux-x64.tar.gz` plus the source archive and `SHA256SUMS`,
- the AUR package `oceandl` is published from that maintained package spec.

Install from the published AUR package with an AUR helper:

```bash
yay -S oceandl
```

Or build it manually from the AUR package repo:

```bash
git clone https://aur.archlinux.org/oceandl.git
cd oceandl
makepkg -si
```

For now, source build remains the default recommendation for most users.

## Commands

List providers:

```bash
./build/oceandl providers
```

List datasets:

```bash
./build/oceandl datasets
```

Show dataset metadata:

```bash
./build/oceandl info oisst
./build/oceandl info gpcp
./build/oceandl info air
```

Download the default dataset from config:

```bash
./build/oceandl download --start-year 2024 --end-year 2025
```

Download an explicit dataset:

```bash
./build/oceandl download oisst --start-year 2024 --end-year 2025 --output-dir data
```

Download a single-file dataset:

```bash
./build/oceandl download gpcp
./build/oceandl download air
```

The legacy alias is still supported:

```bash
./build/oceandl download --dataset oisst --start-year 2024 --end-year 2025
```

Force a re-download of a valid final file:

```bash
./build/oceandl download oisst --start-year 2024 --end-year 2025 --overwrite
```

Disable resume:

```bash
./build/oceandl download oisst --start-year 2024 --end-year 2025 --no-resume
```

Set timeout, chunk size, and retry count:

```bash
./build/oceandl download \
  oisst \
  --start-year 2020 \
  --end-year 2022 \
  --timeout 90 \
  --chunk-size 1048576 \
  --retries 5
```

`chunk_size` is used as the requested receive buffer size passed to `libcurl`.
`retry_count` is bounded to `0..10` to keep retries predictable and avoid pathological retry loops.

Output modes:

```bash
./build/oceandl --verbose download oisst --start-year 2024 --end-year 2024
./build/oceandl --quiet download gpcp
```

## Config file

Default locations:

```text
Linux   : ~/.config/oceandl/config.toml
macOS   : ~/Library/Application Support/oceandl/config.toml
Windows : %APPDATA%\oceandl\config.toml
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
- CI builds and tests on Linux, macOS, and Windows.
- Tagging `v*` triggers the release workflow that builds the Linux release archive, smoke-tests the extracted artifact, and uploads it to GitHub Releases.
- Produced artifacts currently include `oceandl-linux-x64.tar.gz`, the formal source archive `oceandl-src-vX.Y.Z.tar.gz`, and a `SHA256SUMS` file for integrity verification.
- Current recommendation:
  - prefer the source-build path above for the most predictable setup,
  - treat GitHub Release archives as convenience artifacts for the currently published Linux release path,
  - treat the in-repo Arch `makepkg` package spec as the first official package-manager target,
  - do not treat the project as a stable binary-first or broad package-manager distribution yet.
- Detached release signatures are not published yet; SHA-256 checksums are the current baseline until maintainer-managed signing keys are in place.

Checksum verification examples:

```bash
sha256sum -c SHA256SUMS
```

```powershell
$expected = (Select-String 'oceandl-linux-x64.tar.gz' .\SHA256SUMS).ToString().Split(' ')[0]
$actual = (Get-FileHash .\oceandl-linux-x64.tar.gz -Algorithm SHA256).Hash.ToLower()
if ($actual -ne $expected) { throw "checksum mismatch" }
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
