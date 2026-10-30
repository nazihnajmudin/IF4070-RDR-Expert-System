/**
 * @file factbase.h
 * @brief ADT FactBase, indeks fakta sebuah kasus untuk evaluasi klausa rule.
 *
 * FactBase dibangun dari klausa-klausa kueri (fakta). Fakta dikelompokkan per
 * pasangan (jenis klausa, nama variabel). Klausa yang jenisnya berbeda tidak
 * pernah dibandingkan walaupun nama variabelnya sama.
 *
 * Evaluasi memakai Open World Assumption. Klausa rule bernilai benar hanya
 * jika fakta yang ada membuktikannya. Fakta yang tidak ada berarti unknown dan
 * diperlakukan sebagai salah.
 *
 *   Klausa rule        Bernilai benar jika
 *   v                  ada fakta v
 *   !v                 ada fakta !v
 *   v = a (literal)    ada fakta v = a
 *   v != a (literal)   ada fakta v = b dengan b berbeda dari a, atau ada fakta v != a
 *   v op n (numerik)   ada fakta numerik v dan irisannya subset dari {x | x op n}
 *
 * Fakta yang saling bertentangan membuat FactBase tidak konsisten:
 *   - v dan !v
 *   - v = a dan v = b dengan a berbeda dari b
 *   - v = a dan v != a
 *   - irisan fakta numerik suatu variabel kosong
 */

#ifndef RDR_FACTBASE_H
#define RDR_FACTBASE_H

#include "rdr/clause.h"
#include "rdr/strutil.h"

/** Tipe opaque untuk indeks fakta. */
typedef struct FactBase FactBase;

/**
 * @brief Membangun FactBase dari daftar fakta.
 *
 * FactBase tetap dibuat walaupun fakta tidak konsisten. Gunakan
 * factbase_is_consistent untuk memeriksanya.
 *
 * @param facts Daftar klausa fakta. Tidak disimpan setelah fungsi selesai.
 * @param errors Daftar tujuan pesan kontradiksi (boleh NULL).
 * @return FactBase baru yang wajib dibebaskan dengan factbase_destroy.
 */
FactBase *factbase_create(const ClauseList *facts, StrList *errors);

/** @brief Membebaskan FactBase. Aman dipanggil dengan NULL. */
void factbase_destroy(FactBase *fb);

/**
 * @brief Memeriksa apakah seluruh fakta konsisten.
 * @return 1 jika konsisten, 0 jika ada kontradiksi.
 */
int factbase_is_consistent(const FactBase *fb);

/**
 * @brief Mengevaluasi satu klausa rule terhadap fakta.
 * @return 1 jika klausa terbukti benar, 0 jika salah atau unknown.
 */
int factbase_satisfies(const FactBase *fb, const Clause *rule_clause);

/**
 * @brief Mengevaluasi konjungsi klausa rule terhadap fakta.
 *
 * Daftar kosong (rule T) selalu bernilai benar.
 *
 * @return 1 jika seluruh klausa terbukti benar, 0 jika tidak.
 */
int factbase_satisfies_all(const FactBase *fb, const ClauseList *rule);

/**
 * @brief Memeriksa apakah daftar fakta konsisten tanpa menyimpan FactBase.
 * @param errors Daftar tujuan pesan kontradiksi (boleh NULL).
 * @return 1 jika konsisten, 0 jika tidak.
 */
int factbase_check(const ClauseList *facts, StrList *errors);

/**
 * @brief Mengevaluasi rule terhadap daftar fakta secara langsung.
 * @return 1 jika rule terpenuhi, 0 jika tidak.
 */
int factbase_list_satisfies(const ClauseList *facts, const ClauseList *rule);

#endif /* RDR_FACTBASE_H */
