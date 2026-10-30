/**
 * @file strutil.h
 * @brief Utilitas string, buffer string dinamis, daftar string, dan alokasi memori.
 *
 * Modul paling dasar yang dipakai seluruh modul lain. Fungsi alokasi xmalloc,
 * xcalloc, dan xrealloc menghentikan program dengan pesan error jika memori
 * habis, sehingga pemanggil tidak perlu memeriksa nilai NULL.
 *
 * Kepemilikan memori: setiap fungsi yang mengembalikan char* baru (misalnya
 * str_dup) menyerahkan kepemilikan kepada pemanggil, yang wajib membebaskannya
 * dengan free().
 */

#ifndef RDR_STRUTIL_H
#define RDR_STRUTIL_H

#include <stdarg.h>
#include <stddef.h>

/* ===================================================================== */
/* Memory Allocation                                                     */
/* ===================================================================== */

/**
 * @brief Mengalokasikan memori sebesar size byte.
 * @param size Ukuran memori dalam byte. Nilai 0 diperlakukan sebagai 1.
 * @return Pointer ke memori baru. Tidak pernah NULL.
 */
void *xmalloc(size_t size);

/**
 * @brief Mengalokasikan memori untuk count elemen berukuran size dan mengisinya dengan nol.
 * @param count Jumlah elemen.
 * @param size Ukuran tiap elemen dalam byte.
 * @return Pointer ke memori baru yang sudah dinolkan. Tidak pernah NULL.
 */
void *xcalloc(size_t count, size_t size);

/**
 * @brief Mengubah ukuran blok memori.
 * @param ptr Blok memori lama (boleh NULL).
 * @param size Ukuran baru dalam byte. Nilai 0 diperlakukan sebagai 1.
 * @return Pointer ke blok memori dengan ukuran baru. Tidak pernah NULL.
 */
void *xrealloc(void *ptr, size_t size);

/* ===================================================================== */
/* String Helpers                                                        */
/* ===================================================================== */

/**
 * @brief Menduplikasi string.
 * @param s String sumber. Jika NULL, hasilnya juga NULL.
 * @return Salinan baru milik pemanggil.
 */
char *str_dup(const char *s);

/**
 * @brief Menduplikasi paling banyak n byte pertama dari string.
 * @param s String sumber (tidak boleh NULL).
 * @param n Jumlah byte maksimum yang disalin.
 * @return Salinan baru yang selalu diakhiri byte nol.
 */
char *str_ndup(const char *s, size_t n);

/**
 * @brief Membuat salinan rentang [begin, end) tanpa whitespace di awal dan akhir.
 * @param begin Awal rentang.
 * @param end Akhir rentang (eksklusif).
 * @return Salinan baru yang sudah di-trim. Bisa berupa string kosong.
 */
char *str_trim_range(const char *begin, const char *end);

/**
 * @brief Membuat salinan string tanpa whitespace di awal dan akhir.
 * @param s String sumber (tidak boleh NULL).
 * @return Salinan baru yang sudah di-trim.
 */
char *str_trim_dup(const char *s);

/**
 * @brief Memeriksa apakah string kosong atau hanya berisi whitespace.
 * @param s String yang diperiksa. NULL dianggap kosong.
 * @return 1 jika kosong atau hanya whitespace, 0 jika tidak.
 */
int str_is_blank(const char *s);

/**
 * @brief Membandingkan dua string tanpa membedakan huruf besar dan kecil (hanya ASCII).
 * @return 1 jika sama, 0 jika berbeda.
 */
int str_iequals(const char *a, const char *b);

/**
 * @brief Mem-parse seluruh string sebagai bilangan desimal.
 *
 * Format yang diterima: tanda opsional, digit, titik desimal opsional, dan
 * digit desimal. Notasi eksponen, heksadesimal, inf, dan nan ditolak agar
 * perilaku sama di semua platform.
 *
 * @param s String yang di-parse.
 * @param out Tempat menyimpan hasil (boleh NULL jika hanya ingin memeriksa).
 * @return 1 jika seluruh string merupakan bilangan valid, 0 jika tidak.
 */
int str_parse_number(const char *s, double *out);

/**
 * @brief Membuat string baru dengan format gaya printf.
 * @param fmt Format string.
 * @return String baru milik pemanggil.
 */
char *str_printf(const char *fmt, ...);

/**
 * @brief Menyalin string ke buffer berukuran tetap dengan pemotongan aman.
 * @param dst Buffer tujuan.
 * @param size Ukuran buffer tujuan dalam byte.
 * @param src String sumber.
 */
void str_copy(char *dst, size_t size, const char *src);

/**
 * @brief Menulis pesan berformat ke buffer berukuran tetap.
 *
 * Fungsi aman untuk dipanggil dengan dst NULL atau size 0, yang berarti pesan
 * dibuang. Dipakai oleh seluruh modul untuk melaporkan pesan error.
 *
 * @param dst Buffer tujuan (boleh NULL).
 * @param size Ukuran buffer tujuan.
 * @param fmt Format string gaya printf.
 */
void str_set_error(char *dst, size_t size, const char *fmt, ...);

/* ===================================================================== */
/* Dynamic String Buffer                                                 */
/* ===================================================================== */

