# Implementation Plan: RDR Expert System

Dokumen rencana eksekusi yang disusun berdasarkan `plan/PLAN.md` dan `plan/ui-plan.png`.

Status: **revisi 3, seluruh fase selesai, menunggu QC manual**. Revisi ini memasukkan jawaban review pertama dan kedua (bagian 11), penjelasan dosen tentang kasus revisi (bagian 2.5), dan menu Preference (bagian 7).

---

## Progres

| Fase | Status | Catatan |
|---|---|---|
| 0 Persiapan | selesai | GLFW 3.4 dan Nuklear 4.13.3 di-vendor, build Linux dan cross-compile Windows berjalan |
| 1 Core klausa dan fakta | selesai | strutil, clause, numset, query, factbase beserta unit test |
| 2 Engine RDR | selesai | inferensi, akuisisi, tiga strategi revisi, edit manual, validasi, render tree |
| 3 Persistensi dan CSV | selesai | JSON iteratif buatan sendiri, prefs berbasis deskriptor, kb_io, csv, kb_csv |
| 4 App dan CLI | selesai | controller App, platform, rdr-cli, undo dan redo |
| 5 GUI | selesai | dua putaran review visual sudah disetujui |
| 6 Windows | selesai | `scripts/build.bat` diuji lewat cmd.exe asli, exe statis berjalan lewat WSL interop |
| 7 E2E | selesai | 38 skenario lulus (CLI Linux dan Windows, GUI Linux, smoke test GUI Windows), lihat `docs/testing/E2E_REPORT.md` |
| 8 Dokumentasi | selesai | `README.md` dan `docs/`, contoh project, tinjauan konvensi penulisan |

Penyesuaian kecil saat implementasi:
- Output build berada di `build/linux/` dan `build/windows/`.
- Tab Query menampilkan satu klausa per baris (kotak teks Nuklear tidak mendukung soft wrap).

Hasil review visual pertama (sudah dikerjakan dan disetujui):
- Panah tampilan node diganti dari `->` menjadi `>>` (hanya tampilan, grammar klausa tidak berubah).
- Layout utama memakai geometri absolut sehingga tab, kotak panel, bar Q, dan tombol tengah sejajar.
- Tab Result berupa tabel dua kolom (Conclusion dan Rule). Tab Result dan Tree dapat diseleksi dan disalin (drag, Shift+klik, Ctrl+A, Ctrl+C).
- Menu kustom dengan Alt+F/E/P/H dan navigasi panah. Alt+Q/C/Y/R/T/S/L/U/O untuk komponen utama.
- Splitter untuk mengubah proporsi lebar panel, disimpan di session (`view.split`).
- Undo dan Redo berbasis snapshot di lapisan App (tombol, menu Edit, Ctrl+Z dan Ctrl+Y).
- Preferensi baru `ui_help_language` (id atau en) dan halaman Help dua bahasa.

---

## 0. Aturan Kerja

- Git: hanya operasi read-only (`status`, `diff`, `log`, `show`). Tidak ada `add`, `commit`, `push`, `checkout`, `stash`, maupun operasi tulis lainnya.
- Checkpoint: eksekusi berjalan dari fase 0 sampai fase 5, lalu **berhenti untuk review visual**. Perbaikan dan peningkatan visual dilakukan sesuai masukanmu. Fase 6 sampai 8 baru dikerjakan setelah kamu menyatakan GUI sudah sesuai.
- Konvensi dari `PLAN.md` (bahasa dokumentasi, gaya komen, larangan emoji/em dash/semicolon, larangan kalimat diawali "Ini") diterapkan sejak baris kode pertama.

---

## 1. Ringkasan Pemahaman

- Aplikasi GUI expert system berbasis **binary Single Classification RDR (SCRDR)**. Setiap node memiliki edge true (except) dan false (if-not).
- Seluruh modul dan main program ditulis dalam C (C99), dapat dikompilasi di Linux/WSL2 dan Windows, dan dijalankan dengan satu klik executable.
- Knowledge, preferensi, dan state sesi disimpan ke satu file JSON yang human-readable (pretty print). Domain hanya ditentukan oleh nilai default root `T -> <default>`.
- Ada tiga jenis klausa (boolean, literal, numerik) yang dievaluasi dengan Open World Assumption (OWA). Klausa dalam satu node digabung dengan AND.
- Akuisisi knowledge dilakukan lewat koreksi interaktif (termasuk kasus revisi), impor CSV, dan edit manual pada tree.
- Testing terdiri dari unit test, E2E (rancangan dalam md lalu dijalankan), dan QC manual oleh kamu.

