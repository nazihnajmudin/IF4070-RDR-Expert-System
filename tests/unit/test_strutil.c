/**
 * @file test_strutil.c
 * @brief Unit test modul strutil.
 */

#include <stdlib.h>

#include "rdr/strutil.h"
#include "test.h"

TEST(trim)
{
    char *s = str_trim_dup("  \t abc def \n ");
    CHECK_STR_EQ(s, "abc def");
    free(s);
    s = str_trim_dup("   ");
    CHECK_STR_EQ(s, "");
    free(s);
    CHECK(str_is_blank(" \t\n"));
    CHECK(str_is_blank(NULL));
    CHECK(!str_is_blank(" x "));
}

TEST(parse_number)
{
    double v = 0;
    CHECK(str_parse_number("38.5", &v));
    CHECK_DBL_EQ(v, 38.5);
    CHECK(str_parse_number("-3", &v));
    CHECK_DBL_EQ(v, -3);
    CHECK(str_parse_number("+0.25", &v));
    CHECK_DBL_EQ(v, 0.25);
    CHECK(str_parse_number("007", &v));
    CHECK_DBL_EQ(v, 7);
    CHECK(!str_parse_number("1e3", &v));
    CHECK(!str_parse_number("inf", &v));
    CHECK(!str_parse_number("0x10", &v));
    CHECK(!str_parse_number("12abc", &v));
    CHECK(!str_parse_number("", &v));
    CHECK(!str_parse_number("-", &v));
    CHECK(!str_parse_number(".", &v));
}

TEST(strbuf)
{
    StrBuf sb;
    char *s;
    int i;

    strbuf_init(&sb);
    CHECK_STR_EQ(strbuf_cstr(&sb), "");
    strbuf_append(&sb, "ab");
    strbuf_append_char(&sb, 'c');
    strbuf_appendf(&sb, "-%d-%s", 42, "x");
    CHECK_STR_EQ(strbuf_cstr(&sb), "abc-42-x");
    for (i = 0; i < 100; i++) {
        strbuf_append(&sb, "0123456789");
    }
    CHECK_INT_EQ(sb.len, 8 + 1000);
    s = strbuf_steal(&sb);
    CHECK_INT_EQ(sb.len, 0);
    CHECK(sb.data == NULL);
    free(s);
    strbuf_free(&sb);
}

TEST(strlist)
{
    StrList a;
    StrList b;
    char *joined;

    strlist_init(&a);
    strlist_init(&b);
    strlist_push(&a, "x");
    strlist_push(&a, "y");
    strlist_pushf(&a, "z%d", 1);
    CHECK_INT_EQ(a.count, 3);
    CHECK_INT_EQ(strlist_index_of(&a, "y"), 1);
    CHECK_INT_EQ(strlist_index_of(&a, "q"), -1);
    strlist_set(&a, 0, "w");
    strlist_remove_at(&a, 1);
    joined = strlist_join(&a, ", ");
    CHECK_STR_EQ(joined, "w, z1");
    free(joined);
    strlist_copy(&b, &a);
    CHECK_INT_EQ(b.count, 2);
    CHECK_STR_EQ(b.items[1], "z1");
    strlist_free(&a);
    strlist_free(&b);
}

TEST(intlist)
{
    IntList l;
    int i;

    intlist_init(&l);
    for (i = 0; i < 20; i++) {
        intlist_push(&l, i * 2);
    }
    CHECK_INT_EQ(l.count, 20);
    CHECK_INT_EQ(intlist_index_of(&l, 10), 5);
    CHECK_INT_EQ(intlist_index_of(&l, 11), -1);
    intlist_free(&l);
}

TEST(errors_and_copy)
{
    char buf[8];
    str_copy(buf, sizeof(buf), "abcdefghijk");
    CHECK_STR_EQ(buf, "abcdefg");
    str_set_error(buf, sizeof(buf), "%d", 12);
    CHECK_STR_EQ(buf, "12");
    str_set_error(NULL, 0, "%d", 12);
    CHECK(str_iequals("Case", "cASE"));
    CHECK(!str_iequals("Case", "Cases"));
}

int main(void)
{
    RUN(trim);
    RUN(parse_number);
    RUN(strbuf);
    RUN(strlist);
    RUN(intlist);
    RUN(errors_and_copy);
    return TEST_REPORT();
}
