/**
 * @file test_factbase.c
 * @brief Unit test modul numset dan factbase.
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "rdr/factbase.h"
#include "rdr/numset.h"
#include "rdr/query.h"
#include "test.h"

/* ===================================================================== */
/* Test Helpers                                                          */
/* ===================================================================== */

static char *set_text(const NumSet *s)
{
    StrBuf sb;
    strbuf_init(&sb);
    numset_format(s, &sb);
    return strbuf_steal(&sb);
}

static void check_set(const NumSet *s, const char *expected)
{
    char *t = set_text(s);
    CHECK_STR_EQ(t, expected);
    free(t);
}

/* Mengevaluasi rule (teks) terhadap fakta (teks kueri). */
static int holds(const char *facts_text, const char *rule_text)
{
    ClauseList facts;
    ClauseList rule;
    int result;

    clauselist_init(&facts);
    clauselist_init(&rule);
    query_parse(facts_text, &facts, NULL);
    query_parse_rule(rule_text, &rule, NULL);
    result = factbase_list_satisfies(&facts, &rule);
    clauselist_free(&facts);
    clauselist_free(&rule);
    return result;
}

static int consistent(const char *facts_text, size_t *error_count)
{
    ClauseList facts;
    StrList errors;
    int ok;

    clauselist_init(&facts);
    strlist_init(&errors);
    query_parse(facts_text, &facts, NULL);
    ok = factbase_check(&facts, &errors);
    if (error_count != NULL) {
        *error_count = errors.count;
    }
    clauselist_free(&facts);
    strlist_free(&errors);
    return ok;
}

/* ===================================================================== */
/* NumSet Tests                                                          */
/* ===================================================================== */

TEST(numset_from_op)
{
    NumSet s;

    numset_init_from_op(&s, OP_EQ, 5);
    check_set(&s, "{5}");
    numset_free(&s);
    numset_init_from_op(&s, OP_NE, 5);
    check_set(&s, "(-inf, 5) U (5, +inf)");
    numset_free(&s);
    numset_init_from_op(&s, OP_GE, 2);
    check_set(&s, "[2, +inf)");
    numset_free(&s);
    numset_init_from_op(&s, OP_LT, 2);
    check_set(&s, "(-inf, 2)");
    numset_free(&s);
}

TEST(numset_intersect)
{
    NumSet s;

    numset_init_all(&s);
    numset_intersect_op(&s, OP_GT, 37);
    numset_intersect_op(&s, OP_LT, 39);
    check_set(&s, "(37, 39)");
    numset_intersect_op(&s, OP_NE, 38);
    check_set(&s, "(37, 38) U (38, 39)");
    numset_intersect_op(&s, OP_GE, 38);
    check_set(&s, "(38, 39)");
    numset_intersect_op(&s, OP_LE, 38);
    CHECK(numset_is_empty(&s));
    numset_free(&s);

    numset_init_all(&s);
    numset_intersect_op(&s, OP_GE, 3);
    numset_intersect_op(&s, OP_LE, 3);
    check_set(&s, "{3}");
    numset_free(&s);
}

TEST(numset_subset)
{
    NumSet a;
    NumSet b;

    numset_init_from_op(&a, OP_EQ, 39);
    numset_init_from_op(&b, OP_GE, 38.5);
    CHECK(numset_is_subset(&a, &b));
    numset_free(&a);

    numset_init_from_op(&a, OP_LE, 38.6);
    CHECK(!numset_is_subset(&a, &b));
    numset_free(&a);

    numset_init_from_op(&a, OP_GT, 38.5);
    CHECK(numset_is_subset(&a, &b));
    numset_free(&b);
    numset_init_from_op(&b, OP_GT, 38.5);
    CHECK(numset_is_subset(&a, &b));
    numset_free(&b);
    numset_init_from_op(&b, OP_GT, 39);
    CHECK(!numset_is_subset(&a, &b));
    numset_free(&b);
    numset_free(&a);

    numset_init_from_op(&a, OP_GE, 38.5);
    numset_init_from_op(&b, OP_GT, 38.5);
    CHECK(!numset_is_subset(&a, &b));
    numset_free(&a);
    numset_free(&b);

    numset_init_from_op(&a, OP_GT, 7);  /* subset dari gabungan interval hasil != */
    numset_init_from_op(&b, OP_NE, 5);
    CHECK(numset_is_subset(&a, &b));
    numset_free(&a);
    numset_init_from_op(&a, OP_GT, 3);
    CHECK(!numset_is_subset(&a, &b));
    numset_free(&a);
    numset_free(&b);
}