---

## 2. Model RDR (disepakati)

### 2.1 Istilah
Dokumentasi memakai istilah **binary SCRDR** (Compton dan Jansen), dengan edge `true` (except) dan `false` (if-not).

### 2.2 Tepat dua slot edge per node
Setiap node memiliki tepat dua slot, `true_child` dan `false_child`, dan masing-masing boleh kosong. Exception kedua dan seterusnya dari satu node dirantai lewat false edge milik exception sebelumnya:

```
A
├─T─ E1          (exception pertama dari A)
│    ├─T─ ∅
│    └─F─ E2     (exception kedua dari A, dicek jika E1 false)
└─F─ ∅
```

### 2.3 Cornerstone case dan state RDR
Setiap node menyimpan **cornerstone case**, yaitu kueri yang menyebabkan node tersebut dibuat. Cornerstone root adalah kasus kosong. Setelah inferensi, RDR menyimpan:
- `last_evaluated`: node terakhir yang klausanya dievaluasi (true maupun false), yaitu ujung penelusuran reasoner
- `last_concluded`: node terakhir yang bernilai true (sumber kesimpulan akhir, pemilik cornerstone pembanding)
- `last_query`: kueri yang terakhir diproses

### 2.4 Akuisisi normal (exception)
Difference list `D = { c dalam kueri baru | c tidak terpenuhi oleh fakta cornerstone dari last_concluded }`.

Jika `D` tidak kosong, node baru dibuat dengan `rule = D` (atau hasil pilihan/edit user, lihat 2.6), `conclusion = koreksi`, dan `cornerstone = kueri baru`. Node ditempel pada `last_evaluated`:
- sebagai `true_child` jika `last_evaluated` bernilai true (sama dengan `last_concluded`)
- sebagai `false_child` jika `last_evaluated` bernilai false

Slot tersebut dijamin kosong karena inferensi berhenti tepat di sana.

### 2.5 Kasus revisi (D kosong)
Kasus revisi terjadi jika `D` kosong, yaitu ketika setiap klausa kueri baru sudah terpenuhi oleh cornerstone sehingga tidak ada klausa pembeda.

Dasar strateginya adalah posisi `last_concluded`. Node tersebut adalah node "paling kanan" yang menyala pada jalur penelusuran. Setelah node tersebut menyala, reasoner turun ke `true_child`-nya lalu menyusuri rantai false. Semua node di rantai itu bernilai false sampai ujung terdalam, yaitu `last_evaluated`. Jika `last_concluded` tidak punya `true_child`, maka `last_evaluated = last_concluded`.

Tiga strategi tersedia dan dipilih di Preference:

| Strategi | Status | Perilaku |
|---|---|---|
| **Revision node** | **default** | Tambahkan node `T -> <kesimpulan baru>` (rule kosong yang selalu true, cornerstone = kueri baru) pada ujung `last_evaluated`. Node menjadi `false_child` jika `last_evaluated` bernilai false, atau `true_child` jika `last_evaluated = last_concluded`. Hasilnya, kesimpulan `last_concluded` direvisi dengan rule yang sama. Kasus yang tertangkap oleh exception sebelumnya tidak terpengaruh. Node ini ditandai `"revises": <id last_concluded>`. |
| Edit conclusion | non-default (menyalahi prinsip RDR) | Ubah langsung `conclusion` milik `last_concluded`, dan ganti cornerstone-nya dengan kueri baru. Tidak ada node baru. |
| Reject | non-default | Tolak akuisisi dan minta user menambahkan fakta pembeda ke kueri agar `D` tidak kosong. |

Detail tambahan:
- Sebelum revisi diterapkan, dialog koreksi menampilkan peringatan dampaknya, misalnya "Revisi akan mengubah kesimpulan semua kasus yang berakhir di node #n".
- Rule kosong ditampilkan sebagai `T`. Karena itu, `T` menjadi kata kunci dan tidak boleh dipakai sebagai nama variabel.
- Validate KB (4.4) mengenali rantai `revises`. Cornerstone lama yang kini menghasilkan kesimpulan revisi dilaporkan sebagai "superseded" (info), bukan error.

