/**
 * @file csv.h
 * @brief Pembaca CSV sesuai RFC 4180.
 *
 * Field dipisah koma dan record dipisah baris baru (LF atau CRLF). Field
 * boleh diapit tanda kutip ganda agar dapat memuat koma, baris baru, atau
 * tanda kutip (ditulis ganda ""). BOM UTF-8 di awal teks diabaikan, dan baris
 * yang benar-benar kosong dilewati.
 */

#ifndef RDR_CSV_H
#define RDR_CSV_H

#include <stddef.h>

#include "rdr/strutil.h"

/**
 * @brief Hasil parsing CSV.
 */
typedef struct {
    StrList *rows;   /**< Array record, setiap record berupa daftar field. */
    size_t *lines;   /**< Nomor baris awal setiap record (1-based). */
    size_t count;    /**< Jumlah record. */
    size_t cap;      /**< Kapasitas array. */
} CsvTable;

/** @brief Menginisialisasi tabel kosong. */
void csv_table_init(CsvTable *t);

/** @brief Membebaskan seluruh isi tabel. */
void csv_table_free(CsvTable *t);

/**
 * @brief Mem-parse teks CSV.
 * @param out Tabel tujuan. Isi sebelumnya dibuang.
 * @return 1 jika berhasil, 0 jika format tidak valid.
 */
int csv_parse(const char *text, size_t len, CsvTable *out, char *err, size_t errlen);

#endif /* RDR_CSV_H */