/* ===================================================================== */
/* FactBase Evaluation Tests                                             */
/* ===================================================================== */

TEST(eval_boolean)
{
    CHECK(holds("demam", "demam"));
    CHECK(!holds("batuk", "demam"));
    CHECK(!holds("demam", "!demam"));
    CHECK(!holds("", "!demam"));          /* OWA: tidak ada fakta berarti unknown */
    CHECK(holds("!demam", "!demam"));
    CHECK(!holds("!demam", "demam"));
    CHECK(holds("anything", "T"));
    CHECK(holds("", "T"));
}

TEST(eval_literal)
{
    CHECK(holds("warna = merah", "warna = merah"));
    CHECK(!holds("warna = merah", "warna = biru"));
    CHECK(holds("warna = merah", "warna != biru"));
    CHECK(!holds("warna = merah", "warna != merah"));
    CHECK(holds("warna != biru", "warna != biru"));
    CHECK(!holds("warna != biru", "warna = merah"));
    CHECK(!holds("warna != biru", "warna != hijau"));
    CHECK(!holds("", "warna != biru"));
    CHECK(!holds("warna = Merah", "warna = merah"));  /* case-sensitive */
}

TEST(eval_numeric)
{
    CHECK(holds("derajat_demam = 39", "derajat_demam >= 38.5"));
    CHECK(!holds("derajat_demam <= 38.6", "derajat_demam >= 38"));
    CHECK(holds("suhu > 37; suhu < 39", "suhu > 36"));
    CHECK(holds("suhu > 37; suhu < 39", "suhu < 40"));
    CHECK(!holds("suhu > 37; suhu < 39", "suhu > 38"));
    CHECK(holds("x = 5", "x != 4"));
    CHECK(!holds("x = 5", "x != 5"));
    CHECK(holds("x = 5.0", "x = 5"));
    CHECK(!holds("", "x > 0"));
    CHECK(holds("x >= 10", "x > 9.99"));
}

TEST(eval_type_isolation)
{
    CHECK(!holds("x = 5", "x = lima"));
    CHECK(!holds("x = lima", "x = 5"));
    CHECK(!holds("x", "x = 5"));
    CHECK(!holds("x = 5", "x"));
    CHECK(!holds("x = a", "x"));
}

TEST(eval_conjunction)
{
    CHECK(holds("a; b = 2; c = q", "a; b > 1"));
    CHECK(!holds("a; b = 2", "a; b > 1; c = q"));
}

TEST(consistency)
{
    size_t n = 0;

    CHECK(consistent("a; b = 1; c = x", &n));
    CHECK_INT_EQ(n, 0);
    CHECK(!consistent("demam; !demam", &n));
    CHECK_INT_EQ(n, 1);
    CHECK(!consistent("w = a; w = b", &n));
    CHECK(!consistent("w = a; w != a", &n));
    CHECK(!consistent("w != a; w = a", &n));
    CHECK(consistent("w != a; w = b", &n));
    CHECK(consistent("w != a; w != b", &n));
    CHECK(!consistent("x > 5; x < 3", &n));
    CHECK(consistent("x > 5; x < 7", &n));
    CHECK(consistent("x = 5; x = lima", &n));  /* jenis berbeda, tidak dibandingkan */
    CHECK(!consistent("x = 1; x = 2; x = 3", &n));
    CHECK_INT_EQ(n, 1);
}

int main(void)
{
    RUN(numset_from_op);
    RUN(numset_intersect);
    RUN(numset_subset);
    RUN(eval_boolean);
    RUN(eval_literal);
    RUN(eval_numeric);
    RUN(eval_type_isolation);
    RUN(eval_conjunction);
    RUN(consistency);
    return TEST_REPORT();
}
