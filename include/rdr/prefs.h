/**
 * @file prefs.h
 * @brief ADT Preferences berbasis tabel deskriptor.
 *
 * Seluruh preferensi didefinisikan pada satu tabel deskriptor di prefs.c.
 * Default, validasi, serialisasi JSON, import, dan dialog Edit Preference
 * pada GUI dibangkitkan dari tabel tersebut.
 *
 * Langkah menambah preferensi baru:
 *   1. tambahkan konstanta PREF_* baru tepat sebelum PREF_COUNT
 *   2. tambahkan entri deskriptor dengan urutan yang sama di prefs.c
 *   3. baca nilainya dengan prefs_get di modul yang membutuhkan
 *
 * Nilai setiap preferensi disimpan sebagai int. Untuk PREF_BOOL nilainya 0
 * atau 1, untuk PREF_INT berada di rentang [min_value, max_value], dan untuk
 * PREF_ENUM berupa indeks pilihan.
 */

#ifndef RDR_PREFS_H
#define RDR_PREFS_H

#include <stddef.h>

#include "rdr/json.h"
#include "rdr/strutil.h"

/**
 * @brief Tipe nilai preferensi.
 */
typedef enum {
    PREF_BOOL,  /**< Ya atau tidak. */
    PREF_INT,   /**< Bilangan bulat dalam rentang tertentu. */
    PREF_ENUM   /**< Salah satu dari daftar pilihan. */
} PrefType;

/**
 * @brief Identitas preferensi. Urutan wajib sama dengan tabel deskriptor.
 */
typedef enum {
    PREF_TREE_SHOW_EMPTY_SLOTS,    /**< Tampilkan slot kosong pada tree. */
    PREF_TREE_SHOW_NODE_ID,        /**< Tampilkan id node. */
    PREF_TREE_SHOW_CORNERSTONE,    /**< Tampilkan cornerstone pada tree. */
    PREF_TREE_HIGHLIGHT,           /**< Pewarnaan hasil reasoning pada tree. */
    PREF_RESULT_SHOW_RULE,         /**< Tampilkan rule pada tab Result. */
    PREF_ACQ_CLAUSE_SELECTION,     /**< Mode klausa exception. */
    PREF_ACQ_REVISION_STRATEGY,    /**< Strategi kasus revisi. */
    PREF_UI_FONT_SIZE,             /**< Ukuran font GUI. */
    PREF_UI_THEME,                 /**< Tema warna GUI. */
    PREF_UI_HELP_LANGUAGE,         /**< Bahasa halaman Help. */
    PREF_COUNT                     /**< Jumlah preferensi, bukan preferensi. */
} PrefId;

/** Pilihan PREF_ACQ_CLAUSE_SELECTION. */
enum {
    PREF_CLAUSE_EDITABLE = 0,  /**< User boleh memilih dan mengedit klausa exception. */
    PREF_CLAUSE_STRICT = 1     /**< Selalu memakai seluruh difference list. */
};

/** Pilihan PREF_UI_HELP_LANGUAGE. */
enum {
    PREF_HELP_ID = 0,  /**< Bahasa Indonesia. */
    PREF_HELP_EN = 1   /**< Bahasa Inggris. */
};

/** Pilihan PREF_UI_THEME. */
enum {
    PREF_THEME_LIGHT = 0,  /**< Tema terang. */
    PREF_THEME_DARK = 1    /**< Tema gelap. */
};

/*
 * Pilihan PREF_ACQ_REVISION_STRATEGY memakai nilai RdrRevisionStrategy
 * (RDR_REVISION_NODE, RDR_REVISION_EDIT_CONCLUSION, RDR_REVISION_REJECT).
 */

/**
 * @brief Deskripsi satu preferensi.
 */
typedef struct {
    const char *key;                    /**< Key JSON. */
    const char *label;                  /**< Label untuk GUI. */
    const char *group;                  /**< Nama kelompok untuk GUI. */
    const char *description;            /**< Penjelasan singkat untuk GUI. */
    PrefType type;                      /**< Tipe nilai. */
    int default_value;                  /**< Nilai default. */
    int min_value;                      /**< Nilai minimum (PREF_INT). */
    int max_value;                      /**< Nilai maksimum (PREF_INT). */
    const char *const *options;         /**< Nama pilihan di JSON (PREF_ENUM). */
    const char *const *option_labels;   /**< Label pilihan untuk GUI (PREF_ENUM). */
    int option_count;                   /**< Jumlah pilihan (PREF_ENUM). */
} PrefDescriptor;

/**
 * @brief Kumpulan nilai seluruh preferensi.
 */
typedef struct {
    int values[PREF_COUNT];  /**< Nilai per PrefId. */
} Preferences;

/** @brief Mengembalikan deskriptor preferensi. */
const PrefDescriptor *prefs_descriptor(PrefId id);

/** @brief Mengisi seluruh preferensi dengan nilai default. */
void prefs_init_defaults(Preferences *prefs);

/** @brief Mengambil nilai preferensi. */
int prefs_get(const Preferences *prefs, PrefId id);

/**
 * @brief Menetapkan nilai preferensi.
 * @return 1 jika nilai valid dan ditetapkan, 0 jika di luar rentang.
 */
int prefs_set(Preferences *prefs, PrefId id, int value);

/** @brief Memeriksa apakah dua kumpulan preferensi identik. */
int prefs_equal(const Preferences *a, const Preferences *b);

/**
 * @brief Mencari preferensi berdasarkan key JSON.
 * @return 1 jika ditemukan, 0 jika tidak.
 */
int prefs_find(const char *key, PrefId *out);

/**
 * @brief Mengembalikan teks nilai untuk ditampilkan atau disimpan.
 *
 * Contoh: "true", "16", atau "revision_node".
 *
 * @return String baru milik pemanggil.
 */
char *prefs_value_text(PrefId id, int value);

/**
 * @brief Menetapkan nilai dari teks (true/false, bilangan, atau nama pilihan).
 * @return 1 jika berhasil, 0 jika teks tidak valid.
 */
int prefs_set_from_text(Preferences *prefs, PrefId id, const char *text, char *err, size_t errlen);

/**
 * @brief Mengonversi preferensi menjadi object JSON sesuai urutan tabel.
 * @return Object JSON baru milik pemanggil.
 */
JsonValue *prefs_to_json(const Preferences *prefs);

/**
 * @brief Menerapkan nilai dari object JSON.
 *
 * Key yang tidak ada mempertahankan nilai saat ini. Key yang tidak dikenal
 * atau nilai yang tidak valid diabaikan dan dilaporkan ke warnings.
 *
 * @param object Object JSON sumber.
 * @param warnings Daftar tujuan peringatan (boleh NULL).
 * @return Jumlah preferensi yang nilainya berubah.
 */
size_t prefs_from_json(Preferences *prefs, const JsonValue *object, StrList *warnings);

#endif /* RDR_PREFS_H */
