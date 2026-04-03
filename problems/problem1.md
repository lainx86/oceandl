# Perbaiki packaging binary release agar self-contained dan dapat dijalankan di mesin user

## Ringkasan

Workflow GitHub Release saat ini hanya mengarsipkan `oceandl`/`oceandl.exe` dan dokumen. Dari review, binary hasil build masih bergantung pada runtime library eksternal seperti `libcurl`, `fmt`, dan `tomlplusplus`, sehingga artefak release belum aman untuk distribusi publik.

## Kenapa Ini Penting

Ini adalah blocker utama untuk public binary release. Release publik tidak boleh mengandalkan user menebak dependency runtime yang harus dipasang sendiri setelah mengunduh archive.

## Dampak ke User

User bisa mengunduh release, mengekstrak archive, lalu langsung gagal menjalankan binary pada first run karena shared library atau DLL tidak tersedia. Kegagalan seperti ini merusak kepercayaan terhadap project dan membuat binary release tidak berbeda jauh dari source build.

## Bukti dari Review

- `review.md:38-49` menyebut workflow GitHub Release berisiko menghasilkan binary yang tidak jalan di mesin user.
- `review.md:45-47` mencatat bahwa binary Linux ter-link dinamis ke `libcurl`, `libfmt`, dan `libtomlplusplus`, sementara workflow Windows hanya meng-copy `.exe`.
- `review.md:134` menandai `Binary GitHub Release` sebagai `belum siap`.
- `review.md:164` merekomendasikan bundling runtime dependency atau static build per platform.

## Tujuan Perbaikan

Setiap artefak release publik harus self-contained sesuai strategi per platform, sehingga binary hasil ekstraksi dapat dijalankan di mesin user tanpa missing runtime dependency yang tidak didokumentasikan.

## Langkah Implementasi

1. Inventaris dependency runtime hasil build per platform dengan alat yang sesuai, misalnya `ldd` di Linux, `otool -L` di macOS, dan `dumpbin`/`Dependencies` di Windows.
2. Putuskan strategi packaging per platform:
   - Linux: bundle `.so` yang memang perlu dibawa atau buat static link yang realistis.
   - macOS: bundle `.dylib` yang diperlukan dan pastikan path lookup valid.
   - Windows: bundle DLL hasil vcpkg atau ubah triplet/build strategy bila perlu.
3. Ubah workflow release agar men-stage direktori distribusi yang benar, bukan hanya menyalin satu file executable.
4. Tambahkan langkah validasi dependency setelah packaging untuk memastikan tidak ada dependency wajib yang tertinggal.
5. Pastikan nama archive, isi archive, dan layout direktori konsisten lintas platform agar langkah verifikasi dan dokumentasi bisa distandardisasi.
6. Sinkronkan dokumentasi release agar menjelaskan dengan jelas platform mana yang didukung sebagai binary release dan apa saja yang dibundel.

## File/Area yang Mungkin Terdampak

- `.github/workflows/release.yml`
- `CMakeLists.txt`
- `README.md`
- `RELEASING.md`
- `vcpkg.json`
- Script baru untuk staging atau dependency bundling, misalnya di `scripts/` atau `cmake/`

## Kriteria Selesai

- [ ] Ada keputusan packaging yang jelas per platform release yang ditargetkan.
- [ ] Archive Linux, macOS, dan Windows tidak lagi hanya berisi executable tunggal jika binary masih dynamic-linked.
- [ ] Workflow release gagal bila dependency runtime yang diwajibkan tidak ikut terpaketkan.
- [ ] Binary hasil ekstraksi dapat start di runner CI tanpa missing library/DLL error.
- [ ] `README.md` dan/atau `RELEASING.md` menjelaskan cakupan binary release yang sebenarnya.

## Risiko / Catatan

- Static linking untuk `libcurl` dan dependency transitifnya bisa rumit dan tidak selalu portable.
- Linux binary compatibility perlu diputuskan dengan sadar; build di distro terlalu baru bisa tetap bermasalah walau dependency sudah dibundel.
- macOS kemungkinan butuh tindak lanjut codesign/notarization bila distribusi makin luas.
- Jangan membundel library sistem secara membabi buta tanpa memahami implikasi lisensi, ukuran artefak, dan compatibility.

## Prioritas

P0

## Status

partial

## Progress Notes

- Menambahkan mode release bundling opsional di `CMakeLists.txt` lewat `OCEANDL_ENABLE_RELEASE_BUNDLE` dan `OCEANDL_EXTRA_BUNDLE_DIRS`.
- Menambahkan script install-time bundling baru di `cmake/oceandl_install_bundle.cmake.in` yang memakai `BundleUtilities` untuk menyalin non-system runtime dependency ke install tree, serta `patchelf` di Linux untuk memberi `RPATH` `$ORIGIN` pada executable dan shared library hasil bundle.
- Mengubah `.github/workflows/release.yml` agar release archive dibangun dari install tree (`cmake --install --prefix ...`) alih-alih hanya menyalin executable mentah. Scope maintainer-owned public binary release kemudian dipersempit menjadi Linux `x64` saja agar jalur yang dipublish benar-benar yang dipelihara.
- Memperbarui `RELEASING.md` agar checklist release menjelaskan layout install tree, bundling runtime library, dan requirement smoke test dari archive staging.
- File yang diubah:
  - `CMakeLists.txt`
  - `cmake/oceandl_install_bundle.cmake.in`
  - `.github/workflows/release.yml`
  - `RELEASING.md`
- Hasil verifikasi:
  - `cmake -S . -B build-bundle -G Ninja -DCMAKE_BUILD_TYPE=Release -DOCEANDL_ENABLE_RELEASE_BUNDLE=ON -DOCEANDL_EXTRA_BUNDLE_DIRS='/usr/lib;/usr/local/lib'` sukses.
  - `cmake --build build-bundle --parallel` sukses.
  - `ctest --test-dir build-bundle --output-on-failure` lulus `1/1`.
  - `cmake --install build-bundle --prefix "$PWD/dist-test/oceandl-linux-x64"` sukses.
  - `dist-test/oceandl-linux-x64/bin/oceandl --version` dan `datasets` sukses.
  - `ldd dist-test/oceandl-linux-x64/bin/oceandl` menunjukkan `libcurl`, `libfmt`, `libtomlplusplus`, dan dependency transitif utama ter-resolve dari bundle di `dist-test/oceandl-linux-x64/bin/`.
  - `patchelf --print-rpath` pada executable dan library bundel menunjukkan `$ORIGIN`.
  - Archive Linux hasil staging berisi install tree lengkap (`bin/` + docs).
- Sisa pekerjaan:
  - Jika maintainer ingin membuka kembali binary release untuk macOS atau Windows, policy packaging dan verifikasi artefaknya perlu didefinisikan ulang dengan workflow terpisah atau scope release yang lebih luas.
  - Review lagi daftar library Linux yang dibundel jika ingin memperkecil artefak tanpa mengorbankan sifat self-contained.
