# Format File

Aplikasi memakai dua format file: project JSON (`*.rdr.json`) untuk menyimpan knowledge beserta preferensi dan sesi, serta dataset CSV untuk akuisisi batch. Parser dan writer kedua format ditulis sendiri (`src/core/json.c` dan `src/core/csv.c`).

## 1. Project JSON (`*.rdr.json`)

File ditulis dalam UTF-8 dengan pretty print dua spasi dan urutan key yang tetap, sehingga mudah dibaca dan rapi saat dibandingkan dengan diff. Penulisan dilakukan secara atomik (ke file sementara lalu di-rename) agar file lama tidak rusak jika penyimpanan gagal.

### 1.1 Contoh

```json
{
  "format": "rdr-kb",
  "version": 1,
  "next_id": 4,
  "root": {
    "id": 0,
    "rule": [],
    "conclusion": "Miskin",
    "cornerstone": [],
    "true": {
      "id": 1,
      "rule": ["uang > 100"],
      "conclusion": "Butuh Duit",
      "cornerstone": ["uang > 100"],
      "true": {
        "id": 2,
        "rule": ["budi = kaya", "pendiam"],
        "conclusion": "Banyak Duit",
        "cornerstone": ["budi = kaya", "uang > 100", "pendiam"],
        "true": null,
        "false": {
          "id": 3,
          "rule": [],
          "conclusion": "Kaya Raya",
          "cornerstone": ["uang > 100"],
          "revises": 1,
          "true": null,
          "false": null
        }
      },
      "false": null
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
    "ui_theme": "light",
    "ui_help_language": "id"
  },
  "session": {
    "query_draft": ["uang > 100"],
    "last_query": ["uang > 100"],
    "last_evaluated": 3,
    "last_concluded": 3,
    "view": {
      "query_tab": "clause",
      "result_tab": "tree",
      "split": 0.42
    }
  }
}
```

### 1.2 Bagian Header

| Key | Wajib | Keterangan |
|---|---|---|
| `format` | ya | Selalu `"rdr-kb"` |
| `version` | ya | Versi format, saat ini `1` |
| `next_id` | tidak | Id untuk node berikutnya. Jika lebih kecil dari id terbesar, nilai dihitung ulang. |
| `root` | ya | Node root |
| `preferences` | tidak | Nilai preferensi. Key yang hilang memakai nilai default. |
| `session` | tidak | Kondisi sesi terakhir |

### 1.3 Node

Pohon disimpan bersarang mengikuti struktur aslinya. Parser dan writer bersifat iteratif sehingga pohon yang sangat dalam tidak menyebabkan stack overflow.

| Key | Wajib | Keterangan |
|---|---|---|
| `id` | ya | Bilangan bulat unik. Root wajib 0. |
| `rule` | ya | Array teks klausa. Array kosong berarti `T`. Rule root wajib kosong. |
| `conclusion` | ya | Teks tidak kosong |
| `cornerstone` | tidak | Array teks klausa. Wajib konsisten (tanpa kontradiksi). |
| `revises` | tidak | Id node yang direvisi. Hanya ada pada node revisi. |
| `true` | tidak | Node exception atau `null` |
| `false` | tidak | Node alternatif atau `null`. Wajib `null` untuk root. |

### 1.4 Preferensi

| Key | Tipe | Default | Pilihan |
|---|---|---|---|
| `tree_show_empty_slots` | boolean | `true` | |
| `tree_show_node_id` | boolean | `true` | |
| `tree_show_cornerstone` | boolean | `false` | |
| `tree_highlight` | boolean | `true` | |
| `result_show_rule` | boolean | `true` | |
| `acquisition_clause_selection` | string | `"editable"` | `editable`, `strict` |
| `acquisition_revision_strategy` | string | `"revision_node"` | `revision_node`, `edit_conclusion`, `reject` |
| `ui_font_size` | integer | `16` | 12 sampai 28 |
| `ui_theme` | string | `"light"` | `light`, `dark` |
| `ui_help_language` | string | `"id"` | `id`, `en` |

Key yang tidak dikenal atau bernilai tidak valid diabaikan dengan peringatan.

### 1.5 Sesi

| Key | Keterangan |
|---|---|
| `query_draft` | Isi panel kueri, termasuk klausa yang belum valid |
| `last_query` | Kueri yang terakhir dijalankan, atau `null` |
| `last_evaluated`, `last_concluded` | Hasil inferensi terakhir. Saat file dibuka, inferensi dijalankan ulang dan nilai ini dipakai untuk mendeteksi perbedaan. |
| `view.query_tab` | `clause` atau `query` |
| `view.result_tab` | `result` atau `tree` |
| `view.split` | Proporsi lebar panel kiri, 0.15 sampai 0.85 |

### 1.6 Validasi Saat Membuka

File ditolak dengan pesan berlokasi (nomor baris atau id node) jika JSON rusak, `format` atau `version` salah, ada node tanpa kesimpulan, id duplikat, klausa tidak valid, atau cornerstone kontradiktif. Project yang sedang terbuka tidak berubah jika pembukaan gagal.

## 2. Dataset CSV

Dataset dipakai oleh menu Edit > Insert CSV dan perintah CLI `import-csv`.

```csv
case,conclusion
"demam; suhu >= 38",Flu
"demam; suhu >= 38; batuk; sesak_napas",Pneumonia
"batuk; !demam",Alergi
```

| Aturan | Keterangan |
|---|---|
| Header | Baris pertama wajib `case,conclusion` (tanpa membedakan huruf besar dan kecil) |
| Pemisah | Koma, dengan quoting RFC 4180 (field diapit tanda kutip ganda, tanda kutip di dalam field ditulis `""`) |
| Kolom `case` | Klausa kueri yang dipisah titik koma |
| Kolom `conclusion` | Kesimpulan yang benar, tidak boleh kosong |
| Baris kosong | Diabaikan |
| Encoding | UTF-8, BOM di awal file diabaikan |

Setiap baris diproses berurutan. Baris yang gagal (klausa tidak valid, fakta kontradiktif, jumlah kolom salah, kesimpulan kosong) dilaporkan dengan nomor barisnya tanpa menghentikan baris lain.