### 2.6 Pemilihan dan edit klausa exception
Dialog koreksi menampilkan `D` (default: semua klausa tercentang). User boleh menghapus centang atau mengedit klausa, misalnya menggeneralisasi `suhu = 37` menjadi `suhu < 38`. Hasilnya divalidasi otomatis:
1. rule baru harus **terpenuhi** oleh kueri baru
2. rule baru harus **tidak terpenuhi** oleh cornerstone

Preference `acquisition.clause_selection` dapat mengunci mode ini menjadi `strict`, sehingga seluruh `D` selalu dipakai tanpa opsi edit.

### 2.7 Semantik `!` di bawah OWA
Klausa rule `!demam` hanya terpenuhi jika kueri **secara eksplisit** memuat fakta `!demam`. Ketiadaan fakta berarti unknown, bukan false.

### 2.8 Beberapa fakta numerik untuk variabel yang sama
Seluruh fakta numerik per variabel **diiriskan** (AND). Contohnya, kueri `suhu > 37 ; suhu < 39` menjadi himpunan `(37, 39)`, lalu diperiksa apakah himpunan itu subset dari himpunan klausa rule. Himpunan direpresentasikan sebagai gabungan interval agar `!=` dapat ditangani.

---

## 3. Spesifikasi Semantik Klausa

### 3.1 Grammar
```
clause   := boolean | compare
boolean  := ["!"] IDENT
compare  := IDENT OP VALUE            (spasi di sekitar OP opsional saat input, dinormalisasi saat disimpan)
OP       := "=" | "!=" | ">" | "<" | ">=" | "<="
IDENT    := [A-Za-z_][A-Za-z0-9_]*    (case-sensitive, kecuali kata kunci "T")
VALUE    := NUMBER | LITERAL
NUMBER   := bilangan bulat/desimal penuh, contoh -3, 38.5   -> klausa numerik
LITERAL  := satu token tanpa spasi selain NUMBER            -> klausa literal
```
- Tipe klausa ditentukan dari VALUE: jika seluruhnya numerik, klausa bertipe numerik.
- Klausa literal hanya mendukung `=` dan `!=`.
- Bentuk kanonik: `var op value` dengan satu spasi, atau `var` / `!var`. Teks angka asli dipertahankan.
- Kunci fakta adalah pasangan (tipe, variabel). Klausa tidak sejenis tidak pernah dibandingkan.

### 3.2 Tabel evaluasi (klausa rule terhadap fact base kueri)

| Klausa rule | Bernilai true jika |
|---|---|
| `T` (rule kosong) | selalu |
| `v` | fakta `v` ada |
| `!v` | fakta `!v` ada |
| `v = a` (literal) | ada fakta `v = a` |
| `v != a` (literal) | ada fakta `v = b` dengan `b != a`, atau ada fakta `v != a` |
| `v op n` (numerik) | ada fakta numerik untuk `v`, dan irisan himpunannya merupakan subset dari `{x : x op n}` |

Di luar kondisi di atas, nilainya unknown dan diperlakukan sebagai false.

### 3.3 Validasi konsistensi kueri (ditolak dengan pesan error)
- `v` dan `!v` muncul bersamaan
- `v = a` dan `v = b` dengan `a != b`, atau `v = a` dan `v != a`
- irisan fakta numerik suatu variabel kosong (contoh `x > 5 ; x < 3`)
- duplikat klausa identik cukup dideduplikasi tanpa error

---

## 4. Algoritma Inti

### 4.1 Inferensi
```
last_concluded = root ; last_evaluated = root ; trace = [root]
cur = root.true_child
while cur != NULL:
    last_evaluated = cur
    if satisfies(fact_base(query), cur.rule):
        last_concluded = cur ; trace.push(cur) ; cur = cur.true_child
    else:
        cur = cur.false_child
result = last_concluded.conclusion
```

### 4.2 Koreksi
```
if koreksi == last_concluded.conclusion -> tidak ada perubahan
D = difference(query, last_concluded.cornerstone)
if D tidak kosong -> akuisisi normal (2.4)
else              -> strategi revisi sesuai preference (2.5)
setelah tree berubah -> inferensi ulang last_query agar panel hasil langsung menampilkan kesimpulan baru
```

