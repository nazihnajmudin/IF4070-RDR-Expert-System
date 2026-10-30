/**
 * @file kb_io.h
 * @brief Penyimpanan dan pemuatan project RDR dalam format JSON.
 *
 * Satu file project (*.rdr.json) berisi tiga bagian:
 *   - root:        pohon RDR bersarang (setiap node memuat id, rule,
 *                  conclusion, cornerstone, revises opsional, true, false)
 *   - preferences: nilai preferensi (lihat prefs.h)
 *   - session:     draft kueri, kueri terakhir, node terakhir yang
 *                  dievaluasi dan disimpulkan, serta tab yang aktif
 *
 * Contoh ringkas:
 *   {
 *     "format": "rdr-kb",
 *     "version": 1,
 *     "next_id": 2,
 *     "root": {
 *       "id": 0, "rule": [], "conclusion": "Sehat", "cornerstone": [],
 *       "true": { "id": 1, "rule": ["demam"], ... }, "false": null
 *     },
 *     "preferences": { ... },
 *     "session": { ... }
 *   }
 *
 * Bagian preferences dan session bersifat opsional saat dimuat. Bagian yang
 * hilang diisi nilai default.
 */

#ifndef RDR_KB_IO_H
#define RDR_KB_IO_H

#include <stddef.h>

#include "rdr/clause.h"
#include "rdr/json.h"
#include "rdr/prefs.h"
#include "rdr/rdr.h"
#include "rdr/strutil.h"

/** Nilai field "format" pada file project. */
#define KB_FORMAT_NAME "rdr-kb"
/** Versi format file project yang didukung. */
#define KB_FORMAT_VERSION 1

/**
 * @brief Tab panel kueri.
 */
typedef enum {
    SESSION_QUERY_TAB_CLAUSE = 0,  /**< Tab daftar klausa. */
    SESSION_QUERY_TAB_TEXT = 1     /**< Tab free text. */
} SessionQueryTab;

/**
 * @brief Tab panel hasil.
 */
typedef enum {
    SESSION_RESULT_TAB_RESULT = 0,  /**< Tab daftar kesimpulan. */
    SESSION_RESULT_TAB_TREE = 1     /**< Tab tree. */
} SessionResultTab;

/**
 * @brief State sesi yang disimpan bersama project.
 */
typedef struct {
    StrList query_draft;    /**< Teks klausa mentah pada panel kueri. */
    int has_last_query;     /**< 1 jika ada kueri yang terakhir dijalankan. */
    ClauseList last_query;  /**< Kueri yang terakhir dijalankan. */
    int last_evaluated;     /**< Id node terakhir yang dievaluasi, -1 jika tidak ada. */
    int last_concluded;     /**< Id node sumber kesimpulan, -1 jika tidak ada. */
    int query_tab;          /**< Tab panel kueri yang aktif (SessionQueryTab). */
    int result_tab;         /**< Tab panel hasil yang aktif (SessionResultTab). */
    double split;           /**< Proporsi lebar panel kiri terhadap kedua panel (0.15 sampai 0.85). */
} RdrSession;

/** Proporsi default lebar panel kiri. */
#define SESSION_DEFAULT_SPLIT 0.42
/** Batas bawah proporsi panel kiri. */
#define SESSION_MIN_SPLIT 0.15
/** Batas atas proporsi panel kiri. */
#define SESSION_MAX_SPLIT 0.85

/** @brief Menginisialisasi session kosong. */
void rdr_session_init(RdrSession *s);

/** @brief Membebaskan isi session dan mengembalikannya ke keadaan kosong. */
void rdr_session_free(RdrSession *s);

/**
 * @brief Isi lengkap sebuah file project.
 */
typedef struct {
    RdrKb *kb;            /**< Knowledge base (milik struct). */
    Preferences prefs;    /**< Preferensi. */
    RdrSession session;   /**< State sesi. */
} RdrProject;

/** @brief Menginisialisasi project kosong tanpa KB. */
void rdr_project_init(RdrProject *p);

/** @brief Membebaskan isi project. */
void rdr_project_free(RdrProject *p);

/**
 * @brief Mengonversi project menjadi dokumen JSON.
 * @param session Boleh NULL jika session tidak disimpan.
 * @return Dokumen JSON baru milik pemanggil.
 */
JsonValue *kb_to_json(const RdrKb *kb, const Preferences *prefs, const RdrSession *session);

/**
 * @brief Membaca project dari dokumen JSON.
 *
 * @param doc Dokumen JSON.
 * @param out Project tujuan. Isi sebelumnya dibebaskan.
 * @param warnings Daftar tujuan peringatan non-fatal (boleh NULL).
 * @param err Buffer pesan error fatal (boleh NULL).
 * @param errlen Ukuran buffer pesan error.
 * @return 1 jika berhasil, 0 jika dokumen tidak valid.
 */
int kb_from_json(const JsonValue *doc, RdrProject *out, StrList *warnings, char *err, size_t errlen);

/**
 * @brief Menyimpan project ke file JSON (pretty print, ditulis secara atomik).
 * @return 1 jika berhasil, 0 jika gagal.
 */
int kb_save_file(const char *path, const RdrKb *kb, const Preferences *prefs,
                 const RdrSession *session, char *err, size_t errlen);

/**
 * @brief Memuat project dari file JSON.
 * @return 1 jika berhasil, 0 jika gagal.
 */
int kb_load_file(const char *path, RdrProject *out, StrList *warnings, char *err, size_t errlen);

/**
 * @brief Membaca hanya bagian preferences dari file project lain.
 *
 * Key yang tidak ada di file sumber mempertahankan nilai saat ini.
 *
 * @param prefs Preferensi yang akan diubah.
 * @param changed Tempat jumlah preferensi yang berubah (boleh NULL).
 * @return 1 jika berhasil, 0 jika file tidak valid atau tidak memuat preferences.
 */
int kb_import_prefs_file(const char *path, Preferences *prefs, size_t *changed,
                         StrList *warnings, char *err, size_t errlen);

#endif /* RDR_KB_IO_H */
