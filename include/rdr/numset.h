/**
 * @file numset.h
 * @brief ADT NumSet, himpunan bilangan real berupa gabungan interval.
 *
 * NumSet dipakai untuk evaluasi klausa numerik. Setiap klausa numerik
 * "x op n" dipetakan ke himpunan {x | x op n}. Fakta numerik pada kueri
 * diiriskan per variabel, lalu klausa rule bernilai benar jika himpunan fakta
 * merupakan subset dari himpunan klausa rule.
 *
 * Invarian bentuk normal: interval terurut naik, tidak kosong, saling lepas,
 * dan interval yang bersentuhan sudah digabung. Batas tak hingga dinyatakan
 * dengan -INFINITY atau INFINITY dan selalu bersifat terbuka.
 */

#ifndef RDR_NUMSET_H
#define RDR_NUMSET_H

#include <stddef.h>

#include "rdr/clause.h"
#include "rdr/strutil.h"

/**
 * @brief Interval bilangan real dengan batas terbuka atau tertutup.
 */
typedef struct {
    double lo;      /**< Batas bawah, boleh -INFINITY. */
    double hi;      /**< Batas atas, boleh INFINITY. */
    int lo_closed;  /**< 1 jika batas bawah termasuk (kurung siku). */
    int hi_closed;  /**< 1 jika batas atas termasuk (kurung siku). */
} Interval;

/**
 * @brief Himpunan bilangan berupa gabungan interval dalam bentuk normal.
 */
typedef struct {
    Interval *items;  /**< Array interval terurut. */
    size_t count;     /**< Jumlah interval. Nilai 0 berarti himpunan kosong. */
    size_t cap;       /**< Kapasitas array. */
} NumSet;

/** @brief Menginisialisasi himpunan kosong. */
void numset_init_empty(NumSet *set);

/** @brief Menginisialisasi himpunan seluruh bilangan real. */
void numset_init_all(NumSet *set);

/**
 * @brief Menginisialisasi himpunan {x | x op value}.
 * @param op Operator perbandingan.
 * @param value Bilangan pembanding.
 */
void numset_init_from_op(NumSet *set, ClauseOp op, double value);

/** @brief Membebaskan memori himpunan. */
void numset_free(NumSet *set);

/** @brief Menyalin src ke dst. Isi dst sebelumnya dibuang. */
void numset_copy(NumSet *dst, const NumSet *src);

/**
 * @brief Menghitung irisan dua himpunan.
 * @param out Himpunan hasil. Isi sebelumnya dibuang. Boleh sama dengan a atau b.
 */
void numset_intersect(const NumSet *a, const NumSet *b, NumSet *out);

/** @brief Mengiriskan set dengan {x | x op value} secara langsung. */
void numset_intersect_op(NumSet *set, ClauseOp op, double value);

/** @brief Memeriksa apakah himpunan kosong. */
int numset_is_empty(const NumSet *set);

/**
 * @brief Memeriksa apakah a merupakan subset dari b.
 *
 * Himpunan kosong adalah subset dari himpunan apa pun.
 *
 * @return 1 jika a subset b, 0 jika tidak.
 */
int numset_is_subset(const NumSet *a, const NumSet *b);

/**
 * @brief Menuliskan himpunan dalam notasi interval, misalnya "(-inf, 3) U (3, +inf)".
 */
void numset_format(const NumSet *set, StrBuf *sb);

#endif /* RDR_NUMSET_H */