### 4.3 Akuisisi dari CSV
Setiap baris diproses secara berurutan. Inferensi dijalankan pada kueri baris tersebut. Jika hasilnya berbeda dengan kolom kesimpulan, koreksi 4.2 dijalankan dengan seluruh `D` (mode strict). Jika `D` kosong, dipakai strategi revisi dari preference.

Hasil impor berupa laporan berisi:
- jumlah baris yang sudah benar
- jumlah exception dan revisi yang ditambahkan
- baris yang gagal beserta alasannya

### 4.4 Edit manual dan validasi
Operasi yang tersedia:
- edit klausa dan kesimpulan node
- edit kesimpulan default root
- tambah anak pada slot kosong (true/false)
- hapus subtree (dengan konfirmasi)

**Validate KB** memeriksa bahwa setiap cornerstone masih menghasilkan kesimpulan node pemiliknya, dengan memperhitungkan rantai `revises`. Pelanggaran dilaporkan.

Traversal tree (save, load, print, free) diimplementasikan secara iteratif agar rantai panjang tidak menyebabkan stack overflow.

---

## 5. Keputusan Teknis

### 5.1 GUI stack: Nuklear + GLFW 3.4 + OpenGL 1.x, semuanya di-vendor
- Source GLFW (zlib license) dan `nuklear.h` (MIT/Public Domain) di-vendor ke `third_party/` dan dikompilasi bersama program.
- Hasilnya: Windows berupa 1 `.exe` statis, dan Linux hanya membutuhkan libX11/libGL yang sudah tersedia di WSLg.
- Font DejaVu Sans Mono di-embed agar karakter `├ └ │ ─ ∅` tampil benar.
- File dialog berupa file browser buatan sendiri di dalam aplikasi (`dirent` / `FindFirstFileW`).
- Path di Windows ditangani sebagai UTF-8 lewat wrapper `_wfopen`.

### 5.2 File knowledge: JSON (`*.rdr.json`), parser dan writer buatan sendiri
Writer selalu menghasilkan pretty print (indentasi 2 spasi, urutan key tetap) agar mudah dibaca dan rapi saat di-diff.

```json
{
  "format": "rdr-kb",
  "version": 1,
  "next_id": 4,
  "root": {
    "id": 0,
    "rule": [],
    "conclusion": "Sehat",
    "cornerstone": [],
    "true": {
      "id": 1,
      "rule": ["demam", "suhu >= 38"],
      "conclusion": "Flu",
      "cornerstone": ["demam", "suhu = 39", "batuk"],
      "true": null,
      "false": {
        "id": 3,
        "rule": [],
        "conclusion": "Demam Biasa",
        "cornerstone": ["demam"],
        "revises": 1,
        "true": null,
        "false": null
      }
    },
    "false": null
  },
  "preferences": {
    "tree_show_empty_slots": true,
    "tree_show_node_id": true,
    "tree_show_cornerstone": false,
    "tree_highlight": true,
    "result_show_rule": true,
    "acquisition_clause_selection": "editable",
    "acquisition_revision_strategy": "revision_node",
    "ui_font_size": 16,
    "ui_theme": "light"
  },
  "session": {
    "query_draft": ["demam", "suhu = 39"],
    "last_query": ["demam", "suhu = 39"],
    "last_evaluated": 1,
    "last_concluded": 1,
    "view": { "query_tab": "clause", "result_tab": "tree" }
  }
}
```
Field `revises` hanya muncul pada node revisi. Key yang tidak dikenal diabaikan dengan peringatan. Key `preferences` atau `session` yang hilang diisi dengan nilai default.

### 5.3 Format CSV
- Header wajib: `case,conclusion`. Delimiter koma dengan quoting RFC 4180.
- Kolom `case` berisi klausa yang dipisah `;`. Contoh: `"demam; suhu = 39; batuk",Flu`.
- Parser CSV ditulis sendiri.

### 5.4 Build
- C99, `-Wall -Wextra -Wpedantic`. Build test memakai `-fsanitize=address,undefined`.
- `Makefile` dengan target `all`, `gui`, `cli`, `test`, `e2e`, `windows` (cross-compile mingw dari WSL), `clean`, dan `dist`.
- `scripts/build.sh` (UNIX/WSL2) dan `scripts/build.bat` (Windows native, MinGW-w64 gcc seperti w64devkit/MSYS2).
- Output: `build/rdr-expert` (Linux) dan `build/rdr-expert.exe` (Windows, linked statis).
- `tools/embed.c` (buatan sendiri) mengonversi font dan teks Help menjadi array C saat build.

