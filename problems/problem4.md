# Rapikan build hygiene sampai lulus strict warnings dan tambah guardrail CI

## Ringkasan

Review menemukan build dengan `-Wall -Wextra -Wpedantic -Werror` gagal pada kode produksi dan test. Ini menandakan codebase belum bersih di bawah toolchain policy yang lazim dipakai maintainer distro atau package maintainer.

## Kenapa Ini Penting

Build hygiene yang buruk menurunkan kepercayaan terhadap kualitas release dan mempersulit distribusi lewat pihak ketiga. Walau belum memblokir fungsi utama hari ini, ini memengaruhi kesiapan distribusi publik dan onboarding kontributor.

## Dampak ke User

User source-build di environment yang lebih ketat bisa menemui warning yang diperlakukan sebagai error. Maintainer distro juga cenderung ragu mengadopsi project yang tidak bersih di warning policy umum.

## Bukti dari Review

- `review.md:20` menyebut strict build gagal karena designated initializer yang tidak menginisialisasi semua field, termasuk di `cpp/src/validation.cpp`, `cpp/src/target_download_executor.cpp`, dan `cpp/tests/test_main.cpp`.
- `review.md:67-69` menandai build hygiene belum kuat untuk maintainer/paket distro.
- `review.md:140` mencatat `Kontributor onboarding` masih sebagian siap karena hygiene build kurang.
- `review.md:167` merekomendasikan merapikan build hygiene dan, bila memungkinkan, menambah job ASan/UBSan di CI.

## Tujuan Perbaikan

Codebase dan test suite lulus pada profil warning ketat yang disepakati, dengan guardrail CI yang mencegah regresi warning masuk kembali.

## Langkah Implementasi

1. Reproduksi build strict warning secara konsisten lewat command atau preset yang terdokumentasi.
2. Perbaiki warning yang disebut review, dimulai dari designated initializer di file produksi dan test yang sudah teridentifikasi.
3. Audit warning lain yang muncul setelah perbaikan awal agar tidak hanya menutup satu gejala.
4. Tentukan policy warning yang masuk akal per compiler dan platform, lalu implementasikan di CMake atau workflow CI.
5. Tambahkan job CI khusus untuk strict warnings agar regresi terlihat jelas pada pull request.
6. Evaluasi penambahan job sanitizer, minimal Linux `ASan`/`UBSan`, bila sinyalnya cukup stabil dan tidak terlalu flakey.
7. Update dokumentasi contributor/build agar maintainer tahu cara menjalankan check yang sama secara lokal.

## File/Area yang Mungkin Terdampak

- `CMakeLists.txt`
- `.github/workflows/ci.yml`
- `cpp/src/validation.cpp`
- `cpp/src/target_download_executor.cpp`
- `cpp/tests/test_main.cpp`
- `CONTRIBUTING.md`

## Kriteria Selesai

- [ ] Ada command atau preset resmi untuk strict warning build.
- [ ] Warning yang disebut review sudah hilang.
- [ ] CI memiliki guardrail untuk strict warnings pada compiler yang dipilih.
- [ ] Regressi warning menyebabkan CI gagal, bukan lolos diam-diam.
- [ ] Dokumentasi contributor menjelaskan cara menjalankan check yang sama secara lokal.

## Risiko / Catatan

- Warning set berbeda antar compiler; jangan membuat policy yang tidak realistis untuk seluruh matrix sekaligus.
- `-Werror` di semua OS/semua compiler bisa terlalu agresif bila tidak dipilah dengan hati-hati.
- Sanitizer job memberi sinyal kuat, tetapi bisa menambah durasi CI dan flakiness bila setup belum rapih.

## Prioritas

P1

## Status

done

## Progress Notes

- Menambahkan opsi CMake `OCEANDL_STRICT_WARNINGS` agar strict warning build bisa dijalankan resmi untuk target project dengan policy `-Wall -Wextra -Wpedantic -Werror` pada GCC/Clang/AppleClang dan `/W4 /WX` pada MSVC.
- Membereskan initializer parsial yang menyebabkan strict build gagal di `cpp/src/validation.cpp`, `cpp/src/target_download_executor.cpp`, dan `cpp/tests/test_main.cpp`.
- Menambahkan helper metadata kecil di test agar setup fixture tetap ringkas tanpa memicu warning missing-field-initializers.
- Menambahkan job CI `strict-warnings` di Linux yang configure, build, dan menjalankan test suite dengan `-DOCEANDL_STRICT_WARNINGS=ON` supaya regresi warning langsung memblokir PR.
- Memperbarui `CONTRIBUTING.md` dengan command lokal untuk menjalankan strict warning check yang sama seperti guardrail CI.
- File yang diubah: `CMakeLists.txt`, `.github/workflows/ci.yml`, `cpp/src/validation.cpp`, `cpp/src/target_download_executor.cpp`, `cpp/tests/test_main.cpp`, `CONTRIBUTING.md`, `problems/problem4.md`.
- Hasil verifikasi:
  - `cmake -S . -B build-strict -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS='-Wall -Wextra -Wpedantic -Werror'`
  - `cmake --build build-strict --parallel`
  - `cmake -S . -B build-ci-strict -G Ninja -DCMAKE_BUILD_TYPE=Release -DOCEANDL_STRICT_WARNINGS=ON`
  - `cmake --build build-ci-strict --parallel`
  - `ctest --test-dir build-ci-strict --output-on-failure`
  - `./build-ci-strict/oceandl --version`
  - `./build-ci-strict/oceandl datasets`
  - `./build-ci-strict/oceandl providers`
  - `./build-ci-strict/oceandl download --help`
- Sisa pekerjaan: tidak ada untuk scope problem ini; job sanitizer sengaja tidak ditambahkan pada perubahan ini agar guardrail strict warnings tetap fokus dan stabil.
