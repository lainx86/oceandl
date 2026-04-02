# Tambahkan integration test hermetic di CI untuk jalur libcurl dan HTTP server nyata

## Ringkasan

Test saat ini kuat di level behavior, tetapi mayoritas masih berbasis fake client. Review menilai itu belum cukup untuk menjamin bahwa jalur network stack nyata, packaging, dan interaksi dengan `libcurl` benar-benar aman di CI.

## Kenapa Ini Penting

Release-readiness tidak cukup hanya mengandalkan unit-style tests jika produk utamanya adalah downloader CLI yang bergantung pada I/O nyata. Hermetic integration test memberi bukti bahwa binary berfungsi melawan HTTP server lokal yang dikontrol penuh.

## Dampak ke User

Bug yang hanya muncul saat memakai `libcurl`, filesystem nyata, atau server HTTP sungguhan bisa lolos dari CI lalu baru terlihat di mesin user atau saat binary release dipakai.

## Bukti dari Review

- `review.md:85-87` menyebut tidak ada integration test hermetic di CI yang melewati `libcurl` dan HTTP server nyata.
- `review.md:130` menandai `Resume/retry/integrity behavior` masih `sebagian siap` karena belum ada CI integration test nyata.
- `review.md:168` merekomendasikan integration test hermetic dengan HTTP server lokal di CI untuk jalur `libcurl` nyata.

## Tujuan Perbaikan

CI menjalankan set integration test yang deterministik, tanpa akses internet eksternal, dan menggunakan binary `oceandl` atau komponen produksi melawan HTTP server lokal sehingga jalur `libcurl` tervalidasi.

## Langkah Implementasi

1. Tentukan skenario minimum yang wajib diuji secara hermetic, misalnya download sukses, rerun yang melakukan skip, dan satu skenario resume atau fallback metadata.
2. Siapkan fixture data kecil yang stabil dan mudah diverifikasi untuk dipakai oleh server lokal.
3. Tambahkan helper server HTTP lokal yang hanya bind ke localhost dan tidak membutuhkan akses internet.
4. Buat integration test baru yang menjalankan jalur produksi sesungguhnya, bukan fake client.
5. Hubungkan test tersebut ke `ctest` atau langkah CI terpisah dengan timeout, temp directory, dan port management yang deterministik.
6. Jalankan minimal pada satu platform di CI sebagai gate; perluas ke platform lain setelah stabil.
7. Simpan log yang relevan saat gagal agar debugging tidak bergantung pada reproduksi manual.

## File/Area yang Mungkin Terdampak

- `.github/workflows/ci.yml`
- `cpp/tests/test_main.cpp` atau file test integrasi baru di `cpp/tests/`
- `data/`
- Script helper untuk local HTTP server atau test harness
- `CMakeLists.txt`

## Kriteria Selesai

- [ ] Ada minimal satu integration test hermetic yang memakai `libcurl` dan HTTP server lokal nyata.
- [ ] Test tidak melakukan network call ke internet publik.
- [ ] CI menjalankan integration test tersebut secara otomatis.
- [ ] Failure output cukup informatif untuk debugging tanpa harus menebak-nebak.
- [ ] Skenario yang dipilih mencakup perilaku utama downloader, bukan hanya `--help` atau `--version`.

## Risiko / Catatan

- Test integrasi mudah menjadi flaky jika port, timeout, atau cleanup temp dir tidak dikelola dengan disiplin.
- Jangan membuat fixture terlalu besar; tujuannya coverage perilaku nyata, bukan beban transfer data.
- Bila dijalankan di seluruh matrix terlalu cepat, biaya CI dan noise bisa naik; mulai dari platform paling bernilai dulu.

## Prioritas

P1

## Status

done

## Progress Notes

- Menambahkan integration test hermetic Linux-only di `cpp/tests/hermetic_http_integration.py` yang:
  - menjalankan binary `oceandl` produksi,
  - menjalankan HTTP server lokal pada `127.0.0.1`,
  - memakai fixture NetCDF kecil yang stabil,
  - memverifikasi skenario download sukses, rerun yang melakukan skip, dan resume dari partial file dengan `Range`/`If-Range`.
- Menambahkan registrasi test `oceandl_integration_http_local` di `CMakeLists.txt` via `ctest`, dengan timeout 60 detik dan deteksi `Python3` interpreter pada Linux.
- Memperbarui `CONTRIBUTING.md` dengan cara menjalankan integration test hermetic itu secara lokal.
- Tidak perlu mengubah `.github/workflows/ci.yml` untuk membuatnya jadi gate di CI, karena workflow Linux yang sudah ada memang menjalankan `ctest --output-on-failure`; setelah test baru terdaftar di CMake, ia otomatis ikut dijalankan pada job Linux.
- File yang diubah: `CMakeLists.txt`, `cpp/tests/hermetic_http_integration.py`, `CONTRIBUTING.md`, `problems/problem5.md`.
- Hasil verifikasi:
  - `python3 -m py_compile cpp/tests/hermetic_http_integration.py`
  - `cmake -S . -B build-problem5 -G Ninja -DCMAKE_BUILD_TYPE=Release`
  - `cmake --build build-problem5 --parallel`
  - `ctest --test-dir build-problem5 --output-on-failure`
  - `./build-problem5/oceandl --version`
- Catatan verifikasi lokal:
  - di sandbox sesi ini, bind socket localhost diblokir sehingga integration test perlu diverifikasi sekali di luar sandbox;
  - setelah dijalankan di luar sandbox, `ctest` lulus `2/2` termasuk `oceandl_integration_http_local`.
- Sisa pekerjaan: tidak ada untuk scope problem ini.
