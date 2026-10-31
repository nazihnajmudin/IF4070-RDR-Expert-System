# RDR Expert System

Expert system berbasis **binary Single Classification Ripple Down Rules (SCRDR)** dengan antarmuka grafis, ditulis dalam C (C99). Knowledge berupa pohon rule yang terus bertambah setiap kali pakar mengoreksi kesimpulan yang salah. Satu-satunya pengaturan yang berbeda antar domain adalah kesimpulan default pada root `T >> <default>`, sehingga aplikasi yang sama dapat dipakai untuk diagnosis penyakit, perhitungan pajak, klasifikasi, dan domain lain.

Dibuat untuk tugas mata kuliah IF4070 Representasi Pengetahuan dan Penalaran, Institut Teknologi Bandung.

### Kelompok
|NIM|Nama|
|--|-----|
|13523144|Muhamad Nazih Najmudin|
|13523158|Lukas Raja Agripa|

## Fitur Utama

- Engine binary SCRDR buatan sendiri: inferensi, akuisisi exception, kasus revisi dengan tiga strategi, edit manual, dan validasi cornerstone
- Klausa boolean, literal, dan numerik dengan Open World Assumption
- Knowledge, preferensi, dan sesi terakhir disimpan dalam satu file JSON yang mudah dibaca
- Akuisisi batch dari dataset CSV
- GUI dengan panel kueri, hasil, dan tree berwarna, teks yang dapat disalin, undo dan redo, navigasi keyboard, tema terang dan gelap, serta halaman Help dalam bahasa Indonesia dan Inggris
- Program CLI `rdr-cli` untuk pemakaian headless dan pengujian
- Satu executable tanpa instalasi library tambahan untuk Linux, WSL2, dan Windows

## Mulai Cepat

```bash
make                                              # build/linux/rdr-expert dan build/linux/rdr-cli
./build/linux/rdr-expert examples/diagnosis.rdr.json
```

Windows: jalankan `scripts\build.bat` (butuh gcc MinGW-w64) lalu double click `build\windows\rdr-expert.exe`. Rincian ada di [docs/BUILD.md](docs/BUILD.md).

## Dokumentasi

| Dokumen | Isi |
|---|---|
| [docs/USER_GUIDE.md](docs/USER_GUIDE.md) | Panduan pemakaian GUI dan CLI |
| [docs/RDR_SEMANTICS.md](docs/RDR_SEMANTICS.md) | Model RDR, semantik klausa, inferensi, akuisisi, dan kasus revisi |
| [docs/FILE_FORMATS.md](docs/FILE_FORMATS.md) | Format file project JSON dan dataset CSV |
| [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) | Struktur modul, ADT, dan alur data |
| [docs/BUILD.md](docs/BUILD.md) | Kompilasi di Linux, WSL2, dan Windows |
| [docs/testing/E2E_TEST_PLAN.md](docs/testing/E2E_TEST_PLAN.md) | Rancangan end-to-end test |
| [docs/testing/E2E_REPORT.md](docs/testing/E2E_REPORT.md) | Hasil end-to-end test, temuan, dan perbaikan |
| [docs/testing/QC_CHECKLIST.md](docs/testing/QC_CHECKLIST.md) | Daftar pemeriksaan manual |

## Struktur Repository

```
include/rdr/   header publik beserta dokumentasi modul
src/core/      engine RDR, parser klausa, JSON, CSV, preferensi
src/app/       controller aplikasi yang dipakai GUI dan CLI
src/platform/  abstraksi sistem file Linux dan Windows
src/gui/       GUI (Nuklear, GLFW, OpenGL)
src/cli/       program rdr-cli
tests/         unit test dan end-to-end test
examples/      contoh dataset dan project (diagnosis, pajak)
assets/help/   isi halaman Help
third_party/   Nuklear, GLFW, dan font DejaVu Sans Mono
scripts/       skrip build Linux dan Windows
docs/          dokumentasi
```

## Lisensi

Kode program dirilis dengan lisensi MIT (lihat [LICENSE](LICENSE)). Komponen pihak ketiga memakai lisensinya masing-masing (lihat [third_party/README.md](third_party/README.md)).