/**
 * @brief Buffer string yang tumbuh otomatis.
 *
 * Invarian: jika data tidak NULL, data[len] selalu bernilai nol.
 */
typedef struct {
    char *data;  /**< Isi buffer, selalu diakhiri byte nol jika tidak NULL. */
    size_t len;  /**< Panjang isi tanpa byte nol penutup. */
    size_t cap;  /**< Kapasitas alokasi dalam byte. */
} StrBuf;

/** @brief Menginisialisasi buffer kosong tanpa alokasi. */
void strbuf_init(StrBuf *sb);

/** @brief Membebaskan memori buffer dan mengembalikannya ke keadaan kosong. */
void strbuf_free(StrBuf *sb);

/** @brief Mengosongkan isi buffer tanpa membebaskan kapasitas. */
void strbuf_clear(StrBuf *sb);

/** @brief Menambahkan string ke akhir buffer. */
void strbuf_append(StrBuf *sb, const char *s);

/** @brief Menambahkan n byte pertama dari s ke akhir buffer. */
void strbuf_append_n(StrBuf *sb, const char *s, size_t n);

/** @brief Menambahkan satu karakter ke akhir buffer. */
void strbuf_append_char(StrBuf *sb, char c);

/** @brief Menambahkan teks berformat gaya printf ke akhir buffer. */
void strbuf_appendf(StrBuf *sb, const char *fmt, ...);

/** @brief Versi va_list dari strbuf_appendf. */
void strbuf_appendv(StrBuf *sb, const char *fmt, va_list args);

/**
 * @brief Mengambil isi buffer sebagai string milik pemanggil.
 *
 * Buffer kembali kosong setelah pemanggilan. Hasil tidak pernah NULL.
 *
 * @return String hasil yang wajib dibebaskan pemanggil.
 */
char *strbuf_steal(StrBuf *sb);

/**
 * @brief Mengembalikan isi buffer sebagai string read-only.
 * @return Isi buffer atau string kosong jika belum ada alokasi.
 */
const char *strbuf_cstr(const StrBuf *sb);

/* ===================================================================== */
/* String List                                                           */
/* ===================================================================== */

/**
 * @brief Daftar string dinamis. Setiap elemen dimiliki oleh daftar.
 */
typedef struct {
    char **items;  /**< Array elemen. */
    size_t count;  /**< Jumlah elemen. */
    size_t cap;    /**< Kapasitas array. */
} StrList;

/** @brief Menginisialisasi daftar kosong. */
void strlist_init(StrList *list);

/** @brief Membebaskan seluruh elemen dan array daftar. */
void strlist_free(StrList *list);

/** @brief Menghapus seluruh elemen tanpa membebaskan kapasitas array. */
void strlist_clear(StrList *list);

/** @brief Menambahkan salinan string ke akhir daftar. */
void strlist_push(StrList *list, const char *s);

/** @brief Menambahkan string ke akhir daftar dan mengambil alih kepemilikannya. */
void strlist_push_owned(StrList *list, char *s);

/** @brief Menambahkan string berformat gaya printf ke akhir daftar. */
void strlist_pushf(StrList *list, const char *fmt, ...);

/**
 * @brief Menghapus elemen pada indeks tertentu dan menggeser elemen sesudahnya.
 * @param index Indeks elemen. Indeks di luar rentang diabaikan.
 */
void strlist_remove_at(StrList *list, size_t index);

/**
 * @brief Mengganti elemen pada indeks tertentu dengan salinan string baru.
 * @param index Indeks elemen. Indeks di luar rentang diabaikan.
 */
void strlist_set(StrList *list, size_t index, const char *s);

/** @brief Menyalin seluruh isi src ke dst. Isi dst sebelumnya dibuang. */
void strlist_copy(StrList *dst, const StrList *src);

/**
 * @brief Mencari indeks string yang sama persis.
 * @return Indeks elemen atau -1 jika tidak ditemukan.
 */
long strlist_index_of(const StrList *list, const char *s);

/**
 * @brief Menggabungkan seluruh elemen dengan pemisah.
 * @param sep Pemisah antar elemen.
 * @return String baru milik pemanggil.
 */
char *strlist_join(const StrList *list, const char *sep);

/* ===================================================================== */
/* Integer List                                                          */
/* ===================================================================== */

/**
 * @brief Daftar integer dinamis.
 */
typedef struct {
    int *items;    /**< Array elemen. */
    size_t count;  /**< Jumlah elemen. */
    size_t cap;    /**< Kapasitas array. */
} IntList;

/** @brief Menginisialisasi daftar kosong. */
void intlist_init(IntList *list);

/** @brief Membebaskan array daftar. */
void intlist_free(IntList *list);

/** @brief Menghapus seluruh elemen tanpa membebaskan kapasitas. */
void intlist_clear(IntList *list);

/** @brief Menambahkan nilai ke akhir daftar. */
void intlist_push(IntList *list, int value);

/** @brief Menyalin isi src ke dst. */
void intlist_copy(IntList *dst, const IntList *src);

/**
 * @brief Mencari indeks nilai tertentu.
 * @return Indeks elemen atau -1 jika tidak ditemukan.
 */
long intlist_index_of(const IntList *list, int value);

#endif /* RDR_STRUTIL_H */
