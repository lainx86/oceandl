# Ganti security disclosure placeholder dengan kanal nyata dan proses pelaporan yang valid

## Ringkasan

Project masih menampilkan alamat security disclosure placeholder di `SECURITY.md` dan issue template config. Untuk repository publik, kanal pelaporan vulnerability harus nyata, aktif, dan dimonitor.

## Kenapa Ini Penting

Ini blocker untuk kesiapan open-source publik. Security contact palsu membuat project terlihat belum siap menerima laporan kerentanan secara bertanggung jawab.

## Dampak ke User

Reporter bisa mengirim laporan ke alamat yang tidak ada, tidak pernah mendapat respons, atau akhirnya membuka issue publik untuk vulnerability yang seharusnya dilaporkan privat.

## Bukti dari Review

- `review.md:53-61` menyebut jalur pelaporan security untuk publik masih placeholder dan tidak boleh dipajang untuk project publik.
- `review.md:139` menandai `Security disclosure process` sebagai `belum siap`.
- `review.md:166` memberi rekomendasi eksplisit untuk mengganti placeholder security contact dengan kanal nyata.

## Tujuan Perbaikan

Reporter security memiliki satu kanal privat yang benar-benar aktif, jelas pemiliknya, dan terdokumentasi konsisten di seluruh repo.

## Langkah Implementasi

1. Tentukan kanal intake resmi yang benar-benar dijaga, misalnya mailbox maintainer khusus, alias email tim, GitHub Private Vulnerability Reporting, atau kombinasi keduanya.
2. Pastikan ada owner operasional untuk kanal tersebut, termasuk siapa yang memantau inbox dan target waktu acknowledgement awal.
3. Ganti seluruh placeholder di dokumentasi dan template issue dengan kanal final.
4. Perjelas proses minimum di `SECURITY.md`: versi yang didukung, cara melapor, ekspektasi acknowledgement, dan larangan membuka issue publik untuk vulnerability.
5. Jika GitHub Private Vulnerability Reporting akan dipakai, sinkronkan dokumentasi repo dengan setting repository agar link dan prosesnya benar-benar tersedia.
6. Audit dokumen user-facing lain untuk memastikan tidak ada salinan kontak placeholder yang tertinggal.

## File/Area yang Mungkin Terdampak

- `SECURITY.md`
- `.github/ISSUE_TEMPLATE/config.yml`
- `README.md`
- `CONTRIBUTING.md`
- GitHub repository settings terkait security advisories atau private vulnerability reporting

## Kriteria Selesai

- [ ] Tidak ada lagi placeholder security contact di repo.
- [ ] Ada satu kanal pelaporan privat yang valid dan sudah diuji dapat diakses.
- [ ] `SECURITY.md` menjelaskan alur pelaporan dan ekspektasi respons minimum.
- [ ] Issue template config mengarahkan reporter ke kanal privat yang benar.
- [ ] Maintainer yang bertanggung jawab atas intake security sudah ditetapkan.

## Risiko / Catatan

- Jangan mempublikasikan alamat email baru sebelum mailbox benar-benar aktif dan dimonitor.
- Kalau hanya mengandalkan satu orang, process risk tetap tinggi; pertimbangkan alias atau backup owner.
- Jika memakai GitHub private reporting, pastikan repository setting benar-benar aktif; dokumentasi tanpa setting yang aktif sama buruknya dengan placeholder.

## Prioritas

P0

## Status

partial

## Progress Notes

- Mengganti placeholder security contact dengan alamat maintainer yang nyata dan konsisten dengan metadata git repo: `febysyarief.dev@gmail.com`.
- Memperjelas `SECURITY.md` dengan owner operasional, versi yang didukung, larangan public issue/PR untuk vulnerability, target acknowledgement, target update triage, dan langkah disclosure minimum.
- Memperbarui `.github/ISSUE_TEMPLATE/config.yml` agar reporter diarahkan ke `mailto:` yang benar dengan subject yang lebih spesifik.
- Menambahkan rujukan singkat di `README.md` dan `CONTRIBUTING.md` agar user dan kontributor tidak melaporkan vulnerability lewat jalur publik.
- File yang diubah:
  - `SECURITY.md`
  - `.github/ISSUE_TEMPLATE/config.yml`
  - `README.md`
  - `CONTRIBUTING.md`
- Hasil verifikasi:
  - Placeholder lama di tracked repo sudah dihapus dari dokumen dan issue template yang relevan.
  - Alamat email di `SECURITY.md` dan `config.yml` sekarang konsisten.
  - Email yang dipakai cocok dengan identitas maintainer yang muncul di `git log`, `git shortlog`, dan `git config`.
- Sisa pekerjaan:
  - Saya tidak bisa membuktikan dari environment offline ini bahwa inbox eksternal benar-benar aktif menerima email; itu tetap perlu diverifikasi oleh maintainer di luar repo.
  - Jika nantinya GitHub Private Vulnerability Reporting diaktifkan pada repository settings, dokumentasi bisa diperluas untuk menawarkannya sebagai kanal tambahan.
