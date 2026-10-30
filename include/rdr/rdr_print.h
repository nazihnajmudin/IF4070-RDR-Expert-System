/**
 * @file rdr_print.h
 * @brief Render pohon RDR menjadi baris-baris teks Unicode.
 *
 * Setiap edge selalu diberi label T (true child) atau F (false child), dan
 * true child selalu dicetak di atas false child. Contoh:
 *
 *   #0 (T >> Miskin)
 *   ├─T─ #1 (uang > 100 >> Butuh Duit)
 *   │    ├─T─ #2 (budi = kaya >> Banyak Duit)
 *   │    │    ├─T─ ∅
 *   │    │    └─F─ ∅
 *   │    └─F─ ∅
 *   └─F─ ∅
 *
 * Hasil berupa daftar baris terstruktur sehingga GUI dapat mewarnai setiap
 * baris berdasarkan status node pada hasil inferensi.
 */

#ifndef RDR_RDR_PRINT_H
#define RDR_RDR_PRINT_H

#include <stddef.h>

#include "rdr/rdr.h"

/** Simbol untuk slot kosong. */
#define RDR_EMPTY_SLOT_SYMBOL "\xE2\x88\x85"

/**
 * @brief Jenis baris hasil render.
 */
typedef enum {
    RDR_LINE_NODE,        /**< Baris node. */
    RDR_LINE_EMPTY,       /**< Baris slot kosong. */
    RDR_LINE_CORNERSTONE  /**< Baris cornerstone milik node di atasnya. */
} RdrLineKind;

/**
 * @brief Satu baris hasil render.
 */
typedef struct {
    RdrLineKind kind;  /**< Jenis baris. */
    char *prefix;      /**< Garis cabang beserta label edge (milik struct). */
    char *text;        /**< Isi baris (milik struct). */
    int node_id;       /**< Node terkait, -1 untuk slot kosong. */
    int parent_id;     /**< Induk node atau slot, -1 untuk root. */
    int edge;          /**< -1 untuk root, 0 untuk edge true, 1 untuk edge false. */
} RdrLine;

/**
 * @brief Daftar baris hasil render.
 */
typedef struct {
    RdrLine *items;  /**< Array baris. */
    size_t count;    /**< Jumlah baris. */
    size_t cap;      /**< Kapasitas array. */
} RdrLines;

/**
 * @brief Opsi tampilan tree.
 */
typedef struct {
    int show_empty_slots;  /**< 1 untuk menampilkan slot kosong sebagai simbol kosong. */
    int show_node_id;      /**< 1 untuk menampilkan id node (#n). */
    int show_cornerstone;  /**< 1 untuk menampilkan cornerstone di bawah setiap node. */
} RdrPrintOptions;

/** @brief Mengisi opsi dengan nilai default (slot kosong dan id tampil). */
void rdr_print_default_options(RdrPrintOptions *opt);

/**
 * @brief Membuat label node, misalnya "#2 (budi = kaya >> Banyak Duit)".
 *
 * Node revisi diberi akhiran " [revises #n]".
 *
 * @return String baru milik pemanggil.
 */
char *rdr_node_label(const RdrNode *node, int show_id);

/**
 * @brief Merender seluruh tree secara iteratif.
 * @param out Daftar tujuan. Isi sebelumnya dibuang.
 */
void rdr_print_tree(const RdrKb *kb, const RdrPrintOptions *opt, RdrLines *out);

/** @brief Menginisialisasi daftar baris kosong. */
void rdr_lines_init(RdrLines *lines);

/** @brief Membebaskan seluruh baris. */
void rdr_lines_free(RdrLines *lines);

/**
 * @brief Menggabungkan seluruh baris (prefix dan teks) dengan baris baru.
 * @return String baru milik pemanggil.
 */
char *rdr_lines_join(const RdrLines *lines);

#endif /* RDR_RDR_PRINT_H */