---

## 6. Arsitektur dan Struktur Folder

Prinsip: **core tidak tahu apa pun tentang GUI**. GUI dan CLI sama-sama memakai lapisan `app`, sehingga logika yang diuji E2E identik dengan yang dipakai GUI.

```
include/rdr/            header publik (dokumentasi lengkap dalam bahasa Indonesia)
src/core/
  strutil.c/.h          string helper, trim, split, dynamic buffer
  clause.c/.h           ADT Clause: parse, format kanonik, tipe, operator
  numset.c/.h           ADT NumSet: gabungan interval, intersect, subset
  factbase.c/.h         ADT FactBase: indeks fakta, validasi konsistensi, satisfies()
  query.c/.h            ADT ClauseList, parser kueri (split ; dan newline, trim, dedup)
  rdr.c/.h              ADT RdrNode, RdrKnowledgeBase: infer, difference, acquire, revise, edit manual, validate
  rdr_print.c/.h        render tree ke baris teks Unicode beserta status node dan opsi tampilan
  prefs.c/.h            ADT Preferences berbasis tabel deskriptor: default, validasi, konversi ke/dari JSON, import
  json.c/.h             JSON value, parser, pretty-print writer
  kb_io.c/.h            save/load KB, preferences, dan session ke/dari JSON
  csv.c/.h              CSV reader RFC 4180
  kb_csv.c/.h           akuisisi batch dari CSV beserta laporannya
src/app/
  app.c/.h              state aplikasi: KB, path, dirty flag, draft kueri, hasil terakhir, view state, command API
src/platform/
  platform.c/.h         abstraksi fs dan path untuk Linux dan Windows
src/gui/
  main_gui.c            entry point GUI
  gui_backend.c/.h      GLFW, OpenGL, Nuklear, font, tema, loop render
  gui_menu.c            menubar File / Edit / Preference / Help
  gui_query_panel.c     tab Clause dan Query, bar Q + enter, Start, Clear
  gui_result_panel.c    tab Result dan Tree
  gui_prefs_dialog.c    dialog Edit Preference yang dibangkitkan otomatis dari tabel deskriptor
  gui_dialogs.c/.h      New KB, file browser, koreksi, Manual Edit, laporan CSV, Help, konfirmasi
  gui_testhook.c/.h     (hanya untuk build test) skrip aksi dan screenshot PNG
src/cli/
  main_cli.c            rdr-cli: runner skenario untuk E2E dan pemakaian headless
tests/unit/             test_*.c per modul, framework assert minimal buatan sendiri
tests/e2e/              skenario, fixture, dan expected output
third_party/            nuklear, glfw, font (beserta LICENSE)
assets/help/            sumber teks halaman Help
examples/               contoh KB dan CSV untuk 2 domain
scripts/                build.sh, build.bat
tools/                  embed.c
docs/                   dokumentasi akhir
```

---

## 7. Desain GUI

```
┌ File  Edit  Preference  Help ──────────────────────────────────────┐
│ ┌Clause│Query┐                    ┌Result│Tree┐                     │
│ │ budi = kaya     [x]│  [Start]   │ ✔ Banyak Duit              │    │
│ │ uang > 100      [x]│  [Clear]   │     Banyak Duit            │    │
│ │ pendiam         [x]│            │     Butuh Duit             │    │
│ │                    │            │     Miskin                 │    │
│ └────────────────────┘            └────────────────────────────┘    │
│ [Q] [ayam = bertelur ; terbang                          ] [enter]   │
│ status: contoh.rdr.json* | 12 node | pesan info/error               │
└────────────────────────────────────────────────────────────────────┘
```

**Menu**
- **File**: New (dialog nilai default root), Open, Save, Save As, Exit. Ada konfirmasi jika ada perubahan yang belum disimpan.
- **Edit**:
  - *Edit last query*: dialog koreksi berisi kueri terakhir, kesimpulan saat ini, input kesimpulan yang benar, dan daftar `D` (centang/edit jika mode editable). Jika `D` kosong, dialog menampilkan strategi revisi yang aktif beserta peringatan dampaknya.
  - *Manual Edit*: pemilihan node pada tree, form klausa/kesimpulan/cornerstone, tombol Add True Child, Add False Child, Delete Subtree, dan Validate KB.
  - *Insert CSV*: memilih file, menjalankan akuisisi batch, lalu menampilkan laporan.
