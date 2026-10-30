# Model dan Semantik RDR

Dokumen menjelaskan model knowledge, semantik klausa, algoritma inferensi, dan akuisisi knowledge yang diimplementasikan aplikasi. Implementasinya berada di `src/core/rdr.c`, `src/core/factbase.c`, `src/core/numset.c`, dan `src/core/clause.c`.

## 1. Pohon Binary SCRDR

Knowledge berupa pohon biner. Setiap node berisi:

| Bagian | Keterangan |
|---|---|
| id | Bilangan unik yang tidak dipakai ulang. Root selalu 0. |
| rule | Konjungsi (AND) klausa. Rule kosong ditulis `T` dan selalu benar. |
| conclusion | Teks bebas yang hanya ditampilkan dan tidak dipakai dalam reasoning |
| cornerstone | Kueri yang menyebabkan node dibuat |
| revises | Id node yang direvisi (hanya untuk node revisi) |
| true child | Exception, dievaluasi jika rule node benar |
| false child | Alternatif, dievaluasi jika rule node salah |

Setiap node memiliki tepat dua slot anak dan masing-masing boleh kosong. Exception kedua dan seterusnya dari satu node dirantai lewat false child milik exception sebelumnya.

```
#0 (T >> Sehat)
├─T─ #1 (demam >> Flu)
│    ├─T─ #2 (batuk >> Bronkitis)
│    │    ├─T─ ∅
│    │    └─F─ #3 (ruam >> Campak)
│    │         ├─T─ ∅
│    │         └─F─ ∅
│    └─F─ ∅
└─F─ ∅
```

Root selalu berbentuk `T >> <default>`. Root selalu benar sehingga slot false miliknya tidak pernah dievaluasi dan tidak dapat diisi.

## 2. Klausa

### 2.1 Grammar

```
clause   := boolean | compare
boolean  := ["!"] IDENT
compare  := IDENT OP VALUE
OP       := "=" | "!=" | ">" | "<" | ">=" | "<="
IDENT    := [A-Za-z_][A-Za-z0-9_]*      (case-sensitive, kecuali kata kunci T)
VALUE    := NUMBER | LITERAL
NUMBER   := bilangan bulat atau desimal, misalnya -3 atau 38.5
LITERAL  := satu kata tanpa spasi selain NUMBER
```

| Jenis | Contoh | Operator |
|---|---|---|
| boolean | `demam`, `!demam` | tanpa operator, `!` sebagai negasi |
| literal | `demam = tinggi` | `=` dan `!=` |
| numerik | `suhu >= 38.5` | `=`, `!=`, `>`, `<`, `>=`, `<=` |

Spasi di sekitar operator bersifat opsional. Bentuk kanonik selalu `var op value` dengan satu spasi. Teks bilangan asli dipertahankan, misalnya `38.50` tetap ditulis `38.50`, tetapi dibandingkan sebagai bilangan.

### 2.2 Kueri

Kueri adalah klausa fakta yang dipisah titik koma atau baris baru. Spasi di awal dan akhir setiap klausa dibuang dan potongan kosong diabaikan. Contohnya `orang_kaya; demam <= 38.6 ; budget = besar` menghasilkan tiga klausa. Klausa duplikat disimpan sekali.

### 2.3 Evaluasi dengan Open World Assumption

Fakta dikelompokkan per pasangan (jenis klausa, nama variabel). Klausa yang jenisnya berbeda tidak pernah dibandingkan walaupun nama variabelnya sama. Klausa rule hanya benar jika fakta membuktikannya. Fakta yang tidak ada berarti unknown dan diperlakukan sebagai salah.

| Klausa rule | Benar jika |
|---|---|
| `T` | selalu |
| `v` | ada fakta `v` |
| `!v` | ada fakta `!v` (ketiadaan `v` tidak cukup) |
| `v = a` | ada fakta `v = a` |
| `v != a` | ada fakta `v = b` dengan `b` berbeda dari `a`, atau ada fakta `v != a` |
| `v op n` | ada fakta numerik untuk `v`, dan irisan seluruh fakta numerik `v` merupakan subset dari `{x : x op n}` |

Himpunan numerik direpresentasikan sebagai gabungan interval (`NumSet`) agar operator `!=` dapat ditangani. Contoh:

| Fakta | Rule | Hasil | Alasan |
|---|---|---|---|
| `suhu = 39` | `suhu >= 38.5` | benar | {39} subset dari [38.5, inf) |
| `suhu <= 38.6` | `suhu >= 38` | salah | (-inf, 38.6] bukan subset dari [38, inf) |
| `suhu > 37; suhu < 39` | `suhu > 36` | benar | irisan (37, 39) subset dari (36, inf) |
| `x = 5` | `x != 4` | benar | {5} subset dari himpunan selain 4 |
| `suhu = tinggi` | `suhu > 30` | salah | jenis berbeda, tidak dibandingkan |

