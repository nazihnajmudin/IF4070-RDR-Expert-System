/**
 * @file app.h
 * @brief Controller aplikasi yang dipakai bersama oleh GUI dan CLI.
 *
 * App menyimpan seluruh state satu project yang sedang dibuka: knowledge
 * base, preferensi, path file, status perubahan (dirty), draft kueri, hasil
 * inferensi terakhir, tab yang aktif, dan pesan status. Seluruh aksi user
 * (New, Open, Save, Start, koreksi, edit manual, impor) dijalankan lewat
 * fungsi app_* sehingga GUI hanya bertugas menampilkan state dan meneruskan
 * aksi.
 *
 * Undo dan redo: sebelum setiap aksi yang mengubah draft kueri, knowledge
 * base, atau preferensi, App menyimpan snapshot project (format JSON yang
 * sama dengan file). Undo memulihkan snapshot tersebut beserta draft dan
 * hasil inferensinya. Riwayat dibatasi jumlah entri dan total ukurannya, dan
 * dikosongkan saat New atau Open.
 *
 * Aturan dirty: hanya perubahan knowledge base dan preferensi yang membuat
 * project dianggap belum disimpan. Draft kueri, hasil inferensi, dan tab
 * aktif tetap ikut disimpan sebagai session saat Save, tetapi perubahannya
 * tidak memicu konfirmasi unsaved changes.
 */

#ifndef RDR_APP_H
#define RDR_APP_H

#include <stddef.h>

#include "rdr/kb_csv.h"
#include "rdr/kb_io.h"
#include "rdr/prefs.h"
#include "rdr/rdr.h"
#include "rdr/strutil.h"

/** Nama aplikasi untuk judul window. */
#define APP_NAME "RDR Expert System"
/** Ekstensi standar file project. */
#define APP_FILE_EXT ".rdr.json"

/**
 * @brief Tingkat pesan status.
 */
typedef enum {
    APP_STATUS_INFO,     /**< Informasi biasa. */
    APP_STATUS_WARNING,  /**< Peringatan. */
    APP_STATUS_ERROR     /**< Kegagalan aksi. */
} AppStatusLevel;

/** Jumlah maksimum langkah undo. */
#define APP_HISTORY_LIMIT 64
/** Total ukuran maksimum snapshot undo dalam byte. */
#define APP_HISTORY_BYTES (96UL * 1024UL * 1024UL)

/**
 * @brief Satu langkah riwayat undo atau redo.
 */
typedef struct {
    char *state;        /**< Snapshot project dalam JSON (milik entri). */
    size_t size;        /**< Panjang snapshot dalam byte. */
    int kb_change;      /**< 1 jika langkah mengubah KB atau preferensi. */
    char label[96];     /**< Nama aksi untuk pesan status. */
} AppHistoryEntry;

/**
 * @brief Tumpukan riwayat (entri terakhir adalah yang terbaru).
 */
typedef struct {
    AppHistoryEntry *items;  /**< Array entri. */
    size_t count;            /**< Jumlah entri. */
    size_t cap;              /**< Kapasitas array. */
    size_t bytes;            /**< Total ukuran snapshot. */
} AppHistory;

/**
 * @brief State aplikasi.
 */
typedef struct {
    RdrKb *kb;                  /**< Knowledge base aktif. */
    Preferences prefs;          /**< Preferensi aktif. */
    char *file_path;            /**< Path file project, NULL jika belum pernah disimpan. */
    int dirty;                  /**< 1 jika KB atau preferensi berubah sejak disimpan. */
    StrList draft;              /**< Teks klausa mentah pada panel kueri. */
    RdrInference inference;     /**< Hasil inferensi terakhir. */
    int query_tab;              /**< Tab panel kueri yang aktif (SessionQueryTab). */
    int result_tab;             /**< Tab panel hasil yang aktif (SessionResultTab). */
    double split_ratio;         /**< Proporsi lebar panel kiri (lihat SESSION_DEFAULT_SPLIT). */
    AppHistory undo;            /**< Riwayat undo. */
    AppHistory redo;            /**< Riwayat redo. */
    char status[512];           /**< Pesan status terakhir. */
    AppStatusLevel status_level;/**< Tingkat pesan status. */
    StrList details;            /**< Rincian pesan terakhir, misalnya daftar error parse. */
    unsigned long revision;     /**< Bertambah setiap state berubah, untuk sinkronisasi GUI. */
} App;

/**
 * @brief Ringkasan rencana koreksi untuk dialog Edit last query.
 */
