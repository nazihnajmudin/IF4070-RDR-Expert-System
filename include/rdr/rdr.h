/**
 * @file rdr.h
 * @brief Engine binary Single Classification Ripple Down Rules (SCRDR).
 *
 * Knowledge direpresentasikan sebagai pohon biner. Setiap node berisi rule
 * (konjungsi klausa), kesimpulan berupa teks bebas, dan cornerstone case
 * (kueri yang menyebabkan node dibuat). Setiap node memiliki tepat dua slot
 * anak:
 *   - true child  (exception), dievaluasi jika rule node bernilai benar
 *   - false child (if-not), dievaluasi jika rule node bernilai salah
 *
 * Root selalu memiliki rule kosong (T) dan kesimpulan default domain, misalnya
 * "T >> Sehat". Exception kedua dan seterusnya dari satu node dirantai lewat
 * false child milik exception sebelumnya.
 *
 * Inferensi:
 *   cur = root.true_child, last_concluded = root, last_evaluated = root
 *   selama cur ada:
 *       last_evaluated = cur
 *       jika rule cur terpenuhi: last_concluded = cur, cur = cur.true_child
 *       jika tidak: cur = cur.false_child
 *   kesimpulan = last_concluded.conclusion
 *
 * Koreksi (akuisisi):
 *   D = klausa kueri yang tidak terpenuhi oleh cornerstone last_concluded
 *   Jika D tidak kosong, node exception baru (rule D atau rule pilihan user)
 *   ditempel pada last_evaluated: sebagai true child jika last_evaluated
 *   sama dengan last_concluded, atau sebagai false child jika tidak.
 *   Jika D kosong (kasus revisi), dipakai strategi revisi:
 *     - revision node: node "T >> kesimpulan baru" ditempel di posisi yang
 *       sama dan ditandai merevisi last_concluded
 *     - edit conclusion: kesimpulan last_concluded diubah langsung
 *     - reject: koreksi ditolak
 *
 * Setiap perubahan tree menaikkan stamp KB. Hasil inferensi menyimpan stamp
 * saat inferensi dibuat, sehingga koreksi dengan hasil inferensi yang sudah
 * usang dapat ditolak.
 */

#ifndef RDR_RDR_H
#define RDR_RDR_H

#include <stddef.h>

#include "rdr/clause.h"
#include "rdr/strutil.h"

/** Nilai field revises untuk node yang bukan node revisi. */
#define RDR_NO_REVISION (-1)

/* ===================================================================== */
/* Tree Structure                                                        */
/* ===================================================================== */

/**
 * @brief Node pohon RDR.
 *
 * Seluruh field dimiliki oleh node. Field boleh dibaca langsung, tetapi
 * perubahan wajib lewat fungsi rdr_* agar stamp dan indeks KB tetap benar.
 */
typedef struct RdrNode {
    int id;                        /**< Id unik dan stabil. Root selalu 0. */
    ClauseList rule;               /**< Rule node. Kosong berarti T. */
    char *conclusion;              /**< Kesimpulan, teks bebas yang tidak di-parse. */
    ClauseList cornerstone;        /**< Cornerstone case. Kosong untuk root. */
    int revises;                   /**< Id node yang direvisi, atau RDR_NO_REVISION. */
    struct RdrNode *parent;        /**< Induk, NULL untuk root. */
    struct RdrNode *true_child;    /**< Slot anak true (exception). */
    struct RdrNode *false_child;   /**< Slot anak false (if-not). */
} RdrNode;

/**
 * @brief Knowledge base RDR.
 */
typedef struct {
    RdrNode *root;         /**< Root dengan rule T dan kesimpulan default. */
    int next_id;           /**< Id yang akan dipakai node baru berikutnya. */
    size_t node_count;     /**< Jumlah node termasuk root. */
    unsigned long stamp;   /**< Bertambah setiap kali tree berubah. */
    RdrNode **index;       /**< Indeks id ke node. Slot NULL untuk id yang dihapus. */
    size_t index_cap;      /**< Kapasitas indeks. */
} RdrKb;

/**
 * @brief Sisi slot anak.
 */
typedef enum {
    RDR_SIDE_TRUE,   /**< Slot true child. */
    RDR_SIDE_FALSE   /**< Slot false child. */
} RdrSide;

