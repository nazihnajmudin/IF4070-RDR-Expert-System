# Checklist Quality Control

Pemeriksaan manual yang tidak dapat dilakukan secara otomatis oleh unit test maupun end-to-end test. Isi kolom hasil saat melakukan QC.

## 1. Build dan Distribusi Windows

| No | Pemeriksaan | Langkah | Hasil |
|---|---|---|---|
| W1 | Build native | Di Windows dengan w64devkit atau MSYS2, jalankan `scripts\build.bat` dari folder repository | |
| W2 | Double click | Salin `build\windows\rdr-expert.exe` ke folder lain (misalnya Desktop) lalu double click | |
| W3 | Komputer tanpa toolchain | Jalankan exe di komputer Windows yang tidak memiliki gcc atau library tambahan | |
| W4 | SmartScreen dan antivirus | Catat jika Windows menampilkan peringatan untuk exe yang tidak ditandatangani | |
| W5 | Membuka file lewat exe | Seret `examples\diagnosis.rdr.json` ke atas exe | |
| W6 | Nama file non-ASCII | Simpan dan buka project dengan nama atau folder yang memuat huruf non-ASCII, misalnya `pengetahuan_é.rdr.json` | |

## 2. Interaksi

| No | Pemeriksaan | Hasil |
|---|---|---|
| I1 | Mengetik cepat di bar Q, tab Query, dan baris klausa tanpa karakter hilang | |
| I2 | Menahan Backspace dan tombol panah (key repeat) pada field teks | |
| I3 | Copy dan paste antar aplikasi (dari editor lain ke bar Q, dan dari tab Tree ke editor lain) | |
| I4 | Scroll dengan mouse wheel dan touchpad pada tab Tree yang panjang | |
| I5 | Menggeser splitter dengan mouse, termasuk kursor yang berubah menjadi tanda geser | |
| I6 | Navigasi Alt pada layout keyboard yang digunakan | |
| I7 | Menutup window dengan tombol X saat ada perubahan belum disimpan | |

## 3. Tampilan

| No | Pemeriksaan | Hasil |
|---|---|---|
| T1 | Kerapian dan kesesuaian dengan sketsa `plan/ui-plan.png` | |
| T2 | Layar HiDPI atau scaling Windows 125 sampai 200 persen (ukuran font dapat diatur lewat Preference) | |
| T3 | Multi monitor dan memindahkan window antar monitor | |
| T4 | Ukuran window minimum (760 x 480) dan window maksimum | |
| T5 | Tema gelap pada seluruh dialog | |

## 4. Platform Lain

| No | Pemeriksaan | Hasil |
|---|---|---|
| P1 | Linux desktop native (bukan WSL2), misalnya Ubuntu dengan X11 atau Wayland (XWayland) | |
| P2 | Build dengan clang | |

## 5. Knowledge dan Domain

| No | Pemeriksaan | Hasil |
|---|---|---|
| K1 | Membangun knowledge base domain sendiri dari nol lewat koreksi interaktif | |
| K2 | Mengimpor dataset CSV domain sendiri dan meninjau laporan per baris | |
| K3 | Menilai apakah strategi revisi default sesuai penjelasan dosen | |
| K4 | Membuka file `*.rdr.json` dengan editor teks dan menilai keterbacaannya | |