typedef struct {
    int available;              /**< 1 jika ada hasil inferensi yang dapat dikoreksi. */
    const char *conclusion;     /**< Kesimpulan saat ini (milik KB). */
    int concluded_id;           /**< Node sumber kesimpulan. */
    int evaluated_id;           /**< Node terakhir yang dievaluasi. */
    int attach_true;            /**< 1 jika node baru akan menjadi true child. */
    ClauseList difference;      /**< Difference list terhadap cornerstone. */
    int is_revision;            /**< 1 jika difference list kosong (kasus revisi). */
    int editable;               /**< 1 jika preferensi mengizinkan pemilihan klausa. */
    int strategy;               /**< Strategi revisi aktif (RdrRevisionStrategy). */
} AppCorrectionPlan;

/* ===================================================================== */
/* Lifecycle                                                             */
/* ===================================================================== */

/**
 * @brief Membuat App dengan KB baru "T >> default_conclusion".
 * @return App baru, atau NULL jika kesimpulan default kosong.
 */
App *app_create(const char *default_conclusion);

/** @brief Membebaskan App beserta seluruh isinya. */
void app_destroy(App *app);

/**
 * @brief Mengganti project aktif dengan KB baru.
 *
 * Preferensi tetap dipertahankan agar tampilan tidak berubah tiba-tiba.
 *
 * @return 1 jika berhasil, 0 jika kesimpulan default kosong.
 */
int app_new(App *app, const char *default_conclusion);

/**
 * @brief Membuka file project dan memulihkan tampilannya.
 *
 * Preferensi diterapkan, draft kueri dipulihkan, dan inferensi pada kueri
 * terakhir dijalankan ulang agar hasil dan pewarnaan tree tampil kembali.
 *
 * @return 1 jika berhasil, 0 jika gagal (state lama dipertahankan).
 */
int app_open(App *app, const char *path);

/**
 * @brief Menyimpan project ke path saat ini.
 * @return 1 jika berhasil, 0 jika gagal atau belum punya path.
 */
int app_save(App *app);

/**
 * @brief Menyimpan project ke path baru.
 *
 * Jika path tidak berakhiran .json, ekstensi .rdr.json ditambahkan.
 *
 * @return 1 jika berhasil, 0 jika gagal.
 */
int app_save_as(App *app, const char *path);

/**
 * @brief Membuat judul window, misalnya "RDR Expert System - medis.rdr.json*".
 * @return String baru milik pemanggil.
 */
char *app_title(const App *app);

/* ===================================================================== */
/* Query Draft                                                           */
/* ===================================================================== */

/**
 * @brief Menambahkan klausa dari teks kueri ke draft.
 *
 * Teks dipecah dengan semicolon atau baris baru. Teks yang sudah ada persis
 * di draft tidak ditambahkan lagi.
 *
 * @return Jumlah klausa yang ditambahkan.
 */
size_t app_draft_add(App *app, const char *text);

/** @brief Mengganti teks klausa pada indeks tertentu (di-trim). Teks kosong menghapusnya. */
void app_draft_set(App *app, size_t index, const char *text);

/** @brief Menghapus klausa pada indeks tertentu. */
void app_draft_remove(App *app, size_t index);

/** @brief Mengosongkan draft. */
void app_draft_clear(App *app);

/**
 * @brief Mengganti seluruh draft dari teks bebas.
 */
void app_draft_set_text(App *app, const char *text);

/**
 * @brief Menggabungkan draft menjadi teks kueri dengan pemisah "; ".
 * @return String baru milik pemanggil.
 */
char *app_draft_text(const App *app);

/**
 * @brief Memeriksa validitas klausa draft pada indeks tertentu.
 * @return 1 jika valid, 0 jika tidak.
 */
int app_draft_item_valid(const App *app, size_t index, char *err, size_t errlen);

/* ===================================================================== */
/* Reasoning                                                             */
/* ===================================================================== */

/**
 * @brief Menjalankan inferensi pada draft.
 *
 * Jika ada klausa tidak valid atau fakta kontradiktif, inferensi tidak
 * dijalankan dan rinciannya tersedia di app->details.
 *
 * @return 1 jika berhasil, 0 jika gagal.
 */
int app_start(App *app);

/** @brief Mengosongkan draft dan hasil inferensi. */
void app_clear(App *app);

/** @brief Memeriksa apakah ada hasil inferensi yang valid. */
int app_has_result(const App *app);

/** @brief Mengembalikan kesimpulan akhir, atau NULL jika belum ada hasil. */
const char *app_conclusion(const App *app);