- **Preference**:
  - *Edit Preference*: dialog berisi seluruh preferensi (tabel di bawah), dengan tombol Apply, Cancel, dan Reset to Default.
  - *Import Preference*: memilih file project `.rdr.json` lain, lalu hanya objek `preferences` miliknya yang dibaca dan diterapkan. Key yang tidak ada di file sumber mempertahankan nilai saat ini. Key yang tidak dikenal atau nilainya invalid diabaikan dengan peringatan, dan status bar menampilkan ringkasan jumlah preferensi yang berubah.
  - Kedua aksi hanya mengubah state di memori dan membuat status dirty. Agar tersimpan ke file JSON, user tetap harus Save atau Save As.
- **Help**: halaman dokumentasi penggunaan (isi dari `assets/help/`) dan About.

**Panel kueri (kiri)**
- **Tab Clause**: setiap klausa dapat diedit inline dan punya tombol `x`. Klausa invalid ditandai merah.
- **Tab Query**: free text multiline yang tersinkron dua arah dengan tab Clause.
- **Bar Q + enter**: parse input, tambahkan klausa ke daftar, lalu kosongkan input.
- **Start** menjalankan inferensi. **Clear** mengosongkan daftar klausa dan hasil.

**Panel hasil (kanan)**
- **Tab Result**: baris teratas adalah kesimpulan akhir (tebal, berwarna, bertanda centang). Di bawahnya seluruh kesimpulan dari node yang menyala, dari yang terbaru di atas sampai default root di bawah, dengan rule penghasilnya.
- **Tab Tree**: setiap edge **selalu** diberi label `T` atau `F`, dan true-child selalu berada di atas false-child. Slot kosong ditampilkan sebagai `∅` secara default (dapat disembunyikan lewat Preference). Sebelum reasoning semua node berwarna default. Setelah reasoning: hijau untuk node yang menyala, oranye untuk node yang dievaluasi tetapi false, tebal untuk node kesimpulan akhir. Node revisi diberi penanda `rev #n`.

```
#0 (T -> Miskin)
├─T─ #1 (uang > 100 -> Butuh Duit)
│    ├─T─ #2 (budi = kaya -> Banyak Duit)
│    │    ├─T─ ∅
│    │    └─F─ ∅
│    └─F─ #3 (pendiam -> Hemat)
│         ├─T─ ∅
│         └─F─ ∅
└─F─ ∅
```

**Daftar preferensi**

| Preference | Default | Pilihan |
|---|---|---|
| Tampilkan slot kosong pada tree | ya | ya / tidak |
| Tampilkan id node | ya | ya / tidak |
| Tampilkan cornerstone pada tree | tidak | ya / tidak |
| Pewarnaan hasil reasoning | ya | ya / tidak |
| Tampilkan rule pada tab Result | ya | ya / tidak |
| Mode klausa exception | editable | editable / strict |
| Strategi revisi | revision node | revision node / edit conclusion / reject |
| Ukuran font | 16 | 12 sampai 28 |
| Tema | light | light / dark |

**Kemudahan menambah preferensi.** Seluruh preferensi didefinisikan dalam satu tabel deskriptor di `prefs.c`. Setiap entri berisi key JSON, label, tipe (bool, int dengan rentang, atau enum dengan daftar pilihan), dan nilai default. Default, validasi, serialisasi JSON, import, dan isi dialog Edit Preference semuanya dibangkitkan dari tabel tersebut. Untuk menambah preferensi baru cukup menambah satu entri tabel, lalu memakai nilainya di modul yang membutuhkan.

**Pemulihan tampilan saat Open**
- Tree dan preference dimuat lalu langsung diterapkan (font, tema, opsi tree).
- Panel kueri diisi dari `query_draft`.
- Inferensi pada `last_query` dijalankan ulang untuk membangun kembali tab Result dan pewarnaan tree. Jika id hasil hitung ulang berbeda dengan `last_evaluated`/`last_concluded` di file, hasil hitung ulang yang dipakai dan status bar menampilkan peringatan.
- Tab aktif dipulihkan dari `view`. Menu Edit last query langsung tersedia.
- Jika `session` tidak ada, panel hasil kosong dan tree tampil tanpa pewarnaan.