/**
 * @brief Membuat KB baru yang hanya berisi root "T >> default_conclusion".
 * @param default_conclusion Kesimpulan default. Di-trim dan tidak boleh kosong.
 * @return KB baru, atau NULL jika kesimpulan kosong.
 */
RdrKb *rdr_kb_create(const char *default_conclusion);

/** @brief Membebaskan KB beserta seluruh node. Aman dipanggil dengan NULL. */
void rdr_kb_destroy(RdrKb *kb);

/**
 * @brief Mencari node berdasarkan id.
 * @return Node atau NULL jika tidak ada.
 */
RdrNode *rdr_kb_find(const RdrKb *kb, int id);

/**
 * @brief Menyisipkan node baru ke slot kosong.
 *
 * Fungsi tingkat rendah yang dipakai oleh akuisisi, edit manual, dan loader
 * file. Tidak memvalidasi makna rule terhadap cornerstone.
 *
 * @param parent Node induk.
 * @param side Slot tujuan. Slot harus kosong.
 * @param id Id node baru, atau -1 untuk memakai next_id.
 * @param rule Rule node (disalin).
 * @param conclusion Kesimpulan (di-trim, tidak boleh kosong).
 * @param cornerstone Cornerstone case (disalin, boleh NULL untuk kosong).
 * @param revises Id node yang direvisi atau RDR_NO_REVISION.
 * @param err Buffer pesan error (boleh NULL).
 * @param errlen Ukuran buffer pesan error.
 * @return Node baru, atau NULL jika gagal.
 */
RdrNode *rdr_kb_insert(RdrKb *kb, RdrNode *parent, RdrSide side, int id,
                       const ClauseList *rule, const char *conclusion,
                       const ClauseList *cornerstone, int revises,
                       char *err, size_t errlen);

/**
 * @brief Fungsi kunjungan untuk rdr_kb_foreach.
 * @return 0 untuk melanjutkan, nilai lain untuk berhenti.
 */
typedef int (*RdrVisitFn)(const RdrNode *node, int depth, void *user);

/**
 * @brief Mengunjungi seluruh node secara pre-order (node, true, false).
 *
 * Penelusuran iteratif sehingga aman untuk pohon yang sangat dalam.
 *
 * @return Nilai bukan nol pertama dari visit, atau 0 jika selesai.
 */
int rdr_kb_foreach(const RdrKb *kb, RdrVisitFn visit, void *user);

/* ===================================================================== */
/* Inference                                                             */
/* ===================================================================== */

/**
 * @brief Status node pada hasil inferensi.
 */
typedef enum {
    RDR_NODE_IDLE,    /**< Tidak dievaluasi. */
    RDR_NODE_FIRED,   /**< Dievaluasi dan rule bernilai benar. */
    RDR_NODE_FAILED   /**< Dievaluasi dan rule bernilai salah. */
} RdrNodeStatus;

/**
 * @brief Satu langkah evaluasi node.
 */
typedef struct {
    int node_id;  /**< Id node yang dievaluasi. */
    int fired;    /**< 1 jika rule bernilai benar. */
} RdrStep;

/**
 * @brief Hasil inferensi.
 */
typedef struct {
    int valid;               /**< 1 jika berisi hasil inferensi yang sah. */
    ClauseList query;        /**< Salinan kueri yang diproses. */
    int last_evaluated;      /**< Id node terakhir yang dievaluasi. */
    int last_concluded;      /**< Id node terakhir yang menyala (sumber kesimpulan). */
    IntList trace;           /**< Id node yang menyala, urut dari root. */
    RdrStep *steps;          /**< Seluruh langkah evaluasi secara berurutan. */
    size_t step_count;       /**< Jumlah langkah. */
    size_t step_cap;         /**< Kapasitas array langkah. */
    unsigned long stamp;     /**< Stamp KB saat inferensi dijalankan. */
} RdrInference;

/** @brief Menginisialisasi hasil inferensi kosong. */
void rdr_inference_init(RdrInference *inf);

/** @brief Membebaskan isi hasil inferensi dan mengembalikannya ke keadaan kosong. */
void rdr_inference_free(RdrInference *inf);

