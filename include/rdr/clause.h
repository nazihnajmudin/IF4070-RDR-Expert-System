/**
 * @file clause.h
 * @brief ADT Clause (klausa tunggal) dan ClauseList (konjungsi klausa).
 *
 * Klausa adalah unit pernyataan terkecil pada knowledge RDR. Ada tiga jenis:
 *
 *   Jenis     Contoh               Keterangan
 *   boolean   demam, !demam        variabel tanpa nilai, '!' menyatakan negasi
 *   literal   demam = tinggi       nilai berupa token teks, operator = atau !=
 *   numerik   suhu >= 38.5         nilai berupa bilangan, operator = != > < >= <=
 *
 * Grammar:
 *   clause   := boolean | compare
 *   boolean  := ["!"] IDENT
 *   compare  := IDENT OP VALUE
 *   OP       := "=" | "!=" | ">" | "<" | ">=" | "<="
 *   IDENT    := [A-Za-z_][A-Za-z0-9_]*   (case-sensitive, selain kata kunci "T")
 *   VALUE    := NUMBER | LITERAL
 *
 * Spasi di sekitar operator bersifat opsional saat input. Bentuk kanonik
 * selalu "var op value" dengan satu spasi, "var", atau "!var". Teks bilangan
 * asli dipertahankan (misalnya "38.50" tetap "38.50").
 *
 * Kata kunci "T" menyatakan rule yang selalu bernilai benar. Kata kunci ini
 * tidak direpresentasikan sebagai Clause, melainkan sebagai ClauseList kosong.
 */

#ifndef RDR_CLAUSE_H
#define RDR_CLAUSE_H

#include <stddef.h>

#include "rdr/strutil.h"

/** Teks kata kunci untuk rule yang selalu benar. */
#define CLAUSE_TRUE_KEYWORD "T"

/**
 * @brief Jenis klausa.
 */
typedef enum {
    CLAUSE_BOOL,     /**< Variabel boolean, misalnya demam atau !demam. */
    CLAUSE_LITERAL,  /**< Perbandingan teks, misalnya demam = tinggi. */
    CLAUSE_NUMERIC   /**< Perbandingan bilangan, misalnya suhu >= 38.5. */
} ClauseType;

/**
 * @brief Operator klausa.
 *
 * Untuk klausa boolean, OP_EQ berarti variabel positif (demam) dan OP_NE
 * berarti variabel negatif (!demam).
 */
typedef enum {
    OP_EQ,  /**< Sama dengan (=). */
    OP_NE,  /**< Tidak sama dengan (!=). */
    OP_GT,  /**< Lebih besar (>). */
    OP_LT,  /**< Lebih kecil (<). */
    OP_GE,  /**< Lebih besar atau sama dengan (>=). */
    OP_LE   /**< Lebih kecil atau sama dengan (<=). */
} ClauseOp;

/**
 * @brief Satu klausa hasil parsing.
 *
 * Kepemilikan: var dan value dimiliki oleh Clause dan dibebaskan oleh
 * clause_free.
 */
typedef struct {
    ClauseType type;  /**< Jenis klausa. */
    ClauseOp op;      /**< Operator klausa. */
    char *var;        /**< Nama variabel. */
    char *value;      /**< Teks nilai (literal atau bilangan asli). NULL untuk boolean. */
    double number;    /**< Nilai bilangan. Hanya bermakna untuk CLAUSE_NUMERIC. */
} Clause;

/**
 * @brief Mem-parse teks menjadi klausa.
 *
 * Whitespace di awal dan akhir teks diabaikan. Teks "T" ditolak karena
 * merupakan kata kunci rule kosong, bukan klausa.
 *
 * @param text Teks klausa.
 * @param out Tempat hasil. Hanya diisi jika parsing berhasil.
 * @param err Buffer pesan error (boleh NULL).
 * @param errlen Ukuran buffer pesan error.
 * @return 1 jika berhasil, 0 jika teks tidak valid.
 */
int clause_parse(const char *text, Clause *out, char *err, size_t errlen);