**Lainnya**
- Shortcut: Ctrl+N, Ctrl+O, Ctrl+S, Ctrl+Shift+S. Judul window: `RDR Expert System - <file>[*]`.
- Seluruh teks UI tidak memakai emoji, em dash, maupun semicolon (kecuali di dalam contoh sintaks kueri).

---

## 8. Strategi Testing

1. **Unit test** (`make test`): satu file test per modul core dan app, dijalankan dengan ASan/UBSan. Cakupannya meliputi:
   - parsing klausa, NumSet, tabel evaluasi, dan validasi kontradiksi
   - inferensi, akuisisi (sisi true/false, root), dan ketiga strategi revisi
   - edit manual dan validate (termasuk superseded)
   - round-trip JSON termasuk preferences dan session, pretty print stabil, serta JSON malformed
   - CSV dan impor batch
2. **E2E** (`make e2e`): rancangan ditulis di `docs/testing/E2E_TEST_PLAN.md`. Skenario dijalankan lewat `rdr-cli`, yang memakai lapisan `app` yang sama dengan GUI. Untuk GUI ada smoke test visual memakai test hook (skrip aksi dan screenshot PNG yang saya periksa), termasuk skenario Open file dengan session tersimpan. Build Windows dijalankan lewat WSL interop.
3. **QC manual (oleh kamu)**: checklist di `docs/testing/QC_CHECKLIST.md` (lihat bagian 10.2).

---

## 9. Daftar Task

Format: `[ID] Task -> deliverable / kriteria selesai`.

### Fase 0: Persiapan
- [T0.1] Dependensi build terinstal (dijalankan olehmu, lihat 10.1)
- [T0.2] Skeleton repo, `.gitignore`, `.editorconfig`
- [T0.3] Vendor Nuklear, GLFW 3.4, dan DejaVu Sans Mono beserta lisensinya -> versi tercatat di `third_party/README.md`
- [T0.4] `Makefile`, `scripts/build.sh`, `scripts/build.bat`, `tools/embed.c` -> `make` menghasilkan hello-window di Linux
- [T0.5] Framework unit test minimal -> `make test` berjalan

### Fase 1: Core klausa dan fakta
- [T1.1] `strutil` beserta unit test
- [T1.2] `clause` (termasuk kata kunci `T`) beserta unit test
- [T1.3] `numset` beserta unit test
- [T1.4] `query` beserta unit test
- [T1.5] `factbase` sesuai tabel 3.2 dan validasi 3.3, beserta unit test

### Fase 2: Engine RDR
- [T2.1] Struktur node dan KB: create dengan default, id stabil, free iteratif
- [T2.2] Inferensi 4.1 -> trace, last_evaluated, last_concluded, status per node
- [T2.3] Difference dan akuisisi normal 2.4, termasuk validasi rule hasil edit user (2.6)
- [T2.4] Kasus revisi 2.5 dengan tiga strategi beserta field `revises`
- [T2.5] Edit manual dan Validate KB (dengan superseded)
- [T2.6] `rdr_print` dengan opsi tampilan (slot kosong, id, cornerstone) dan metadata pewarnaan
- [T2.7] Unit test seluruh fase 2

### Fase 3: Persistensi, preferensi, dan CSV
- [T3.1] `json` parser dan pretty-print writer -> unit test valid/malformed/escape/UTF-8
- [T3.2] `prefs` berbasis tabel deskriptor -> default, validasi, konversi JSON, import parsial dengan laporan
- [T3.3] `kb_io` -> save/load tree, preferences, dan session dengan pesan error berlokasi, uji round-trip
- [T3.4] `csv` reader RFC 4180
- [T3.5] `kb_csv` akuisisi batch dan laporan

### Fase 4: Lapisan aplikasi dan CLI
- [T4.1] `app` controller -> state, dirty flag, draft kueri, view state, pemulihan session saat Open, seluruh command GUI, beserta unit test
- [T4.2] `platform` -> list dir, fopen UTF-8, path util
- [T4.3] `rdr-cli` runner skenario

