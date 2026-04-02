# Siapkan distribusi package manager setelah release binary dan metadata rilis stabil

## Ringkasan

Checklist review menandai package manager distribution sebagai belum siap karena belum ada formula, manifest, atau package spec. Review juga menegaskan bahwa topik ini sebaiknya dikerjakan setelah packaging binary, checksum, dan release process dasar sudah beres.

## Kenapa Ini Penting

Distribusi yang lebih luas biasanya mengandalkan package manager. Tanpa spesifikasi paket yang terawat, user publik tetap harus build manual atau mengunduh archive secara langsung, yang membatasi adopsi.

## Dampak ke User

User yang mengharapkan instalasi via Homebrew, Winget, Scoop, AUR, atau jalur package manager lain belum punya opsi resmi. Ini membuat onboarding lebih berat dan maintenance release lebih manual.

## Bukti dari Review

- `review.md:136` menandai `Package manager distribution` sebagai `belum siap`.
- `review.md:150-151` menyebut project belum siap untuk package manager distribution.
- `review.md:171` menyarankan memikirkan Homebrew, Scoop/Winget, AUR, atau source tarball formal setelah problem release inti selesai.

## Tujuan Perbaikan

Project memiliki rencana distribusi package manager yang realistis, dimulai dari target paling relevan dan ditopang oleh artefak release yang stabil, checksum yang konsisten, dan dokumentasi instalasi yang siap dipakai publik.

## Langkah Implementasi

1. Tentukan package manager target tahap pertama berdasarkan platform utama user, kapasitas maintainer, dan kualitas artefak release yang sudah tersedia.
2. Definisikan kontrak release yang dibutuhkan package manager: nama file, checksum, URL asset, versioning, dan layout archive.
3. Pilih urutan implementasi yang realistis, misalnya mulai dari Homebrew dan satu jalur Windows sebelum ekspansi lebih jauh.
4. Siapkan formula atau manifest awal beserta instruksi update versinya setiap rilis.
5. Dokumentasikan ownership maintenance agar update package tidak tertinggal dari tag release.
6. Tambahkan validasi ringan bahwa install path dari package manager benar-benar menghasilkan binary yang bisa dijalankan.

## File/Area yang Mungkin Terdampak

- `README.md`
- `RELEASING.md`
- `.github/workflows/release.yml`
- Formula/manifest/package spec baru sesuai target yang dipilih
- Repository eksternal package manager, bila distribusi tidak disimpan di repo ini

## Kriteria Selesai

- [ ] Ada keputusan package manager mana yang didukung resmi pada tahap pertama.
- [ ] Release artifact contract sudah cukup stabil untuk dipakai package manager.
- [ ] Minimal satu formula atau manifest resmi tersedia dan bisa dipakai install.
- [ ] Proses update versi dan checksum untuk package manager terdokumentasi.
- [ ] README menjelaskan jalur install resmi yang tersedia.

## Risiko / Catatan

- Jangan memulai package manager distribution sebelum packaging release dan checksum stabil; kalau tidak, maintenance akan cepat rusak.
- Setiap ecosystem punya policy dan beban maintenance berbeda; pilih sedikit jalur yang benar-benar bisa dipelihara.
- Beberapa package manager memakai repository eksternal, sehingga ownership dan SLA update perlu dipikirkan sejak awal.

## Prioritas

P2

## Status

partial

## Progress Notes

- Memutuskan target package manager tahap pertama: Arch `makepkg` / AUR-compatible source package. Alasan utamanya adalah Linux punya coverage CI paling dalam saat ini, maintainer burden lebih rendah daripada membuka Homebrew dan Windows ecosystem sekaligus, dan jalurnya bisa diverifikasi langsung di environment lokal.
- Menambahkan kontrak artefak release formal untuk package-manager consumer:
  - source archive `oceandl-src-vX.Y.Z.tar.gz`
  - diproduksi oleh `scripts/create_source_release.sh`
  - dipublish oleh workflow `Release`
  - ikut masuk ke `SHA256SUMS`
- Menambahkan package spec resmi di repo:
  - `packaging/arch/oceandl/PKGBUILD`
  - `packaging/arch/oceandl/.SRCINFO`
  - `packaging/arch/oceandl/README.md`
- Menambahkan verifikasi lokal untuk jalur install package manager:
  - `scripts/verify_arch_pkgbuild.sh` membangun paket via `makepkg`, mengekstrak hasil package, lalu menjalankan smoke check pada binary terpasang.
- Memperbarui `README.md` dan `RELEASING.md` agar keputusan target package manager, kontrak source archive, dan proses update versi/checksum terdokumentasi.
- Hasil verifikasi:
  - `./scripts/create_source_release.sh 0.2.0 dist/oceandl-src-v0.2.0.tar.gz`
  - `sha256sum dist/oceandl-src-v0.2.0.tar.gz`
  - `./scripts/verify_arch_pkgbuild.sh dist/oceandl-src-v0.2.0.tar.gz packaging/arch/oceandl`
  - `bash -n scripts/create_source_release.sh scripts/verify_arch_pkgbuild.sh`
  - `cd packaging/arch/oceandl && makepkg --printsrcinfo > .SRCINFO`
- Sisa pekerjaan:
  - repo saat ini belum memiliki git tag / GitHub Release publik yang cocok, jadi `PKGBUILD` belum bisa dipin ke checksum release yang benar-benar sudah dipublish. Karena itu status tetap `partial` sampai tagged release pertama dibuat dan checksum default `SKIP` diganti dengan SHA-256 dari `SHA256SUMS`.