/* ===================================================================== */
/* Knowledge Acquisition                                                 */
/* ===================================================================== */

/**
 * @brief Menyusun rencana koreksi untuk hasil inferensi terakhir.
 * @param plan Rencana tujuan. Wajib dibebaskan dengan app_correction_plan_free.
 * @return 1 jika ada hasil yang dapat dikoreksi, 0 jika tidak.
 */
int app_correction_plan(const App *app, AppCorrectionPlan *plan);

/** @brief Membebaskan isi rencana koreksi. */
void app_correction_plan_free(AppCorrectionPlan *plan);

/**
 * @brief Mengoreksi kesimpulan hasil inferensi terakhir.
 *
 * Rule kustom hanya dipakai jika preferensi mode klausa bernilai editable.
 * Setelah tree berubah, inferensi pada kueri terakhir dijalankan ulang.
 *
 * @param custom_rule Rule exception pilihan user, atau NULL untuk seluruh difference list.
 * @return Hasil koreksi.
 */
RdrAcqResult app_correct(App *app, const char *conclusion, const ClauseList *custom_rule);

/* ===================================================================== */
/* Manual Editing                                                        */
/* ===================================================================== */

/**
 * @brief Mengganti rule node dari teks (sintaks kueri, atau "T").
 * @return 1 jika berhasil, 0 jika gagal.
 */
int app_edit_rule(App *app, int id, const char *rule_text, char *err, size_t errlen);

/** @brief Mengganti kesimpulan node. */
int app_edit_conclusion(App *app, int id, const char *conclusion, char *err, size_t errlen);

/** @brief Mengganti cornerstone node dari teks kueri. */
int app_edit_cornerstone(App *app, int id, const char *case_text, char *err, size_t errlen);

/**
 * @brief Menambahkan node anak dari teks.
 * @param new_id Tempat id node baru (boleh NULL).
 */
int app_add_child(App *app, int parent_id, RdrSide side, const char *rule_text,
                  const char *conclusion, const char *case_text, int *new_id,
                  char *err, size_t errlen);

/**
 * @brief Menghapus node beserta subtree-nya.
 * @param removed Tempat jumlah node yang dihapus (boleh NULL).
 */
int app_delete_node(App *app, int id, size_t *removed, char *err, size_t errlen);

/**
 * @brief Memvalidasi KB dan menuliskan ringkasannya ke status.
 * @param out Laporan. Wajib dibebaskan dengan rdr_validation_free.
 */
void app_validate(App *app, RdrValidation *out);

/* ===================================================================== */
/* Import and Preferences                                                */
/* ===================================================================== */

/**
 * @brief Mengimpor dataset CSV ke KB aktif.
 * @param report Laporan. Wajib dibebaskan dengan csv_report_free.
 * @return 1 jika file dapat dibaca, 0 jika tidak.
 */
int app_import_csv(App *app, const char *path, CsvImportReport *report);

/**
 * @brief Mengimpor preferensi dari file project lain.
 *
 * Tidak menulis ke file. Project menjadi dirty jika ada nilai yang berubah.
 *
 * @return 1 jika berhasil, 0 jika gagal.
 */
int app_import_prefs(App *app, const char *path);

/**
 * @brief Mengganti seluruh preferensi (misalnya dari dialog Edit Preference).
 * @return 1 jika ada nilai yang berubah.
 */
int app_set_prefs(App *app, const Preferences *prefs);

/* ===================================================================== */
/* Undo and Redo                                                         */
/* ===================================================================== */

/** @brief Memeriksa apakah ada langkah yang dapat di-undo. */
int app_can_undo(const App *app);

/** @brief Memeriksa apakah ada langkah yang dapat di-redo. */
int app_can_redo(const App *app);

/**
 * @brief Membatalkan aksi terakhir.
 * @return 1 jika berhasil, 0 jika tidak ada riwayat.
 */
int app_undo(App *app);

/**
 * @brief Mengulang aksi yang terakhir dibatalkan.
 * @return 1 jika berhasil, 0 jika tidak ada riwayat.
 */
int app_redo(App *app);

/* ===================================================================== */
/* Status                                                                */
/* ===================================================================== */

/** @brief Menetapkan pesan status berformat gaya printf. Rincian lama dihapus. */
void app_set_status(App *app, AppStatusLevel level, const char *fmt, ...);

/** @brief Membangun session dari state saat ini. */
void app_build_session(const App *app, RdrSession *out);

#endif /* RDR_APP_H */