### Fase 5: GUI
- [T5.1] Backend GLFW, Nuklear, font embedded, tema light/dark, ukuran font dinamis
- [T5.2] Layout utama sesuai sketsa dan status bar
- [T5.3] Panel kueri (tab Clause dan Query, bar Q, Start, Clear)
- [T5.4] Panel hasil: tab Result dan Tree berwarna
- [T5.5] Menu Preference: Edit Preference (dialog generik dari tabel deskriptor) dan Import Preference
- [T5.6] Menu File: New, file browser, Open (dengan pemulihan tampilan), Save, Save As, konfirmasi unsaved
- [T5.7] Edit > Edit last query (akuisisi normal dan revisi)
- [T5.8] Edit > Manual Edit dan Validate KB
- [T5.9] Edit > Insert CSV beserta laporan
- [T5.10] Help dan About
- [T5.11] Test hook (skrip aksi dan screenshot PNG) untuk verifikasi visual saya sendiri

### Checkpoint review visual (berhenti)
Saya menyerahkan:
- cara build dan run
- screenshot setiap layar dan dialog
- contoh KB sementara untuk dicoba
- daftar hal yang perlu kamu cek

Saya lalu melakukan iterasi perbaikan dan peningkatan visual sesuai masukanmu. Fase 6 baru dimulai setelah kamu menyatakan GUI sudah sesuai.

### Fase 6: Windows
- [T6.1] Cross-compile statis dengan mingw-w64, jalankan via WSL interop, ambil screenshot
- [T6.2] Review `build.bat`. Eksekusi native di Windows masuk QC

### Fase 7: E2E
- [T7.1] `docs/testing/E2E_TEST_PLAN.md` (skenario, langkah, expected)
- [T7.2] Implementasi skenario CLI dan smoke test visual GUI
- [T7.3] Jalankan, debug, dan perbaiki, lalu catat di `docs/testing/E2E_REPORT.md`

### Fase 8: Dokumentasi dan contoh
- [T8.1] Contoh KB dan CSV untuk 2 domain di `examples/`
- [T8.2] `docs/`: `README.md`, `USER_GUIDE.md`, `ARCHITECTURE.md`, `RDR_SEMANTICS.md`, `FILE_FORMATS.md`, `BUILD.md`, `testing/QC_CHECKLIST.md`
- [T8.3] Review akhir konvensi penulisan di seluruh kode dan dokumen

---

## 10. Prasyarat dan Batasan

### 10.1 Dependensi build (sudah terinstal)
```bash
sudo apt install libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl-dev mingw-w64
```

### 10.2 Hal yang tidak dapat saya verifikasi tanpa kamu (masuk QC)
- Build native di Windows dengan `build.bat`
- Double-click `.exe` di Windows Explorer, mesin tanpa toolchain, SmartScreen/antivirus
- Kenyamanan interaksi (keyboard, mouse, scroll, clipboard antar aplikasi), HiDPI, multi-monitor
- Penilaian estetika tampilan
- Linux desktop non-WSL

---

## 11. Keputusan Review Pertama

| No | Topik | Keputusan |
|---|---|---|
| 1 | GUI stack | Nuklear + GLFW vendored |
| 2 | JSON | parser dan writer buatan sendiri, output pretty print |
| 3 | Klausa exception | pilihan dan edit tersedia di dialog, bisa dikunci ke strict lewat Preference |
| 4 | D kosong | kasus revisi (2.5), default revision node |
| 5 | Semantik literal `!=` | disetujui |
| 6 | Irisan fakta numerik | disetujui |
| 7 | Token literal, IDENT, case-sensitive | disetujui |
| 8 | Edit last query | dialog koreksi kesimpulan kueri terakhir |
| 9 | Format CSV | disetujui |
| 10 | Tampilan tree | label `T`/`F` selalu tampil, slot kosong tampil secara default, dapat diatur lewat Preference |
| 11 | State sesi | disimpan di JSON, tampilan dipulihkan saat Open |
| 12 | Git | hanya operasi read-only |
| 13 | Checkpoint | berhenti setelah fase 5 untuk review visual |
| 14 | Strategi non-default kasus revisi | "usulan paksa tambah rule baru" berarti Reject |
| 15 | Letak Preference | menu bar `File | Edit | Preference | Help`, berisi Edit Preference dan Import Preference, tidak menulis file otomatis, mudah diperluas |
| 16 | Dependensi build | sudah terinstal |
