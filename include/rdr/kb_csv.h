/**
 * @file kb_csv.h
 * @brief Akuisisi knowledge secara batch dari dataset CSV.
 *
 * Format dataset:
 *   case,conclusion
 *   "demam; suhu = 39; batuk",Flu
 *   "batuk; !demam",Alergi
 *
 * Header wajib berisi kolom case dan conclusion (tanpa membedakan huruf).
 * Kolom case memakai sintaks kueri biasa. Setiap baris diproses berurutan:
 * inferensi dijalankan, lalu jika hasilnya berbeda dari kolom conclusion,
 * koreksi dilakukan dengan seluruh difference list (mode strict). Kasus revisi
 * memakai strategi revisi yang diberikan.
 */

#ifndef RDR_KB_CSV_H
#define RDR_KB_CSV_H

#include <stddef.h>

#include "rdr/rdr.h"
#include "rdr/strutil.h"

/**
 * @brief Laporan hasil impor CSV.
 */
typedef struct {
    size_t rows;                /**< Jumlah baris data (tanpa header). */
    size_t already_correct;     /**< Baris yang sudah menghasilkan kesimpulan benar. */
    size_t exceptions_added;    /**< Jumlah node exception yang ditambahkan. */
    size_t revisions_added;     /**< Jumlah node revisi yang ditambahkan. */
    size_t conclusions_edited;  /**< Jumlah kesimpulan yang diubah langsung. */
    size_t rejected;            /**< Baris yang ditolak strategi reject. */
    size_t failed;              /**< Baris yang gagal diproses. */
    StrList messages;           /**< Catatan per baris, diawali "line N:". */
} CsvImportReport;

/** @brief Menginisialisasi laporan kosong. */
void csv_report_init(CsvImportReport *r);

/** @brief Membebaskan isi laporan. */
void csv_report_free(CsvImportReport *r);

/**
 * @brief Membuat ringkasan satu baris dari laporan.
 * @return String baru milik pemanggil.
 */
char *csv_report_summary(const CsvImportReport *r);

/**
 * @brief Mengimpor dataset CSV dari teks.
 * @param strategy Strategi untuk kasus revisi.
 * @param report Laporan tujuan. Isi sebelumnya dibuang.
 * @return 1 jika dataset dapat dibaca (walaupun sebagian baris gagal), 0 jika format tidak valid.
 */
int kb_csv_import_text(RdrKb *kb, const char *text, size_t len, RdrRevisionStrategy strategy,
                       CsvImportReport *report, char *err, size_t errlen);

/**
 * @brief Mengimpor dataset CSV dari file.
 * @return 1 jika dataset dapat dibaca, 0 jika file atau format tidak valid.
 */
int kb_csv_import_file(RdrKb *kb, const char *path, RdrRevisionStrategy strategy,
                       CsvImportReport *report, char *err, size_t errlen);

#endif /* RDR_KB_CSV_H */