### 2.4 Fakta Kontradiktif

Kueri ditolak jika memuat fakta yang saling bertentangan:
- `v` dan `!v`
- `v = a` dan `v = b` dengan `a` berbeda dari `b`, atau `v = a` dan `v != a`
- irisan fakta numerik suatu variabel kosong, misalnya `x > 5; x < 3`

## 3. Inferensi

```
cur = root.true_child
last_concluded = root
last_evaluated = root
selama cur ada:
    last_evaluated = cur
    jika rule cur terpenuhi:
        last_concluded = cur
        cur = cur.true_child
    jika tidak:
        cur = cur.false_child
kesimpulan = last_concluded.conclusion
```

Hasil inferensi menyimpan kueri, `last_evaluated` (ujung penelusuran), `last_concluded` (sumber kesimpulan), daftar node yang menyala (trace), dan status setiap node yang dievaluasi. Tab Result menampilkan trace dari yang terbaru sampai root. Tab Tree mewarnai node yang menyala dan yang gagal.

## 4. Akuisisi Knowledge

### 4.1 Difference List

Saat user mengoreksi kesimpulan, aplikasi menghitung difference list:

```
D = { c dalam kueri | c tidak terpenuhi oleh fakta cornerstone milik last_concluded }
```

### 4.2 Exception

Jika `D` tidak kosong, node baru dibuat dengan rule `D`, kesimpulan koreksi, dan cornerstone kueri saat ini. Node ditempel pada `last_evaluated`:
- sebagai true child jika `last_evaluated` sama dengan `last_concluded` (node yang menyala belum memiliki exception)
- sebagai false child jika `last_evaluated` bernilai salah (ujung rantai exception)

Slot tersebut dijamin kosong karena inferensi berhenti tepat di sana.

Pada mode `editable`, user boleh menghapus centang atau mengedit klausa `D`, misalnya menggeneralisasi `suhu = 37` menjadi `suhu < 38`. Rule hasil edit wajib tidak kosong, terpenuhi oleh kueri saat ini, dan tidak terpenuhi oleh cornerstone. Pada mode `strict`, seluruh `D` selalu dipakai.

### 4.3 Kasus Revisi

Jika `D` kosong, tidak ada klausa yang membedakan kueri dari cornerstone. Strategi diatur lewat preferensi `acquisition_revision_strategy`:

| Strategi | Perilaku |
|---|---|
| `revision_node` (default) | Node `T >> <kesimpulan baru>` ditempel di ujung penelusuran (`last_evaluated`) dengan aturan sisi yang sama seperti exception, dan ditandai `revises` ke `last_concluded`. Kesimpulan lama direvisi dengan rule yang sama, sedangkan kasus yang tertangkap exception sebelumnya tidak terpengaruh. |
| `edit_conclusion` | Kesimpulan `last_concluded` diubah langsung dan cornerstone-nya diganti kueri saat ini. Strategi ini tidak sesuai prinsip RDR karena menulis ulang knowledge lama. |
| `reject` | Koreksi ditolak. User diminta menambahkan fakta pembeda ke kueri. |

Strategi revision node berasal dari penjelasan dosen mata kuliah. Node `last_concluded` adalah node paling kanan yang menyala pada jalur penelusuran, dan seluruh node setelahnya bernilai salah sampai ujung terdalam. Menambahkan `T >> <kesimpulan baru>` pada ujung tersebut membuat kueri yang sama menghasilkan kesimpulan baru.

### 4.4 Akuisisi dari CSV

Setiap baris dataset diproses berurutan seperti koreksi biasa dengan mode strict. Baris yang sudah menghasilkan kesimpulan benar dilewati. Baris yang tidak valid dilaporkan dengan nomor barisnya.

## 5. Edit Manual dan Validasi

Edit manual mengubah tree tanpa melalui akuisisi: mengganti rule, kesimpulan, dan cornerstone, menambah anak pada slot kosong, serta menghapus subtree. Root selalu mempertahankan rule `T`, cornerstone kosong, dan slot false kosong.

Validasi menjalankan inferensi pada setiap cornerstone dan memeriksa bahwa kesimpulannya sama dengan kesimpulan node pemiliknya:

| Temuan | Jenis | Arti |
|---|---|---|
| mismatch | error | Cornerstone kini menghasilkan kesimpulan lain, biasanya akibat edit manual |
| inconsistent case | error | Cornerstone berisi fakta kontradiktif |
| superseded | info | Cornerstone kini disimpulkan oleh node revisi dari node pemiliknya |
| no cornerstone | info | Node non-root tanpa cornerstone, biasanya dibuat lewat edit manual |
