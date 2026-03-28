# oceandl

`oceandl` sekarang ditulis ulang sebagai CLI C++ untuk mengunduh dataset NetCDF dari NOAA PSL. Struktur project tetap memakai dataset registry dan provider abstraction, tetapi runtime utama sudah berpindah ke `libcurl` untuk transfer HTTP dan `tomlplusplus` untuk config.

## Fitur

- CLI native C++ dengan build `CMake`
- Dataset registry terpisah dari provider implementation
- Provider abstraction untuk multi-source di masa depan
- Download dataset `per_year` dan `single_file`
- Default dataset `oisst`
- Default output root `~/data/oceandl`
- `oceandl --version`, `providers`, `datasets`, `info`, dan `download`
- `oceandl --help` dan `<command> --help`
- Resume dari file `.part` jika server mendukung `Range`
- Fallback aman ke download ulang penuh jika resume tidak tersedia
- Skip file final yang sudah valid
- Validasi ringan berbasis ukuran file dan signature NetCDF/HDF5
- Config TOML sederhana
- Mode `--verbose` dan `--quiet`

Dataset bawaan:

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

Dependency yang diasumsikan tersedia di sistem:

- CMake
- compiler C++20
- `libcurl`
- `fmt`
- `tomlplusplus`

Build lokal:

```bash
cmake -S . -B build
cmake --build build
```

Jalankan test:

```bash
ctest --test-dir build --output-on-failure
```

Jalankan binary:

```bash
./build/oceandl --help
./build/oceandl --version
./build/oceandl datasets
./build/oceandl info gpcp
./build/oceandl download gpcp
```

## Command

Lihat provider:

```bash
./build/oceandl providers
```

Lihat dataset:

```bash
./build/oceandl datasets
```

Lihat metadata dataset:

```bash
./build/oceandl info oisst
./build/oceandl info gpcp
./build/oceandl info air
```

Download dataset default dari config:

```bash
./build/oceandl download --start-year 2024 --end-year 2025
```

Download dataset eksplisit:

```bash
./build/oceandl download oisst --start-year 2024 --end-year 2025 --output-dir data
```

Download dataset single-file:

```bash
./build/oceandl download gpcp
./build/oceandl download air
```

Masih mendukung alias lama:

```bash
./build/oceandl download --dataset oisst --start-year 2024 --end-year 2025
```

Paksa unduh ulang file final valid:

```bash
./build/oceandl download oisst --start-year 2024 --end-year 2025 --overwrite
```

Matikan resume:

```bash
./build/oceandl download oisst --start-year 2024 --end-year 2025 --no-resume
```

Atur timeout, chunk size, dan retry:

```bash
./build/oceandl download \
  oisst \
  --start-year 2020 \
  --end-year 2022 \
  --timeout 90 \
  --chunk-size 1048576 \
  --retries 5
```

`chunk_size` dipakai sebagai ukuran buffer receive yang diminta ke `libcurl`.

Mode output:

```bash
./build/oceandl --verbose download oisst --start-year 2024 --end-year 2024
./build/oceandl --quiet download gpcp
```

## Config file

Lokasi default:

```text
~/.config/oceandl/config.toml
```

Contoh:

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

Semua flag CLI tetap meng-override config file.

## Struktur output

```text
~/data/oceandl/
  oisst/
    sst.day.mean.2024.nc
  gpcp/
    precip.mon.mean.nc
  air/
    air.mon.mean.nc
```

## Struktur source

- `cpp/src/cli.cpp`
  Parsing CLI dan wiring aplikasi.
- `cpp/src/downloader.cpp`
  Core downloader untuk retry, resume, validasi, dan summary.
- `cpp/src/catalog.cpp`
  Dataset registry dan metadata bawaan.
- `cpp/src/providers/`
  Provider abstraction dan implementasi NOAA PSL.
- `cpp/src/http_client.cpp`
  HTTP transport berbasis `libcurl`.
- `cpp/src/config.cpp`
  Loader config TOML.
- `cpp/src/validation.cpp`
  Validasi NetCDF ringan.

## Menambah dataset baru

1. Tambahkan metadata dataset baru di `cpp/src/catalog.cpp`.
2. Arahkan `provider_key` ke provider yang relevan.
3. Pilih `FileMode::PerYear` atau `FileMode::SingleFile`.
4. Jika URL pattern masih cocok dengan provider yang ada, tidak perlu mengubah CLI/downloader.
5. Jika source baru butuh perilaku khusus, tambahkan provider baru di `cpp/src/providers/` lalu registrasikan di `cpp/src/providers/registry.cpp`.

## License

MIT. Lihat file `LICENSE`.
