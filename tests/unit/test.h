/**
 * @file test.h
 * @brief Framework unit test minimal berbasis makro.
 *
 * Setiap file test mendefinisikan fungsi test dengan TEST(nama), lalu
 * memanggilnya dari main() lewat RUN(nama). Hasil akhir dilaporkan oleh
 * TEST_REPORT() yang sekaligus menjadi nilai kembali main().
 *
 * Contoh:
 *   TEST(trim_basic) { CHECK_STR_EQ(trimmed, "abc"); }
 *   int main(void) { RUN(trim_basic); return TEST_REPORT(); }
 */

#ifndef RDR_TEST_H
#define RDR_TEST_H

#include <math.h>
#include <stdio.h>
#include <string.h>

/** Jumlah assertion yang gagal pada seluruh test. */
static int test_failures = 0;
/** Jumlah assertion yang dijalankan. */
static int test_checks = 0;
/** Jumlah fungsi test yang dijalankan. */
static int test_count = 0;

/** Mendefinisikan fungsi test dengan nama tertentu. */
#define TEST(name) static void test_##name(void)

/** Menjalankan fungsi test yang didefinisikan dengan TEST(name). */
#define RUN(name)                \
    do {                         \
        test_count++;            \
        test_##name();           \
    } while (0)

/** Memeriksa bahwa ekspresi bernilai benar. */
#define CHECK(expr)                                                            \
    do {                                                                       \
        test_checks++;                                                         \
        if (!(expr)) {                                                         \
            test_failures++;                                                   \
            fprintf(stderr, "  FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr);  \
        }                                                                      \
    } while (0)

/** Memeriksa bahwa dua nilai integer sama. */
#define CHECK_INT_EQ(actual, expected)                                         \
    do {                                                                       \
        long long a_ = (long long)(actual);                                    \
        long long e_ = (long long)(expected);                                  \
        test_checks++;                                                         \
        if (a_ != e_) {                                                        \
            test_failures++;                                                   \
            fprintf(stderr, "  FAIL %s:%d: %s == %lld, expected %lld\n",       \
                    __FILE__, __LINE__, #actual, a_, e_);                      \
        }                                                                      \
    } while (0)

/** Memeriksa bahwa dua bilangan real sama dengan toleransi kecil. */
#define CHECK_DBL_EQ(actual, expected)                                         \
    do {                                                                       \
        double a_ = (double)(actual);                                          \
        double e_ = (double)(expected);                                        \
        test_checks++;                                                         \
        if (fabs(a_ - e_) > 1e-9) {                                            \
            test_failures++;                                                   \
            fprintf(stderr, "  FAIL %s:%d: %s == %g, expected %g\n",           \
                    __FILE__, __LINE__, #actual, a_, e_);                      \
        }                                                                      \
    } while (0)

/** Memeriksa bahwa dua string sama (NULL dianggap berbeda dari string apa pun). */
#define CHECK_STR_EQ(actual, expected)                                         \
    do {                                                                       \
        const char *a_ = (actual);                                             \
        const char *e_ = (expected);                                           \
        test_checks++;                                                         \
        if (a_ == NULL || e_ == NULL || strcmp(a_, e_) != 0) {                 \
            test_failures++;                                                   \
            fprintf(stderr, "  FAIL %s:%d: %s == \"%s\", expected \"%s\"\n",   \
                    __FILE__, __LINE__, #actual, a_ ? a_ : "(null)",           \
                    e_ ? e_ : "(null)");                                       \
        }                                                                      \
    } while (0)

/** Mencetak ringkasan hasil test dan mengembalikan exit code. */
#define TEST_REPORT()                                                          \
    (fprintf(stderr, "%s: %d tests, %d checks, %d failures\n", __FILE__,       \
             test_count, test_checks, test_failures),                          \
     test_failures == 0 ? 0 : 1)

#endif /* RDR_TEST_H */
