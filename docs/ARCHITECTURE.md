# Arsitektur

## 1. Prinsip

- **Core tidak bergantung pada GUI.** Engine RDR, parser, dan format file berada di `src/core` dan dapat diuji tanpa window.
- **Satu controller untuk dua antarmuka.** GUI dan CLI sama-sama memakai `App` (`src/app/app.c`), sehingga perilaku yang diuji lewat CLI identik dengan perilaku GUI.
- **Domain bebas.** Tidak ada logika khusus domain. Domain hanya ditentukan oleh kesimpulan default root dan knowledge yang dibangun user.
- **ADT dengan kepemilikan memori yang jelas.** Setiap header di `include/rdr/` mendokumentasikan siapa yang memiliki dan membebaskan memori.
- **Iteratif untuk struktur dalam.** Penelusuran tree, parser JSON, dan writer JSON tidak memakai rekursi agar aman untuk rantai exception yang sangat panjang.

## 2. Lapisan dan Modul

```
           GUI (src/gui)                 CLI (src/cli)
                 \                           /
                  \                         /
                   App controller (src/app)
                   /        |          \
        kb_io, kb_csv     rdr        prefs           (src/core)
            |   |          |
          json csv     factbase, rdr_print
                           |
                 clause, query, numset
                           |
                        strutil            platform (src/platform)
```

| Modul | Header | Tanggung jawab |
|---|---|---|
| strutil | `strutil.h` | Alokasi aman, string, `StrBuf`, `StrList`, `IntList` |
| clause | `clause.h` | ADT `Clause` dan `ClauseList`, parser klausa, bentuk kanonik |
| query | `query.h` | Pemecahan teks kueri dan teks rule menjadi klausa |
| numset | `numset.h` | ADT `NumSet` (gabungan interval) untuk semantik numerik |
| factbase | `factbase.h` | Indeks fakta kueri, deteksi kontradiksi, evaluasi klausa rule |
| rdr | `rdr.h` | ADT `RdrNode` dan `RdrKb`, inferensi, difference list, koreksi, revisi, edit manual, validasi |
| rdr_print | `rdr_print.h` | Render tree menjadi baris teks Unicode dengan label T dan F |
| json | `json.h` | ADT `JsonValue`, parser dan pretty-print writer iteratif |
| csv | `csv.h` | Pembaca CSV RFC 4180 |
| prefs | `prefs.h` | Preferensi berbasis tabel deskriptor |
| kb_io | `kb_io.h` | Konversi project (tree, preferensi, sesi) ke dan dari JSON |
| kb_csv | `kb_csv.h` | Akuisisi batch dari CSV beserta laporannya |
| platform | `platform.h` | File UTF-8, penulisan atomik, listing folder, path Linux dan Windows |
| app | `app.h` | State project, draft kueri, aksi user, dirty flag, undo dan redo |

## 3. ADT Utama

| ADT | Isi | Catatan |
|---|---|---|
| `Clause` | jenis, operator, variabel, nilai teks, nilai bilangan | Boolean memakai `OP_EQ` untuk `v` dan `OP_NE` untuk `!v` |
| `ClauseList` | array klausa | Dimaknai sebagai konjungsi. Kosong berarti `T`. |
| `NumSet` | interval terurut, saling lepas, tidak bersentuhan | Mendukung irisan dan uji subset |
| `FactBase` | entri per (jenis, variabel) | Opaque. Dibangun dari kueri. |
| `RdrNode` | id, rule, kesimpulan, cornerstone, revises, induk, dua anak | Diubah hanya lewat fungsi `rdr_*` |
| `RdrKb` | root, `next_id`, jumlah node, `stamp`, indeks id | `stamp` naik setiap tree berubah |
| `RdrInference` | kueri, `last_evaluated`, `last_concluded`, trace, langkah, `stamp` | Koreksi ditolak jika `stamp` tidak lagi sama dengan KB |
| `Preferences` | array nilai per `PrefId` | Didefinisikan oleh tabel deskriptor |
| `App` | KB, preferensi, path, draft, hasil, tab, status, riwayat undo dan redo | Dipakai GUI dan CLI |

## 4. Alur Data

### 4.1 Start
1. Draft (teks klausa mentah) di-parse oleh `query_parse_list`.
2. `rdr_infer` membangun `FactBase`, memeriksa kontradiksi, lalu menelusuri tree.
3. GUI membangun ulang tampilan Result dan Tree dari `RdrInference`.

