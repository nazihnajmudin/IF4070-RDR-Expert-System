# Panduan Pengguna

Panduan pemakaian aplikasi RDR Expert System. Konsep RDR dan aturan penulisan klausa dijelaskan lebih rinci di [RDR_SEMANTICS.md](RDR_SEMANTICS.md). Halaman yang sama juga tersedia di dalam aplikasi lewat menu Help (F1) dalam bahasa Indonesia dan Inggris.

## 1. Menjalankan Aplikasi

```bash
./build/linux/rdr-expert                          # Linux atau WSL2
./build/linux/rdr-expert examples/pajak.rdr.json  # langsung membuka project
```

Pada Windows, double click `rdr-expert.exe` atau seret file `*.rdr.json` ke atas exe.

Tanpa file, aplikasi menampilkan dialog **Welcome**. Ketik kesimpulan default lalu tekan Enter untuk membuat knowledge base baru, atau pilih Open Project untuk membuka file yang sudah ada.

## 2. Tampilan Utama

```
┌ File  Edit  Preference  Help ─────────────────────────────────────┐
│ ┌Clause│Query┐                     ┌Result│Tree┐                   │
│ │ 1. demam              [×] │ [Start] │ ✔ Pneumonia             │ │
│ │ 2. suhu = 39          [×] │ [Clear] │ Conclusion  Rule        │ │
│ │ 3. batuk              [×] │         │ Pneumonia   #2 batuk... │ │
│ │                           │ [Undo]  │ Flu         #1 demam... │ │
│ │                           │ [Redo]  │ Sehat       #0 T        │ │
│ └───────────────────────────┘         └─────────────────────────┘ │
│ Q [demam; suhu >= 38.5                                ] [Enter]   │
│ diagnosis.rdr.json   10 nodes   Conclusion: Pneumonia             │
└───────────────────────────────────────────────────────────────────┘
```

| Bagian | Fungsi |
|---|---|
| Menubar | File, Edit, Preference, Help |
| Panel kiri | Tab Clause (daftar klausa) dan tab Query (teks bebas, satu klausa per baris) |
| Kolom tengah | Start, Clear, Undo, Redo |
| Panel kanan | Tab Result (kesimpulan) dan tab Tree (pohon knowledge) |
| Bar Q | Input klausa baru |
| Status bar | Nama file, tanda modified, jumlah node, dan pesan terakhir. Tombol Details muncul jika pesan memiliki rincian. |

Lebar kedua panel dapat diubah dengan menggeser celah di antara panel dan kolom tengah.

## 3. Menjalankan Kueri

1. Ketik klausa pada bar Q, pisahkan dengan titik koma, lalu tekan Enter. Contoh: `demam; suhu >= 38.5; batuk`.
2. Klausa tampil di tab Clause. Klik sebuah klausa untuk mengeditnya, atau tekan tombol × untuk menghapusnya. Klausa tidak valid ditampilkan merah dengan penjelasan saat kursor diarahkan ke atasnya.
3. Tab Query menampilkan kueri yang sama sebagai teks bebas yang dapat diedit langsung.
4. Tekan **Start** (F5) untuk menjalankan reasoning. **Clear** mengosongkan kueri dan hasil.

Ringkasan penulisan klausa:

| Jenis | Contoh |
|---|---|
| Boolean | `demam`, `!demam` |
| Literal | `warna = merah`, `warna != merah` |
| Numerik | `suhu >= 38.5`, `umur < 17`, `x != 0` |

## 4. Membaca Hasil

**Tab Result** menampilkan kesimpulan akhir bertanda centang, lalu tabel kesimpulan dari node yang menyala (terbaru di atas) beserta id dan rule penghasilnya.

**Tab Tree** menampilkan seluruh pohon. Anak T (exception) selalu di atas anak F (alternatif), dan slot kosong ditandai `∅`. Setelah Start:

| Tampilan | Arti |
|---|---|
| tebal dengan latar hijau | node kesimpulan akhir |
| hijau | node yang menyala |
| oranye | node yang dievaluasi tetapi salah |
| warna biasa | node yang tidak dievaluasi |

Isi kedua tab dapat diseleksi dengan drag mouse atau Shift+klik, diseleksi seluruhnya dengan Ctrl+A, lalu disalin dengan Ctrl+C. Double click sebuah node di tab Tree untuk membukanya di Manual Edit.

## 5. Mengoreksi Kesimpulan

Jika kesimpulan salah, pilih **Edit > Edit last query** lalu ketik kesimpulan yang benar.

- **Exception.** Dialog menampilkan klausa kueri yang tidak dimiliki cornerstone case node kesimpulan saat ini. Klausa tersebut menjadi rule node baru. Klausa dapat dihapus centangnya atau diedit untuk menggeneralisasi rule, misalnya `suhu = 37` menjadi `suhu < 38`. Pesan hijau berarti rule valid.
- **Kasus revisi.** Jika tidak ada klausa pembeda, dialog menjelaskan strategi revisi yang aktif dan dampaknya. Strategi default menambahkan node `T >> <kesimpulan baru>` sehingga kueri yang sama menghasilkan kesimpulan baru.

