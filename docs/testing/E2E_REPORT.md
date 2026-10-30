# Laporan End-to-End Test

Laporan pelaksanaan rancangan pada `docs/testing/E2E_TEST_PLAN.md`.

## Lingkungan

| Komponen | Keterangan |
|---|---|
| Sistem | Windows 11 (build 26200) dengan WSL2 Ubuntu 24.04.5, WSLg (Mesa OpenGL 4.5) |
| Compiler Linux | gcc 13.3, C99, `-Wall -Wextra -Wpedantic -Wshadow -Wstrict-prototypes` |
| Compiler Windows | mingw-w64 gcc 13 (cross-compile), executable statis |
| Windows runtime | Dijalankan lewat WSL interop dengan driver OpenGL NVIDIA asli |

## Hasil Akhir

```
CLI scenarios (linux)      14 passed
CLI scenarios (windows)    14 passed
GUI scenarios (linux)       9 passed
GUI scenarios (windows)     1 passed
E2E: 38 passed, 0 failed
```

Seluruh unit test (7 file, 634 check, dengan AddressSanitizer dan UndefinedBehaviorSanitizer) juga lulus. Build Linux dan Windows tidak menghasilkan warning.

Skenario E14 (2000 baris CSV, 1601 node, simpan, buka, inferensi, dan validasi seluruh cornerstone) selesai dalam sekitar 0.3 detik.

## Temuan dan Perbaikan

Temuan berikut muncul saat skenario dijalankan pertama kali. Seluruhnya sudah diperbaiki dan skenario terkait dijalankan ulang sampai lulus.

| No | Skenario | Temuan | Jenis | Perbaikan |
|---|---|---|---|---|
| 1 | E04 | Koreksi tanpa hasil inferensi melaporkan "knowledge base changed", padahal belum ada hasil sama sekali | Pesan menyesatkan | `rdr_correct` membedakan hasil yang belum ada dengan hasil yang usang. Unit test ditambahkan. |
| 2 | E04 | Start yang gagal karena klausa invalid mempertahankan hasil lama, sedangkan Start yang gagal karena kontradiksi menghapusnya. Hasil lama dapat terlihat seolah milik kueri baru. | Perilaku tidak konsisten | Start yang gagal selalu membuang hasil lama. Unit test ditambahkan. |
| 3 | G04 | Saat menu dibuka dengan keyboard, kursor mouse yang diam di atas header lain langsung mengambil alih menu, sehingga Enter menjalankan item menu yang salah | Bug GUI | Menu berpindah karena hover hanya jika mouse benar-benar bergerak. |
| 4 | G06 | Saat panel kanan sempit, legenda warna tab Tree menimpa tombol tab | Bug tampilan | Legenda disembunyikan jika ruang di baris tab tidak cukup. |
| 5 | G09 | Setelah klik slot kosong di Manual Edit, form tambah anak berada di bawah form node sehingga tombol Add Node harus di-scroll | Kegunaan | Selama mode tambah anak, hanya form tambah anak yang ditampilkan. |
| 6 | G07 | Dialog Unsaved Changes menyisakan ruang kosong yang besar | Kerapian | Tinggi dialog disesuaikan dengan isinya. |
| 7 | E14 | Baris tree pada rantai sangat dalam berisi ribuan karakter dan seluruhnya digambar setiap frame | Performa GUI | Tampilan teks hanya menggambar kolom yang berada di area terlihat. |

Temuan selama pengembangan GUI (sebelum suite E2E final) yang juga sudah diperbaiki:

| No | Temuan | Perbaikan |
|---|---|---|
| 8 | Teks di editor Query atau baris klausa hilang jika fokus dipindah lewat keyboard, karena Nuklear tidak memberi sinyal deactivated | Commit dilakukan pada setiap transisi field dari aktif ke tidak aktif |
| 9 | Fokus keyboard hilang saat Alt+C atau Alt+Y sekaligus mengganti tab, karena Nuklear melepas fokus ketika jumlah field berubah | Permintaan fokus ditunda satu frame saat tab berganti |
| 10 | Test hook memakai posisi kursor asli sehingga drag yang disimulasikan tidak akurat | Posisi kursor asli diabaikan saat test hook aktif |

## Catatan

- Expected output CLI hanya boleh diperbarui dengan `--update` setelah perubahan perilaku yang disengaja, lalu diff-nya ditinjau.
- Skenario GUI memakai ukuran window 1200 x 760 dan font 16 piksel. Koordinat klik di skrip mengikuti ukuran tersebut.
- Pengujian yang membutuhkan manusia tercantum di `docs/testing/QC_CHECKLIST.md`.
