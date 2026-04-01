# oceandl

`oceandl` is a lightweight C++ CLI for downloading ocean/climate NetCDF datasets from NOAA PSL. The project is still in alpha: the current focus is safe download flow, practical resume behavior, and a codebase that stays simple to extend.

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

## Build

Expected system dependencies:

- CMake
- C++20 compiler
- `libcurl`
- `fmt`
- `tomlplusplus`

For Windows CI and reproducible dependency bootstrap, this repo also ships `vcpkg.json`.

Platforms currently covered by CI:

- Linux
- macOS
- Windows

Local build:

```bash
cmake -S . -B build
cmake --build build
```

Install the binary to a custom prefix:

```bash
cmake --install build --prefix /tmp/oceandl-install
```

Windows install example:

```powershell
cmake --install build --config Release --prefix "$env:TEMP\oceandl-install"
```

Run tests:

```bash
ctest --test-dir build --output-on-failure
```

Windows PowerShell example:

```powershell
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build --build-config Release --output-on-failure
```

Run the binary:

```bash
./build/oceandl --help
./build/oceandl --version
./build/oceandl datasets
./build/oceandl info gpcp
./build/oceandl download gpcp
```

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

## Troubleshooting

- `Could not resolve hostname`:
  Check DNS/network connectivity or configure the network/proxy environment correctly.
- `response did not provide a verifiable file size`:
  The remote endpoint did not provide `Content-Length`/`Content-Range`; this is rejected for safety.
- `retry_count must be between 0 and 10`:
  Reduce `retry_count` in CLI flags or `config.toml`.
- `target is already being used by another process`:
  Another `oceandl` process is downloading the same target; wait for it to finish.

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

- CI builds and tests on Linux, macOS, and Windows.
- Tagging `v*` triggers the release workflow that builds platform artifacts and uploads them to GitHub Releases.
- Produced artifacts include the binary plus `README`, `LICENSE`, and `CHANGELOG`.

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
