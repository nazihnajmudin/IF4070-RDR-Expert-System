# Rancangan End-to-End Test

Dokumen berisi rancangan pengujian end-to-end (E2E) aplikasi RDR Expert System. Pengujian E2E menjalankan program utuh (bukan modul terpisah) dari sudut pandang user, lalu membandingkan hasilnya dengan perilaku yang diharapkan.

## Cara Menjalankan

```bash
make e2e                                  # CLI dan GUI di Linux atau WSL2
sh tests/e2e/run_e2e.sh --windows         # tambahan: executable Windows lewat WSL interop
sh tests/e2e/run_e2e.sh --no-gui          # tanpa display (misalnya server CI)
sh tests/e2e/run_e2e.sh --update          # menulis ulang expected output CLI (wajib ditinjau)
```

Hasil ditulis ke `build/e2e/`:
- `out/*.out` dan `out/*.diff` untuk output dan selisih skenario CLI
- `out/*.gui.log` untuk log skenario GUI, termasuk baris `EXPECT FAILED`
- `shots/` untuk screenshot setiap skenario GUI

## Pendekatan

### Skenario CLI
Setiap skenario adalah skrip perintah `rdr-cli` di `tests/e2e/cli/`. Program CLI memakai controller `App` yang sama dengan GUI, sehingga seluruh alur knowledge (inferensi, akuisisi, revisi, edit manual, file, CSV, undo) teruji tanpa GUI. Output dibandingkan baris per baris dengan `tests/e2e/expected/`. Setiap expected output sudah ditinjau manual terhadap algoritma dan semantik pada `docs/RDR_SEMANTICS.md`.

### Skenario GUI
Setiap skenario adalah skrip test hook di `tests/e2e/gui/` yang dijalankan oleh build khusus (`make TESTHOOK=1`). Skrip mengendalikan GUI asli dengan klik, drag, ketikan, tombol keyboard, dan shortcut, lalu memeriksa state aplikasi dengan perintah `expect`. Skenario gagal jika ada `expect` yang tidak terpenuhi. Screenshot disimpan sebagai bukti visual.

### Platform
Skenario CLI dijalankan dengan executable Linux dan Windows. Skenario GUI dijalankan penuh di Linux (WSLg), dan skenario G02 dijalankan juga dengan executable Windows sebagai smoke test.

## Skenario CLI

| ID | File | Tujuan | Hasil yang diharapkan |
|---|---|---|---|
| E01 | `01_inference_basics` | Inferensi pada KB baru, kueri kosong, hapus klausa | Selalu kesimpulan default, trace hanya root |
| E02 | `02_acquisition_sides` | Penempatan exception | Exception dari node yang menyala masuk slot T, dari node yang gagal masuk slot F, difference list sesuai cornerstone |
| E03 | `03_clause_semantics` | Semantik klausa | Negasi butuh fakta eksplisit, subset numerik, irisan fakta, literal `!=`, isolasi jenis klausa |
| E04 | `04_invalid_input` | Input tidak valid | Setiap jenis error sintaks dan kontradiksi dilaporkan, koreksi tanpa hasil ditolak |
| E05 | `05_revision_strategies` | Kasus revisi | Reject menolak, edit conclusion mengubah node, revision node menambah `T >> ...` di ujung penelusuran, validasi memberi catatan superseded |
| E06 | `06_custom_rule` | Rule exception pilihan user | Rule yang tidak memenuhi kueri ditolak, generalisasi valid diterima, mode strict mengabaikan rule pilihan |
| E07 | `07_manual_edit_validate` | Edit manual | Batasan root, slot terisi, id tidak ada, klausa invalid ditolak, validasi menemukan mismatch, hapus subtree |
| E08 | `08_save_open_session` | Simpan dan buka project | Tree, preferensi, draft, dan hasil terakhir pulih |
| E09 | `09_csv_import` | Insert CSV | Baris valid menjadi exception atau revisi, baris rusak dilaporkan per nomor baris, file tanpa header dan file hilang ditolak |
| E10 | `10_import_prefs` | Import Preference | Hanya preferensi yang berubah, file bukan project ditolak |
| E11 | `11_undo_redo` | Undo dan redo | Perubahan kueri, knowledge, dan preferensi dapat dibatalkan dan diulang, aksi baru mengosongkan redo |
| E12 | `12_tree_display` | Preferensi tampilan | Slot kosong, id, cornerstone, pewarnaan, dan rule pada hasil mengikuti preferensi |
| E13 | `13_file_errors` | File rusak | JSON rusak, format salah, dan klausa invalid dilaporkan dengan lokasi, project aktif tidak berubah |
| E14 | `14_large_dataset` | Skala | Dataset 2000 baris menghasilkan 1601 node, dapat disimpan, dibuka, diinferensi, dan divalidasi |

## Skenario GUI

| ID | File | Interaksi | Pemeriksaan |
|---|---|---|---|
| G01 | `01_welcome_create` | Mengetik kesimpulan default di dialog Welcome lalu Enter | Dialog tertutup, root berisi default, project belum dirty |
| G02 | `02_query_correct_undo` | Klik bar Q, ketik, Enter, klik Start, koreksi lewat dialog, klik Undo dan Redo | Draft, kesimpulan, jumlah node, dirty, dan riwayat sesuai setiap langkah |
| G03 | `03_revision_case` | Koreksi yang tidak memiliki klausa pembeda | Node revisi ditambahkan dan kesimpulan berubah |
| G04 | `04_menu_keyboard` | Alt+E, panah, Enter, Esc, klik header menu, hover, Alt+H | Menu dan dialog yang terbuka sesuai |
| G05 | `05_copy_text` | Alt+T, Ctrl+A, Ctrl+C, Alt+R, drag seleksi | Isi clipboard sesuai teks Tree, tabel Result, dan seleksi parsial |
| G06 | `06_save_open_restore` | Drag splitter, Save, New, Open, drag splitter lagi | Tree, tab, draft, hasil, dan proporsi panel pulih, riwayat undo kosong, splitter terbatas 0.15 sampai 0.85 |
| G07 | `07_unsaved_flow` | Ctrl+N dengan perubahan, klik Cancel, Ctrl+N, klik Discard | Cancel mempertahankan project, Discard melanjutkan ke New |
| G08 | `08_prefs_csv` | Dialog preferensi, tema gelap, font, Ctrl+Z, Insert CSV | Tema dan font berubah lalu kembali lewat undo, laporan CSV terbuka |
| G09 | `09_manual_edit` | Double click hasil dan node tree, klik slot kosong, isi form, Add Node, Validate, Close | Node bertambah, validasi lulus, kesimpulan baru dipakai inferensi |

## Kriteria Lulus

Seluruh skenario CLI tidak memiliki selisih dengan expected output, dan seluruh skenario GUI berakhir dengan exit code 0 tanpa baris `EXPECT FAILED`.

## Di Luar Cakupan Otomatis

Hal yang tidak dapat diverifikasi otomatis tercantum di `docs/testing/QC_CHECKLIST.md`.
