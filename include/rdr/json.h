/**
 * @file json.h
 * @brief ADT JsonValue beserta parser dan writer JSON (RFC 8259).
 *
 * Parser dan writer diimplementasikan secara iteratif dengan stack eksplisit,
 * sehingga dokumen yang sangat dalam (misalnya pohon RDR dengan rantai
 * panjang) tidak menyebabkan stack overflow.
 *
 * Writer selalu menghasilkan pretty print dengan indentasi dua spasi dan
 * urutan key sesuai urutan penyisipan. Array yang seluruh elemennya skalar
 * ditulis dalam satu baris agar mudah dibaca, misalnya ["demam", "batuk"].
 *
 * Kepemilikan: setiap JsonValue yang dimasukkan ke array atau object menjadi
 * milik container tersebut dan ikut dibebaskan oleh json_free.
 */

#ifndef RDR_JSON_H
#define RDR_JSON_H

#include <stddef.h>

/**
 * @brief Jenis nilai JSON.
 */
typedef enum {
    JSON_NULL,    /**< null */
    JSON_BOOL,    /**< true atau false */
    JSON_NUMBER,  /**< bilangan */
    JSON_STRING,  /**< string UTF-8 */
    JSON_ARRAY,   /**< array */
    JSON_OBJECT   /**< object dengan key berurutan */
} JsonType;

/**
 * @brief Nilai JSON.
 *
 * Untuk array, items berisi elemen. Untuk object, keys dan items paralel.
 */
typedef struct JsonValue {
    JsonType type;              /**< Jenis nilai. */
    int boolean;                /**< Nilai untuk JSON_BOOL. */
    double number;              /**< Nilai untuk JSON_NUMBER. */
    char *string;               /**< Nilai untuk JSON_STRING. */
    struct JsonValue **items;   /**< Elemen array atau nilai object. */
    char **keys;                /**< Key object (NULL untuk array). */
    size_t count;               /**< Jumlah elemen atau pasangan. */
    size_t cap;                 /**< Kapasitas array internal. */
    int line;                   /**< Baris posisi nilai di sumber (1-based, 0 jika dibuat manual). */
    int column;                 /**< Kolom posisi nilai di sumber (1-based). */
} JsonValue;

/* ===================================================================== */
/* Construction                                                          */
/* ===================================================================== */

/** @brief Membuat nilai null. */
JsonValue *json_new_null(void);

/** @brief Membuat nilai boolean. */
JsonValue *json_new_bool(int value);

/** @brief Membuat nilai bilangan. */
JsonValue *json_new_number(double value);

/** @brief Membuat nilai string (string disalin). NULL diperlakukan sebagai string kosong. */
JsonValue *json_new_string(const char *value);

/** @brief Membuat array kosong. */
JsonValue *json_new_array(void);

/** @brief Membuat object kosong. */
JsonValue *json_new_object(void);

/** @brief Menambahkan elemen ke akhir array dan mengambil alih kepemilikannya. */
void json_array_push(JsonValue *array, JsonValue *value);

/**
 * @brief Menetapkan pasangan key dan nilai pada object.
 *
 * Jika key sudah ada, nilai lama dibebaskan dan diganti tanpa mengubah
 * urutan. Jika belum ada, pasangan ditambahkan di akhir.
 */
void json_object_set(JsonValue *object, const char *key, JsonValue *value);

/** @brief Membebaskan nilai beserta seluruh isinya. Aman dipanggil dengan NULL. */
void json_free(JsonValue *value);

/* ===================================================================== */
/* Access                                                                */
/* ===================================================================== */

/**
 * @brief Mengambil nilai berdasarkan key pada object.
 * @return Nilai, atau NULL jika bukan object atau key tidak ada.
 */
JsonValue *json_object_get(const JsonValue *object, const char *key);

/**
 * @brief Memeriksa apakah nilai adalah bilangan bulat yang muat di int.
 * @param out Tempat hasil (boleh NULL).
 * @return 1 jika bilangan bulat, 0 jika tidak.
 */
int json_get_int(const JsonValue *value, int *out);

/** @brief Mengembalikan nama jenis nilai untuk pesan error. */
const char *json_type_name(JsonType type);

/* ===================================================================== */
/* Parsing and Writing                                                   */
/* ===================================================================== */

/**
 * @brief Mem-parse teks JSON.
 *
 * BOM UTF-8 di awal teks diabaikan. Pesan error memuat posisi baris dan
 * kolom, misalnya "line 3, column 7: expected ':' after object key".
 *
 * @param text Teks JSON.
 * @param len Panjang teks dalam byte.
 * @param err Buffer pesan error (boleh NULL).
 * @param errlen Ukuran buffer pesan error.
 * @return Nilai hasil parse, atau NULL jika teks tidak valid.
 */
JsonValue *json_parse(const char *text, size_t len, char *err, size_t errlen);

/**
 * @brief Menulis nilai sebagai JSON pretty print diakhiri baris baru.
 * @return String baru milik pemanggil.
 */
char *json_write(const JsonValue *value);

#endif /* RDR_JSON_H */
