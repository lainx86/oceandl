# Tambahkan verifikasi artefak release dengan checksum, smoke test archive final, dan jalur signature

## Ringkasan

Workflow release saat ini mempublikasikan artefak tanpa checksum, tanpa signature, dan tanpa validasi terhadap archive final yang benar-benar akan diunduh user. Ini membuat integritas artefak dan kualitas paket akhir tidak cukup terjamin.

## Kenapa Ini Penting

Distribusi publik butuh cara verifikasi yang jelas. Tanpa checksum minimal, user tidak punya cara sederhana untuk memastikan file yang diunduh utuh. Tanpa smoke test terhadap archive final, workflow bisa tetap hijau walau artefak yang dipublish rusak atau tidak runnable.

## Dampak ke User

User tidak bisa memverifikasi bahwa artefak yang diunduh sama dengan yang diproduksi CI. Jika archive final rusak, salah isi, atau tidak executable setelah diekstrak, masalah baru diketahui setelah user mencoba sendiri.

## Bukti dari Review

- `review.md:79-81` menyebut release asset dipublish tanpa checksum, signature, atau smoke test archive final.
- `review.md:135` menandai `Checksum/signature release` sebagai `belum siap`.
- `review.md:165` merekomendasikan penambahan checksum minimal SHA-256 untuk semua release asset.
- `review.md:164` juga menekankan perlunya smoke test terhadap archive final.

## Tujuan Perbaikan

Setiap artefak release memiliki checksum yang dipublish bersama release, dan workflow memvalidasi archive final sebelum aset dipublikasikan. Bila signing key siap, release juga memiliki signature yang bisa diverifikasi.

## Langkah Implementasi

1. Tambahkan tahap pembuatan checksum SHA-256 untuk setiap archive release dan satu file checksum teragregasi.
2. Tambahkan job atau step validasi yang mengunduh artefak hasil packaging, mengekstraknya, lalu menjalankan smoke command dari hasil ekstraksi.
3. Pastikan validasi memakai isi archive final, bukan binary dari folder build mentah.
4. Publish file checksum bersama artefak release dan dokumentasikan cara verifikasinya.
5. Putuskan strategi signature:
   - Jika kunci signing sudah siap, tambahkan detached signature.
   - Jika belum siap, dokumentasikan keputusan eksplisit bahwa checksum menjadi baseline sekarang dan signature dijadwalkan sebagai tahap berikutnya.
6. Perbarui checklist release agar maintainer wajib memeriksa checksum dan hasil smoke test sebelum publish.

## File/Area yang Mungkin Terdampak

- `.github/workflows/release.yml`
- `RELEASING.md`
- `README.md`
- Script baru untuk generate checksum atau verifikasi artefak

## Kriteria Selesai

- [ ] Setiap release menghasilkan checksum SHA-256 untuk semua artefak.
- [ ] File checksum ikut dipublish sebagai release asset.
- [ ] Workflow release menjalankan smoke test terhadap archive final yang diekstrak.
- [ ] Release gagal dipublish jika checksum generation atau smoke test archive final gagal.
- [ ] Strategi signature sudah diputuskan dan terdokumentasi, baik sudah diimplementasikan maupun dijadwalkan secara eksplisit.

## Risiko / Catatan

- Smoke test archive final bergantung pada problem packaging; dua topik ini harus bergerak selaras.
- Signature tanpa manajemen kunci yang benar hanya menambah kompleksitas tanpa nilai nyata.
- Dokumentasi verifikasi harus memakai perintah yang realistis untuk Linux, macOS, dan Windows, bukan hanya satu platform.

## Prioritas

P1

## Status

partial

## Progress Notes

- Menambahkan verifikasi archive final lintas platform di workflow release melalui job `verify-artifacts` yang mengunduh artifact hasil packaging, mengekstraknya, lalu menjalankan smoke command dari hasil ekstraksi.
- Menambahkan `SHA256SUMS` generation di job `publish` dan mempublikasikannya sebagai release asset bersama archive platform.
- Menambahkan script reusable untuk verifikasi archive final:
  - `scripts/verify_release_archive.sh`
  - `scripts/verify_release_archive.ps1`
- Memperbarui `README.md` dan `RELEASING.md` agar checksum release, cara verifikasi, dan strategi signature terdokumentasi dengan jelas.
- Strategi signature yang dipilih saat ini:
  - checksum SHA-256 menjadi baseline integrity mechanism
  - detached signature belum dipublikasikan sampai maintainer-managed signing keys dan rotasi kunci siap
- File yang diubah:
  - `.github/workflows/release.yml`
  - `scripts/verify_release_archive.sh`
  - `scripts/verify_release_archive.ps1`
  - `RELEASING.md`
  - `README.md`
- Hasil verifikasi:
  - script verifikasi archive Linux berjalan sukses terhadap archive lokal hasil staging
  - checksum SHA-256 lokal berhasil dibuat dan diverifikasi ulang dengan `sha256sum -c`
  - workflow YAML sudah diperbarui agar publish job gagal bila checksum generation gagal
- Sisa pekerjaan:
  - menjalankan workflow release nyata di GitHub Actions untuk memverifikasi jalur macOS dan Windows end-to-end
