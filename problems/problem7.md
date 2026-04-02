# Putuskan dan perjelas perilaku file .lock setelah download sukses di Unix

## Ringkasan

Review menemukan bahwa file `.lock` sengaja dibiarkan tertinggal di Unix setelah operasi sukses. Secara teknis ini tidak langsung merusak download, tetapi perilakunya membingungkan untuk user publik karena direktori terlihat seperti masih terkunci.

## Kenapa Ini Penting

Perilaku locking adalah bagian dari UX dan trust model tool downloader. Artefak lock yang ambigu bisa membuat user mengira proses macet, gagal cleanup, atau tidak aman dijalankan ulang.

## Dampak ke User

User dapat melihat file `*.lock` setelah download sukses lalu mengira ada proses lain yang masih aktif atau output belum final. Ini berpotensi memicu laporan bug palsu, manual cleanup yang tidak perlu, atau ketidakpercayaan terhadap safety mechanism tool.

## Bukti dari Review

- `review.md:91-95` menyebut file `.lock` sengaja dibiarkan di Unix setelah sukses dan ini membingungkan user publik.
- `review.md:110` memasukkan lock artifact tertinggal sebagai contoh pengalaman yang mengecewakan user.
- `review.md:120` merekomendasikan dua opsi: hapus `.lock` saat selesai atau dokumentasikan dengan jelas jika memang sengaja dipertahankan.
- `review.md:170` meminta keputusan strategi `.lock` sebelum rilis.

## Tujuan Perbaikan

Ada keputusan produk dan implementasi yang jelas untuk perilaku `.lock` setelah sukses, lengkap dengan test dan dokumentasi yang konsisten sehingga user tidak salah menafsirkan artefak tersebut.

## Langkah Implementasi

1. Putuskan policy final untuk Unix: hapus lock artifact setelah sukses, atau pertahankan dengan alasan yang jelas.
2. Audit seluruh jalur lifecycle lock pada kondisi sukses, skip, error, recovery legacy lock, dan crash/interruption.
3. Jika policy-nya remove on success, rancang cleanup yang tidak mengorbankan safety terhadap concurrent process.
4. Jika policy-nya retain, tambahkan dokumentasi dan output user-facing yang menjelaskan arti file `.lock` dan kapan aman diabaikan.
5. Perbarui test untuk mencerminkan policy final, terutama test yang saat ini mengharapkan lock artifact tetap ada setelah skip atau resume.
6. Tinjau ulang perilaku lintas platform agar perbedaan Unix vs Windows benar-benar disengaja, bukan kebetulan implementasi.

## File/Area yang Mungkin Terdampak

- `cpp/src/download_lock.cpp`
- `cpp/src/target_download_executor.cpp`
- `cpp/tests/test_main.cpp`
- `README.md`
- Dokumentasi user-facing lain yang menjelaskan perilaku download/resume

## Kriteria Selesai

- [ ] Ada keputusan eksplisit tentang lifecycle `.lock` di Unix.
- [ ] Implementasi dan test suite konsisten dengan keputusan tersebut.
- [ ] User-facing documentation menjelaskan perilaku `.lock` yang akan mereka lihat.
- [ ] Jalur sukses, skip, dan recovery lock memiliki perilaku yang bisa diprediksi.
- [ ] Tidak ada lagi ambiguitas apakah lock artifact berarti proses masih berjalan.

## Risiko / Catatan

- Menghapus lock terlalu cepat bisa melemahkan proteksi terhadap concurrent writer jika implementasinya tidak hati-hati.
- Mempertahankan lock untuk alasan teknis tertentu sah, tetapi harus dikomunikasikan dengan sangat jelas.
- Perubahan ini menyentuh area reliability; test coverage wajib dijaga agar tidak memicu regresi resume/locking behavior.

## Prioritas

P2

## Status

done

## Progress Notes

- Memutuskan policy final Unix `.lock`: artefak lock harus dihapus setelah jalur sukses, skip, dan recovery selesai. Implementasi Unix sekarang memakai lock directory yang dibersihkan saat `TargetFileLock` dilepas, sehingga perilakunya selaras dengan Windows di level user-facing.
- Menambahkan recovery kompatibilitas untuk legacy Unix `.lock` file dari implementasi lama. Jika file lama masih aktif dikunci proses lain, download tetap gagal dengan error lock; jika file lama sudah stale, artefaknya dibersihkan lalu download dilanjutkan.
- Memperbarui test downloader agar mengharapkan tidak ada `.lock` yang tertinggal setelah success, skip, resume, dan recovery stale lock. Ditambahkan juga test baru untuk success path biasa dan recovery legacy lock file Unix.
- Memperbarui `README.md` pada bagian reliability/troubleshooting agar user tahu `.lock` bersifat sementara, kapan ia akan hilang, dan apa arti lock conflict jika masih muncul.
- File yang diubah:
  - `cpp/src/download_lock.cpp`
  - `cpp/src/download_lock.hpp`
  - `cpp/tests/test_main.cpp`
  - `README.md`
  - `problems/problem7.md`
- Hasil verifikasi:
  - `cmake -S . -B build-problem7 -G Ninja -DCMAKE_BUILD_TYPE=Release`
  - `cmake --build build-problem7 --parallel`
  - `ctest --test-dir build-problem7 --output-on-failure`
  - `./build-problem7/oceandl --version`
  - `./build-problem7/oceandl datasets`
  - `./build-problem7/oceandl download --help`
  - `cmake -S . -B build-problem7-strict -G Ninja -DCMAKE_BUILD_TYPE=Release -DOCEANDL_STRICT_WARNINGS=ON`
  - `cmake --build build-problem7-strict --parallel`
- Sisa pekerjaan: tidak ada untuk scope `problem7.md`.
