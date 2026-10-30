/**
 * @file test_clause.c
 * @brief Unit test modul clause dan query.
 */

#include <stdlib.h>
#include <string.h>

#include "rdr/clause.h"
#include "rdr/query.h"
#include "test.h"

/* Mem-parse lalu mengembalikan bentuk kanonik, atau NULL jika gagal. */
static char *canon(const char *text)
{
    Clause c;
    char *s;

    if (!clause_parse(text, &c, NULL, 0)) {
        return NULL;
    }
    s = clause_to_string(&c);
    clause_free(&c);
    return s;
}

static void check_canon(const char *text, const char *expected)
{
    char *s = canon(text);
    CHECK_STR_EQ(s, expected);
    free(s);
}

static void check_invalid(const char *text)
{
    char *s = canon(text);
    if (s != NULL) {
        fprintf(stderr, "  expected invalid: '%s' parsed as '%s'\n", text, s);
    }
    CHECK(s == NULL);
    free(s);
}

TEST(parse_boolean)
{
    Clause c;

    CHECK(clause_parse("  demam ", &c, NULL, 0));
    CHECK_INT_EQ(c.type, CLAUSE_BOOL);
    CHECK_INT_EQ(c.op, OP_EQ);
    CHECK_STR_EQ(c.var, "demam");
    CHECK(c.value == NULL);
    clause_free(&c);

    CHECK(clause_parse("!demam", &c, NULL, 0));
    CHECK_INT_EQ(c.op, OP_NE);
    clause_free(&c);

    check_canon("! demam", "!demam");
    check_canon("_x1", "_x1");
}

TEST(parse_literal)
{
    Clause c;

    CHECK(clause_parse("demam = tinggi", &c, NULL, 0));
    CHECK_INT_EQ(c.type, CLAUSE_LITERAL);
    CHECK_INT_EQ(c.op, OP_EQ);
    CHECK_STR_EQ(c.value, "tinggi");
    clause_free(&c);

    check_canon("demam=tinggi", "demam = tinggi");
    check_canon("warna   !=  merah", "warna != merah");
    check_canon("kota = jakarta_selatan", "kota = jakarta_selatan");
}

TEST(parse_numeric)
{
    Clause c;

    CHECK(clause_parse("derajat_demam >= 38.5", &c, NULL, 0));
    CHECK_INT_EQ(c.type, CLAUSE_NUMERIC);
    CHECK_INT_EQ(c.op, OP_GE);
    CHECK_DBL_EQ(c.number, 38.5);
    clause_free(&c);

    check_canon("x>3", "x > 3");
    check_canon("x<=-2.5", "x <= -2.5");
    check_canon("x = 38.50", "x = 38.50");
    check_canon("x != 0", "x != 0");
    check_canon("x<3", "x < 3");
}

TEST(parse_invalid)
{
    char err[256];
    Clause c;

    check_invalid("");
    check_invalid("   ");
    check_invalid("T");
    check_invalid("T = 3");
    check_invalid("1abc");
    check_invalid("suhu badan = 3");
    check_invalid("x >");
    check_invalid("x > tinggi");
    check_invalid("x <= abc");
    check_invalid("!x = 3");
    check_invalid("x = a b");
    check_invalid("x == 3");
    check_invalid("x = !a");
    check_invalid("x-y = 3");
    check_invalid("!");

    CHECK(!clause_parse("x > tinggi", &c, err, sizeof(err)));
    CHECK(strstr(err, "numeric") != NULL);
}

TEST(equality)
{
    Clause a;
    Clause b;

    clause_parse("x = 5", &a, NULL, 0);
    clause_parse("x=5.0", &b, NULL, 0);
    CHECK(clause_equals(&a, &b));
    clause_free(&b);
    clause_parse("x = 6", &b, NULL, 0);
    CHECK(!clause_equals(&a, &b));
    clause_free(&b);
    clause_parse("x = lima", &b, NULL, 0);
    CHECK(!clause_equals(&a, &b));
    clause_free(&b);
    clause_free(&a);
}

TEST(clause_list)
{
    ClauseList list;
    ClauseList copy;
    Clause c;
    char *s;

    clauselist_init(&list);
    clauselist_init(&copy);
    s = clauselist_rule_text(&list);
    CHECK_STR_EQ(s, "T");
    free(s);

    clause_parse("a", &c, NULL, 0);
    clauselist_push_move(&list, &c);
    CHECK(c.var == NULL);
    clause_parse("b = 2", &c, NULL, 0);
    clauselist_push_copy(&list, &c);
    clause_free(&c);

    s = clauselist_rule_text(&list);
    CHECK_STR_EQ(s, "a AND b = 2");
    free(s);

    clauselist_copy(&copy, &list);
    CHECK(clauselist_equals(&copy, &list));
    clauselist_remove_at(&copy, 0);
    CHECK(!clauselist_equals(&copy, &list));
    s = clauselist_join(&copy, "; ");
    CHECK_STR_EQ(s, "b = 2");
    free(s);

    clauselist_free(&list);
    clauselist_free(&copy);
}

TEST(query_split_and_parse)
{
    StrList pieces;
    StrList errors;
    ClauseList list;
    char *s;

    strlist_init(&pieces);
    query_split("orang_kaya; demam <= 38.6 ; budget = besar", &pieces);
    CHECK_INT_EQ(pieces.count, 3);
    CHECK_STR_EQ(pieces.items[0], "orang_kaya");
    CHECK_STR_EQ(pieces.items[1], "demam <= 38.6");
    CHECK_STR_EQ(pieces.items[2], "budget = besar");
    strlist_clear(&pieces);
    query_split("  a \n\n b ;; \r\n c  ", &pieces);
    CHECK_INT_EQ(pieces.count, 3);
    strlist_free(&pieces);

    clauselist_init(&list);
    strlist_init(&errors);
    CHECK_INT_EQ(query_parse("a; x=1; a ; x = 1.0; bad >; y = q", &list, &errors), 1);
    CHECK_INT_EQ(list.count, 3);
    CHECK_INT_EQ(errors.count, 1);
    CHECK(strncmp(errors.items[0], "Clause 5:", 9) == 0);
    s = query_format(&list);
    CHECK_STR_EQ(s, "a; x = 1; y = q");
    free(s);
    clauselist_free(&list);
    strlist_free(&errors);
}

TEST(rule_parse)
{
    ClauseList list;
    StrList errors;

    clauselist_init(&list);
    strlist_init(&errors);
    CHECK_INT_EQ(query_parse_rule(" T ", &list, &errors), 0);
    CHECK_INT_EQ(list.count, 0);
    CHECK_INT_EQ(query_parse_rule("", &list, &errors), 0);
    CHECK_INT_EQ(list.count, 0);
    CHECK_INT_EQ(query_parse_rule("T; a", &list, &errors), 1);
    CHECK_INT_EQ(list.count, 0);
    CHECK_INT_EQ(query_parse_rule("a; b > 2", &list, &errors), 0);
    CHECK_INT_EQ(list.count, 2);
    clauselist_free(&list);
    strlist_free(&errors);
}

int main(void)
{
    RUN(parse_boolean);
    RUN(parse_literal);
    RUN(parse_numeric);
    RUN(parse_invalid);
    RUN(equality);
    RUN(clause_list);
    RUN(query_split_and_parse);
    RUN(rule_parse);
    return TEST_REPORT();
}