/**
 * @brief Menjalankan inferensi.
 *
 * Kueri yang berisi fakta kontradiktif ditolak.
 *
 * @param query Fakta kasus.
 * @param out Hasil inferensi. Isi sebelumnya dibuang.
 * @param errors Daftar tujuan pesan error (boleh NULL).
 * @return 1 jika berhasil, 0 jika kueri tidak konsisten.
 */
int rdr_infer(const RdrKb *kb, const ClauseList *query, RdrInference *out, StrList *errors);

/** @brief Mengembalikan status node pada hasil inferensi. */
RdrNodeStatus rdr_inference_status(const RdrInference *inf, int node_id);

/**
 * @brief Memeriksa apakah hasil inferensi masih sesuai dengan kondisi KB.
 * @return 1 jika valid dan stamp sama, 0 jika tidak.
 */
int rdr_inference_is_current(const RdrKb *kb, const RdrInference *inf);

/**
 * @brief Mengembalikan kesimpulan akhir hasil inferensi.
 * @return Teks kesimpulan milik node, atau NULL jika hasil tidak valid.
 */
const char *rdr_inference_conclusion(const RdrKb *kb, const RdrInference *inf);

/* ===================================================================== */
/* Knowledge Acquisition                                                 */
/* ===================================================================== */

/**
 * @brief Strategi koreksi saat difference list kosong (kasus revisi).
 */
typedef enum {
    RDR_REVISION_NODE,             /**< Tambah node "T >> kesimpulan baru" (default). */
    RDR_REVISION_EDIT_CONCLUSION,  /**< Ubah langsung kesimpulan node cornerstone. */
    RDR_REVISION_REJECT            /**< Tolak koreksi. */
} RdrRevisionStrategy;

/**
 * @brief Status hasil koreksi.
 */
typedef enum {
    RDR_ACQ_EXCEPTION_ADDED,    /**< Node exception baru ditambahkan. */
    RDR_ACQ_REVISION_ADDED,     /**< Node revisi "T >> ..." ditambahkan. */
    RDR_ACQ_CONCLUSION_EDITED,  /**< Kesimpulan node cornerstone diubah langsung. */
    RDR_ACQ_UNCHANGED,          /**< Kesimpulan koreksi sama dengan hasil saat ini. */
    RDR_ACQ_REJECTED,           /**< Kasus revisi ditolak oleh strategi reject. */
    RDR_ACQ_ERROR               /**< Input tidak valid atau hasil inferensi usang. */
} RdrAcqStatus;

/**
 * @brief Hasil koreksi.
 */
typedef struct {
    RdrAcqStatus status;  /**< Status koreksi. */
    int node_id;          /**< Node baru atau node yang diubah, -1 jika tidak ada. */
    char message[512];    /**< Penjelasan hasil untuk ditampilkan ke user. */
} RdrAcqResult;

/**
 * @brief Menghitung difference list.
 *
 * D berisi klausa kueri yang tidak terpenuhi oleh fakta cornerstone.
 *
 * @param query Kueri baru.
 * @param cornerstone Cornerstone case pembanding.
 * @param out Daftar tujuan. Isi sebelumnya dibuang.
 */
void rdr_difference(const ClauseList *query, const ClauseList *cornerstone, ClauseList *out);

/**
 * @brief Menghitung difference list untuk hasil inferensi.
 *
 * Pembandingnya adalah cornerstone milik node last_concluded.
 *
 * @return 1 jika berhasil, 0 jika hasil inferensi tidak valid.
 */
int rdr_inference_difference(const RdrKb *kb, const RdrInference *inf, ClauseList *out);

/**
 * @brief Memvalidasi rule exception pilihan user.
 *
 * Rule valid jika tidak kosong, terpenuhi oleh kueri, dan tidak terpenuhi
 * oleh cornerstone.
 *
 * @return 1 jika valid, 0 jika tidak.
 */
int rdr_validate_exception_rule(const ClauseList *query, const ClauseList *cornerstone,
                                const ClauseList *rule, char *err, size_t errlen);

/**
 * @brief Mengoreksi kesimpulan hasil inferensi terakhir.
 *
 * @param inf Hasil inferensi yang masih current.
 * @param conclusion Kesimpulan yang benar menurut pakar.
 * @param custom_rule Rule exception pilihan user, atau NULL untuk memakai
 *        seluruh difference list. Diabaikan pada kasus revisi.
 * @param strategy Strategi untuk kasus revisi.
 * @return Hasil koreksi.
 */