/** @brief Membebaskan isi klausa. Struct klausa sendiri tidak dibebaskan. */
void clause_free(Clause *c);

/** @brief Menyalin klausa secara mendalam ke dst. */
void clause_copy(Clause *dst, const Clause *src);

/** @brief Menuliskan bentuk kanonik klausa ke buffer. */
void clause_format(const Clause *c, StrBuf *sb);

/**
 * @brief Membuat bentuk kanonik klausa.
 * @return String baru milik pemanggil.
 */
char *clause_to_string(const Clause *c);

/**
 * @brief Memeriksa kesamaan makna dua klausa.
 *
 * Klausa numerik dibandingkan berdasarkan nilai bilangan, sehingga "x = 5"
 * sama dengan "x = 5.0".
 *
 * @return 1 jika sama, 0 jika berbeda.
 */
int clause_equals(const Clause *a, const Clause *b);

/** @brief Mengembalikan simbol teks operator, misalnya ">=". */
const char *clause_op_symbol(ClauseOp op);

/** @brief Mengembalikan nama jenis klausa untuk ditampilkan. */
const char *clause_type_name(ClauseType type);

/**
 * @brief Memeriksa apakah teks (setelah di-trim) adalah kata kunci "T".
 * @return 1 jika kata kunci, 0 jika bukan.
 */
int clause_is_true_keyword(const char *text);

/* ===================================================================== */
/* Clause List                                                           */
/* ===================================================================== */

/**
 * @brief Daftar klausa yang dimaknai sebagai konjungsi (AND).
 *
 * Daftar kosong bermakna T, yaitu selalu benar. Setiap elemen dimiliki oleh
 * daftar.
 */
typedef struct {
    Clause *items;  /**< Array klausa. */
    size_t count;   /**< Jumlah klausa. */
    size_t cap;     /**< Kapasitas array. */
} ClauseList;

/** @brief Menginisialisasi daftar kosong. */
void clauselist_init(ClauseList *list);

/** @brief Membebaskan seluruh klausa dan array daftar. */
void clauselist_free(ClauseList *list);

/** @brief Menghapus seluruh klausa tanpa membebaskan kapasitas. */
void clauselist_clear(ClauseList *list);

/**
 * @brief Memindahkan klausa ke akhir daftar.
 *
 * Kepemilikan isi c berpindah ke daftar, lalu c dikosongkan.
 */
void clauselist_push_move(ClauseList *list, Clause *c);

/** @brief Menambahkan salinan klausa ke akhir daftar. */
void clauselist_push_copy(ClauseList *list, const Clause *c);

/** @brief Menghapus klausa pada indeks tertentu. Indeks di luar rentang diabaikan. */
void clauselist_remove_at(ClauseList *list, size_t index);

/** @brief Menyalin seluruh isi src ke dst. Isi dst sebelumnya dibuang. */
void clauselist_copy(ClauseList *dst, const ClauseList *src);

/**
 * @brief Memeriksa apakah daftar memuat klausa yang sama makna dengan c.
 * @return 1 jika ada, 0 jika tidak.
 */
int clauselist_contains(const ClauseList *list, const Clause *c);

/**
 * @brief Memeriksa kesamaan dua daftar sebagai himpunan klausa.
 * @return 1 jika setiap klausa a ada di b dan sebaliknya.
 */
int clauselist_equals(const ClauseList *a, const ClauseList *b);

/**
 * @brief Menggabungkan bentuk kanonik seluruh klausa dengan pemisah.
 *
 * Daftar kosong menghasilkan string kosong.
 *
 * @return String baru milik pemanggil.
 */
char *clauselist_join(const ClauseList *list, const char *sep);

/**
 * @brief Membuat teks rule untuk ditampilkan.
 *
 * Klausa digabung dengan " AND ". Daftar kosong menghasilkan "T".
 *
 * @return String baru milik pemanggil.
 */
char *clauselist_rule_text(const ClauseList *list);

/** @brief Menambahkan bentuk kanonik setiap klausa ke akhir daftar string. */
void clauselist_to_strlist(const ClauseList *list, StrList *out);

#endif /* RDR_CLAUSE_H */