Tekan **Apply Correction** (atau Enter). Hasil langsung diperbarui.

## 6. Mengelola Knowledge

| Menu | Fungsi |
|---|---|
| File > New | Membuat knowledge base baru dengan kesimpulan default |
| File > Open, Save, Save As | Membuka dan menyimpan project `*.rdr.json`. Tree, preferensi, kueri, hasil terakhir, tab, dan lebar panel ikut tersimpan. |
| Edit > Undo, Redo | Membatalkan dan mengulang perubahan kueri, knowledge, dan preferensi |
| Edit > Insert CSV | Membangun knowledge dari dataset CSV (format di [FILE_FORMATS.md](FILE_FORMATS.md)). Laporan per baris ditampilkan setelahnya. |
| Edit > Manual Edit | Mengedit rule, kesimpulan, dan cornerstone node, menambah anak pada slot kosong, menghapus subtree, dan memvalidasi knowledge base |

Di Manual Edit, klik node untuk mengeditnya atau klik slot `∅` untuk langsung menambah anak di slot tersebut. Edit manual tidak melalui akuisisi RDR, jadi jalankan **Validate Knowledge Base** setelahnya.

Aplikasi meminta konfirmasi sebelum New, Open, atau keluar jika knowledge atau preferensi belum disimpan.

## 7. Preferensi

**Preference > Edit Preference** membuka dialog pengaturan:

| Kelompok | Preferensi |
|---|---|
| Tree | slot kosong, id node, cornerstone, pewarnaan hasil |
| Result | rule penghasil pada tabel kesimpulan |
| Acquisition | mode klausa exception (editable atau strict), strategi revisi |
| Appearance | ukuran font, tema terang atau gelap, bahasa Help |

**Preference > Import Preference** menyalin preferensi dari project lain. Preferensi tersimpan ke file saat project disimpan.

## 8. Keyboard

| Shortcut | Aksi |
|---|---|
| Ctrl+N, Ctrl+O, Ctrl+S, Ctrl+Shift+S | New, Open, Save, Save As |
| Ctrl+Z, Ctrl+Y | Undo, Redo (saat tidak sedang mengetik) |
| F5 | Start |
| F1 | Help |
| Esc | Menutup dialog atau menu |
| Alt+F, Alt+E, Alt+P, Alt+H | Membuka menu. Panah memilih item, Enter menjalankan. |
| Alt+Q, Alt+C, Alt+Y | Fokus ke bar Q, tab Clause, tab Query |
| Alt+R, Alt+T | Tab Result, tab Tree (siap untuk Ctrl+A dan Ctrl+C) |
| Alt+S, Alt+L, Alt+U, Alt+O | Start, Clear, Undo, Redo |

Tahan Alt untuk melihat huruf mnemonic yang digaris bawahi.

## 9. Program CLI

`rdr-cli` menjalankan perintah yang sama tanpa GUI, dari file skrip atau stdin.

```bash
./build/linux/rdr-cli                    # membaca perintah dari stdin
./build/linux/rdr-cli --echo skrip.txt   # menjalankan skrip dan menampilkan setiap perintah
./build/linux/rdr-cli --strict skrip.txt # berhenti dengan exit code 1 pada error pertama
```

| Perintah | Fungsi |
|---|---|
| `new <default>` | Knowledge base baru |
| `open <path>`, `save [path]` | Membuka dan menyimpan project |
| `add <klausa>`, `query <klausa>` | Menambah klausa atau mengganti seluruh kueri |
| `draft`, `remove <n>`, `clear` | Melihat, menghapus, dan mengosongkan kueri |
| `start`, `result`, `tree`, `plan` | Reasoning, hasil, tree, rencana koreksi |
| `correct <kesimpulan> [\| <rule>]` | Koreksi, opsional dengan rule pilihan |
| `rule`, `conclusion`, `cornerstone <id> <isi>` | Edit manual node |
| `add-child <id> true\|false <rule> \| <kesimpulan> [\| <cornerstone>]` | Menambah anak |
| `delete <id>`, `validate` | Menghapus subtree dan memvalidasi |
| `import-csv <path>`, `import-prefs <path>` | Impor dataset dan preferensi |
| `pref [<key> <nilai>]` | Melihat atau mengubah preferensi |
| `undo`, `redo`, `status`, `help`, `quit` | Lain-lain |

Contoh sesi:

```
new Sehat
query demam; suhu = 39
start
correct Flu
tree
save diagnosis
```

## 10. Contoh Project

| File | Domain | Kesimpulan default |
|---|---|---|
| `examples/diagnosis.rdr.json` dan `examples/diagnosis.csv` | Diagnosis penyakit sederhana | Sehat |
| `examples/pajak.rdr.json` dan `examples/pajak.csv` | Tarif pajak berdasarkan penghasilan dan status | 0% |

Project contoh dibangun dari dataset CSV di sebelahnya. Contoh kueri untuk dicoba: `demam; suhu = 39; batuk; sesak_napas` dan `penghasilan = 700; status = umkm`.