RdrAcqResult rdr_correct(RdrKb *kb, const RdrInference *inf, const char *conclusion,
                         const ClauseList *custom_rule, RdrRevisionStrategy strategy);

/* ===================================================================== */
/* Manual Editing                                                        */
/* ===================================================================== */

/**
 * @brief Mengganti rule node. Rule root harus tetap kosong.
 * @return 1 jika berhasil, 0 jika gagal.
 */
int rdr_node_set_rule(RdrKb *kb, int id, const ClauseList *rule, char *err, size_t errlen);

/**
 * @brief Mengganti kesimpulan node. Kesimpulan di-trim dan tidak boleh kosong.
 * @return 1 jika berhasil, 0 jika gagal.
 */
int rdr_node_set_conclusion(RdrKb *kb, int id, const char *conclusion, char *err, size_t errlen);

/**
 * @brief Mengganti cornerstone node. Cornerstone root harus tetap kosong dan
 *        fakta cornerstone harus konsisten.
 * @return 1 jika berhasil, 0 jika gagal.
 */
int rdr_node_set_cornerstone(RdrKb *kb, int id, const ClauseList *cornerstone,
                             char *err, size_t errlen);

/**
 * @brief Menambahkan anak pada slot kosong.
 *
 * Slot false milik root tidak dapat diisi karena root selalu bernilai benar
 * sehingga slot tersebut tidak pernah dievaluasi.
 *
 * @param new_id Tempat menyimpan id node baru (boleh NULL).
 * @return 1 jika berhasil, 0 jika gagal.
 */
int rdr_node_add_child(RdrKb *kb, int parent_id, RdrSide side, const ClauseList *rule,
                       const char *conclusion, const ClauseList *cornerstone,
                       int *new_id, char *err, size_t errlen);

/**
 * @brief Menghapus node beserta seluruh subtree-nya. Root tidak dapat dihapus.
 * @param removed Tempat menyimpan jumlah node yang dihapus (boleh NULL).
 * @return 1 jika berhasil, 0 jika gagal.
 */
int rdr_node_delete(RdrKb *kb, int id, size_t *removed, char *err, size_t errlen);

/* ===================================================================== */
/* Validation                                                            */
/* ===================================================================== */

/**
 * @brief Jenis temuan validasi KB.
 */
typedef enum {
    RDR_ISSUE_SUPERSEDED,        /**< Info: cornerstone kini disimpulkan oleh node revisinya. */
    RDR_ISSUE_NO_CORNERSTONE,    /**< Info: node non-root tanpa cornerstone. */
    RDR_ISSUE_MISMATCH,          /**< Error: cornerstone menghasilkan kesimpulan lain. */
    RDR_ISSUE_INCONSISTENT_CASE  /**< Error: cornerstone berisi fakta kontradiktif. */
} RdrIssueKind;

/**
 * @brief Satu temuan validasi.
 */
typedef struct {
    RdrIssueKind kind;  /**< Jenis temuan. */
    int node_id;        /**< Node pemilik cornerstone. */
    int reached_id;     /**< Node yang menyimpulkan cornerstone, -1 jika tidak relevan. */
    char *message;      /**< Penjelasan temuan (milik struct). */
} RdrIssue;

/**
 * @brief Laporan validasi KB.
 */
typedef struct {
    RdrIssue *items;  /**< Array temuan. */
    size_t count;     /**< Jumlah temuan. */
    size_t cap;       /**< Kapasitas array. */
    size_t checked;   /**< Jumlah node yang diperiksa. */
    size_t errors;    /**< Jumlah temuan berjenis error. */
} RdrValidation;

/**
 * @brief Memeriksa bahwa setiap cornerstone masih menghasilkan kesimpulan
 *        node pemiliknya.
 *
 * Cornerstone dianggap sah jika menghasilkan kesimpulan dengan teks yang sama.
 * Cornerstone yang kini disimpulkan oleh node revisi (lewat rantai revises)
 * dilaporkan sebagai info superseded.
 *
 * @param out Laporan. Wajib dibebaskan dengan rdr_validation_free.
 */
void rdr_validate(const RdrKb *kb, RdrValidation *out);

/** @brief Membebaskan isi laporan validasi. */
void rdr_validation_free(RdrValidation *v);

#endif /* RDR_RDR_H */
