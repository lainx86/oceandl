# Perjelas dokumentasi instalasi publik, support matrix, dan arti status alpha

## Ringkasan

README saat ini baru menyebut dependency tingkat tinggi seperti `libcurl`, `fmt`, dan `tomlplusplus`, tetapi belum memberi langkah bootstrap yang konkret untuk Linux, macOS, dan Windows. Review juga menegaskan bahwa project masih alpha dan belum boleh diposisikan sebagai stable binary release umum.

## Kenapa Ini Penting

Untuk distribusi publik, dokumentasi harus bisa dipakai user baru tanpa menebak paket apa yang dipasang atau apa ekspektasi stabilitas project. Dokumentasi yang jujur juga mencegah mismatch antara kondisi nyata project dan framing rilis.

## Dampak ke User

User baru bisa gagal build hanya karena tidak tahu nama paket dependency di OS mereka. User lain juga bisa salah menganggap project sudah siap sebagai "download binary and it just works" padahal review menyarankan framing `alpha` dan `source-first`.

## Bukti dari Review

- `review.md:73-76` menyebut dokumentasi instalasi masih terlalu tinggi levelnya dan user baru masih harus menebak paket yang perlu dipasang.
- `review.md:99-103` menegaskan project masih berstatus alpha dan jangan di-branding sebagai stable public release.
- `review.md:132` menandai `Install docs untuk user baru` sebagai `sebagian siap`.
- `review.md:153-158` memberi framing rilis yang jujur: alpha, source-first, untuk early adopter teknis.
- `review.md:169` merekomendasikan penjelasan dependency per OS, support matrix, dan arti status alpha di README.

## Tujuan Perbaikan

README dan dokumen terkait memberi langkah instalasi yang konkret per OS, menyatakan platform yang didukung, dan menjelaskan dengan jujur arti status alpha serta ekspektasi penggunaan binary vs source build.

## Langkah Implementasi

1. Tetapkan support matrix yang benar-benar didukung berdasarkan CI dan tingkat keyakinan maintainer.
2. Tambahkan instruksi bootstrap dependency yang konkret untuk Linux, macOS, dan Windows, termasuk contoh package manager atau toolchain yang direkomendasikan.
3. Perjelas perbedaan antara source build, local install, dan binary release bila tersedia.
4. Tambahkan penjelasan singkat tentang arti status alpha, siapa target user saat ini, dan apa yang belum dijanjikan project.
5. Sinkronkan narasi yang sama di `README.md`, `RELEASING.md`, dan bila perlu `CONTRIBUTING.md`.
6. Verifikasi instruksi dengan menjalankannya di environment bersih atau melalui langkah CI yang bisa dipakai ulang sebagai referensi.

## File/Area yang Mungkin Terdampak

- `README.md`
- `RELEASING.md`
- `CONTRIBUTING.md`
- `.github/workflows/ci.yml`
- `vcpkg.json`

## Kriteria Selesai

- [ ] README memiliki langkah instalasi dependency yang konkret per OS yang didukung.
- [ ] Support matrix dinyatakan jelas, bukan implisit.
- [ ] Status alpha dijelaskan dengan bahasa yang jujur dan tidak misleading.
- [ ] Dokumentasi menjelaskan apakah release saat ini source-first, binary-first, atau kombinasi dengan batasan tertentu.
- [ ] Instruksi yang ditulis sudah diverifikasi minimal sekali di environment yang relevan.

## Risiko / Catatan

- Nama paket dependency berbeda antar distro Linux; jangan membuat klaim terlalu umum tanpa contoh distro yang jelas.
- Jika binary release belum benar-benar siap, dokumentasi harus tetap konsisten dengan fakta itu.
- Hindari mencampur terlalu banyak jalur install sekaligus; pilih jalur utama yang memang didukung.

## Prioritas

P1

## Status

done

## Progress Notes

- Menambahkan section `Alpha status` di `README.md` untuk menjelaskan secara eksplisit bahwa `oceandl` masih alpha, source-first, dan ditujukan untuk early adopter teknis serta kontributor.
- Menambahkan support matrix yang jelas di `README.md`, berbasis platform yang benar-benar dicakup CI saat ini: Linux, macOS, dan Windows.
- Menambahkan instruksi bootstrap dependency yang konkret di `README.md`:
  - Debian/Ubuntu example yang mengikuti langkah install dependency di CI,
  - Arch-like example dengan nama paket ekuivalen,
  - Homebrew example untuk macOS,
  - Visual Studio/Build Tools + `vcpkg` example untuk Windows.
- Memperjelas perbedaan source build vs GitHub Release artifact di `README.md`: source build tetap jalur utama yang direkomendasikan, sedangkan release archive diposisikan sebagai convenience artifact yang masih alpha.
- Memperbarui `RELEASING.md` agar release checklist dan public messaging tetap konsisten dengan framing alpha/source-first dan support matrix yang benar-benar diuji.
- Menambahkan catatan sinkronisasi di `CONTRIBUTING.md` agar perubahan platform support, bootstrap dependency, atau release positioning ikut memperbarui `README.md` dan `RELEASING.md`.
- File yang diubah: `README.md`, `RELEASING.md`, `CONTRIBUTING.md`, `problems/problem6.md`.
- Hasil verifikasi:
  - `cat /etc/os-release`
  - `pacman -Q cmake ninja curl fmt tomlplusplus`
  - `cmake -S . -B build-problem6 -G Ninja -DCMAKE_BUILD_TYPE=Release`
  - `cmake --build ./build-problem6 --parallel`
  - `cmake --install build-problem6 --prefix /tmp/oceandl-install-problem6`
  - `/tmp/oceandl-install-problem6/bin/oceandl --version`
  - `/tmp/oceandl-install-problem6/bin/oceandl datasets`
  - `/tmp/oceandl-install-problem6/bin/oceandl providers`
  - `/tmp/oceandl-install-problem6/bin/oceandl download --help`
  - `./build-problem6/oceandl --version`
  - `./build-problem6/oceandl datasets`
  - `./build-problem6/oceandl providers`
  - `./build-problem6/oceandl download --help`
  - `ctest --test-dir build-problem6 --output-on-failure`
- Catatan verifikasi:
  - build/install/source-run diverifikasi langsung pada environment Linux lokal ini,
  - contoh bootstrap Debian/Ubuntu, Homebrew, dan Windows `vcpkg` diselaraskan dengan langkah dependency install yang sudah ada di workflow CI,
  - integration test localhost pada `ctest` perlu dijalankan di luar sandbox sesi ini karena bind socket localhost diblokir oleh sandbox, tetapi lulus saat dijalankan unrestricted.
- Sisa pekerjaan: tidak ada untuk scope problem ini.
