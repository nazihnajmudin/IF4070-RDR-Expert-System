/**
 * @file query.h
 * @brief Parser teks kueri dan teks rule menjadi ClauseList.
 *
 * Sintaks kueri adalah klausa-klausa yang dipisah semicolon atau baris baru.
 * Whitespace di awal dan akhir setiap potongan dibuang, dan potongan kosong
 * diabaikan. Contoh "orang_kaya; demam <= 38.6 ; budget = besar" menghasilkan
 * tiga klausa: "orang_kaya", "demam <= 38.6", dan "budget = besar".
 *
 * Teks rule memakai sintaks yang sama. Bedanya, teks rule boleh berisi kata
 * kunci "T" saja (atau kosong) yang berarti rule selalu benar.
 */

#ifndef RDR_QUERY_H
#define RDR_QUERY_H

#include "rdr/clause.h"
#include "rdr/strutil.h"

/** Pemisah standar saat menggabungkan klausa menjadi teks kueri. */
#define QUERY_SEPARATOR "; "

/**
 * @brief Memecah teks kueri menjadi potongan teks klausa yang sudah di-trim.
 *
 * Fungsi tidak memvalidasi sintaks klausa. Potongan kosong diabaikan.
 *
 * @param text Teks kueri.
 * @param out Daftar tujuan. Potongan ditambahkan ke akhir daftar.
 */
void query_split(const char *text, StrList *out);

/**
 * @brief Mem-parse daftar potongan teks menjadi klausa.
 *
 * Klausa duplikat (sama makna) hanya disimpan sekali. Potongan yang tidak
 * valid tidak dimasukkan ke out, dan pesan error-nya ditambahkan ke errors
 * dalam format "Clause <nomor>: <pesan>".
 *
 * @param pieces Daftar teks klausa.
 * @param out Daftar klausa tujuan. Klausa ditambahkan ke akhir daftar.
 * @param errors Daftar pesan error (boleh NULL).
 * @return Jumlah potongan yang gagal di-parse.
 */
size_t query_parse_list(const StrList *pieces, ClauseList *out, StrList *errors);

/**
 * @brief Mem-parse teks kueri menjadi klausa.
 *
 * Gabungan query_split dan query_parse_list.
 *
 * @return Jumlah potongan yang gagal di-parse.
 */
size_t query_parse(const char *text, ClauseList *out, StrList *errors);

/**
 * @brief Mem-parse teks rule menjadi klausa.
 *
 * Teks kosong atau teks yang hanya berisi "T" menghasilkan daftar kosong
 * (rule selalu benar). Kata kunci "T" tidak boleh dicampur dengan klausa lain.
 *
 * @return Jumlah error yang ditemukan.
 */
size_t query_parse_rule(const char *text, ClauseList *out, StrList *errors);

/**
 * @brief Menggabungkan klausa menjadi teks kueri dengan pemisah "; ".
 * @return String baru milik pemanggil.
 */
char *query_format(const ClauseList *list);

#endif /* RDR_QUERY_H */