### 4.2 Koreksi
1. `app_correction_plan` menghitung difference list terhadap cornerstone `last_concluded`.
2. Dialog menampilkan klausa yang dapat dipilih atau penjelasan kasus revisi.
3. `app_correct` menyimpan snapshot undo, memanggil `rdr_correct`, lalu menjalankan ulang inferensi.

### 4.3 Simpan dan Buka
1. `app_build_session` menyusun draft, kueri terakhir, tab, dan proporsi panel.
2. `kb_to_json` dan `json_write` menghasilkan teks yang ditulis secara atomik oleh `platform_write_file`.
3. Saat dibuka, `kb_load_file` memvalidasi isi file, lalu `app_open` memulihkan preferensi, draft, tab, dan hasil inferensi.

### 4.4 Undo dan Redo
Sebelum setiap aksi yang mengubah draft, knowledge, atau preferensi, `App` menyimpan snapshot project dalam format JSON yang sama dengan file. Undo memulihkan snapshot tersebut termasuk hasil inferensinya. Riwayat dibatasi 64 langkah dan 96 MiB.

## 5. GUI

GUI memakai [Nuklear](https://github.com/Immediate-Mode-UI/Nuklear) (immediate mode) di atas GLFW dan OpenGL 1.x. Backend GLFW dan OpenGL untuk Nuklear ditulis sendiri.

| File | Tanggung jawab |
|---|---|
| `gui.h` | State bersama `Gui` dan antarmuka antar modul GUI |
| `gui_backend.c` | Window, antrean input, font atlas, clipboard, render OpenGL |
| `gui_theme.c` | Tema terang dan gelap |
| `gui_widgets.c` | Tab, tombol, mnemonic Alt, field edit, dialog window |
| `gui_textview.c` | Tampilan teks read-only yang dapat diseleksi dan disalin |
| `gui_menu.c` | Menubar, popup menu, shortcut, navigasi Alt |
| `gui_query_panel.c` | Tab Clause dan Query serta bar Q |
| `gui_result_panel.c` | Tab Result (tabel kesimpulan) dan Tree (berwarna) |
| `gui_actions.c` | Aksi File, Undo, Redo, alur unsaved changes |
| `gui_dialogs.c` | Welcome, New, Unsaved, laporan CSV, Help, About, Details |
| `gui_file_dialog.c` | File browser |
| `gui_correct_dialog.c` | Dialog Edit last query |
| `gui_manual_dialog.c` | Dialog Manual Edit |
| `gui_prefs_dialog.c` | Dialog Edit Preference yang dibangkitkan dari tabel deskriptor |
| `gui_testhook.c` | Skrip aksi, pemeriksaan `expect`, dan screenshot untuk pengujian |
| `main_gui.c` | Layout jendela utama, splitter, dan main loop |

Catatan desain GUI:
- **Event-driven.** Window hanya digambar ulang saat ada input. Antrean input memastikan klik dan ketikan cepat tidak hilang.
- **Geometri absolut.** Jendela utama dan Manual Edit memakai `nk_layout_space`, sehingga tepi panel, tab, tombol, dan bar Q sejajar presisi.
- **Monospace.** Seluruh teks memakai DejaVu Sans Mono, sehingga tabel Result dan tree dapat disejajarkan per karakter dan posisi seleksi teks dihitung dari kolom.

## 6. Menambah Preferensi Baru

1. Tambahkan konstanta `PREF_*` sebelum `PREF_COUNT` di `include/rdr/prefs.h`.
2. Tambahkan entri deskriptor dengan urutan yang sama di `src/core/prefs.c` (key JSON, label, kelompok, deskripsi, tipe, default, rentang atau pilihan).
3. Baca nilainya dengan `prefs_get` di modul yang membutuhkan.

Default, validasi, penyimpanan JSON, Import Preference, dan dialog Edit Preference otomatis mengikuti tabel deskriptor.

## 7. Konvensi Kode

- C99 tanpa ekstensi compiler pada kode sendiri. Seluruh kode menghasilkan nol warning.
- Dokumentasi fungsi, prosedur, dan tipe data ditulis di header dalam bahasa Indonesia. Nama identifier dalam bahasa Inggris.
- Komentar blok proses berupa nama proses dalam bahasa Inggris, misalnya `/* Tree Traversal */`. Komentar di samping baris berupa anotasi singkat dalam bahasa Indonesia.
- Prefix nama fungsi mengikuti modulnya, misalnya `rdr_`, `clause_`, `app_`, dan `gui_`.
