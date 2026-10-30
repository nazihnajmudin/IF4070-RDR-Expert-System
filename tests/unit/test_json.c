/**
 * @file test_json.c
 * @brief Unit test parser dan writer JSON.
 */

#include <stdlib.h>
#include <string.h>

#include "rdr/json.h"
#include "rdr/strutil.h"
#include "test.h"

static JsonValue *parse(const char *text)
{
    return json_parse(text, strlen(text), NULL, 0);
}

static void check_error(const char *text, const char *expected_fragment)
{
    char err[256] = "";
    JsonValue *v = json_parse(text, strlen(text), err, sizeof(err));
    CHECK(v == NULL);
    if (strstr(err, expected_fragment) == NULL) {
        fprintf(stderr, "  error for '%s' was '%s'\n", text, err);
    }
    CHECK(strstr(err, expected_fragment) != NULL);
    json_free(v);
}

TEST(scalars)
{
    JsonValue *v = parse("  42 ");
    int n = 0;
    CHECK(v != NULL && v->type == JSON_NUMBER);
    CHECK(json_get_int(v, &n));
    CHECK_INT_EQ(n, 42);
    json_free(v);

    v = parse("-1.5e2");
    CHECK_DBL_EQ(v->number, -150);
    CHECK(!json_get_int(v, NULL) || v->number == -150);
    json_free(v);

    v = parse("true");
    CHECK(v->type == JSON_BOOL && v->boolean == 1);
    json_free(v);
    v = parse("null");
    CHECK(v->type == JSON_NULL);
    json_free(v);
    v = parse("\"a\\\"b\\\\c\\n\\u00e9\\ud83d\\ude00\"");
    CHECK_STR_EQ(v->string, "a\"b\\c\n\xC3\xA9\xF0\x9F\x98\x80");
    json_free(v);
}

TEST(containers)
{
    JsonValue *v = parse("{\"a\": [1, 2, {\"b\": null}], \"c\": {}, \"d\": []}");
    JsonValue *a;

    CHECK(v != NULL && v->type == JSON_OBJECT);
    CHECK_INT_EQ(v->count, 3);
    a = json_object_get(v, "a");
    CHECK(a != NULL && a->type == JSON_ARRAY && a->count == 3);
    CHECK(json_object_get(a->items[2], "b")->type == JSON_NULL);
    CHECK(json_object_get(v, "c")->count == 0);
    CHECK(json_object_get(v, "missing") == NULL);
    json_free(v);
}

TEST(errors)
{
    check_error("", "unexpected end");
    check_error("{", "unexpected end");
    check_error("[1, 2", "expected ',' or ']'");
    check_error("[1, ]", "expected a value");
    check_error("{\"a\" 1}", "expected ':'");
    check_error("{a: 1}", "expected a string key");
    check_error("{\"a\": 1,}", "expected a string key");
    check_error("\"abc", "unterminated string");
    check_error("01", "unexpected content");
    check_error("1.", "digits after decimal");
    check_error("tru", "expected a value");
    check_error("[1] 2", "unexpected content");
    check_error("\"\\x\"", "invalid escape");
    check_error("{\n  \"a\": 1\n  \"b\": 2\n}", "line 3, column 3");
}

TEST(pretty_print)
{
    JsonValue *doc = json_new_object();
    JsonValue *arr = json_new_array();
    JsonValue *nested = json_new_object();
    char *text;

    json_array_push(arr, json_new_string("demam"));
    json_array_push(arr, json_new_string("suhu >= 38"));
    json_object_set(nested, "x", json_new_null());
    json_object_set(doc, "id", json_new_number(1));
    json_object_set(doc, "rule", arr);
    json_object_set(doc, "empty", json_new_array());
    json_object_set(doc, "child", nested);
    json_object_set(doc, "ratio", json_new_number(0.25));
    json_object_set(doc, "split", json_new_number(0.42));
    json_object_set(doc, "third", json_new_number(1.0 / 3.0));
    json_object_set(doc, "id", json_new_number(7));  /* ganti nilai tanpa mengubah urutan */

    text = json_write(doc);
    CHECK_STR_EQ(text,
        "{\n"
        "  \"id\": 7,\n"
        "  \"rule\": [\"demam\", \"suhu >= 38\"],\n"
        "  \"empty\": [],\n"
        "  \"child\": {\n"
        "    \"x\": null\n"
        "  },\n"
        "  \"ratio\": 0.25,\n"
        "  \"split\": 0.42,\n"
        "  \"third\": 0.33333333333333331\n"
        "}\n");
    free(text);
    json_free(doc);
}

TEST(round_trip_and_depth)
{
    JsonValue *root = json_new_object();
    JsonValue *cur = root;
    JsonValue *parsed;
    char *text;
    char *text2;
    int i;

    for (i = 0; i < 30000; i++) {  /* 30000 object bersarang menguji parser dan writer iteratif */
        JsonValue *child = json_new_object();
        json_object_set(cur, "s", json_new_string("tab\there \"q\""));
        json_object_set(cur, "next", child);
        cur = child;
    }
    text = json_write(root);
    parsed = json_parse(text, strlen(text), NULL, 0);
    CHECK(parsed != NULL);
    text2 = json_write(parsed);
    CHECK(strcmp(text, text2) == 0);
    free(text);
    free(text2);
    json_free(root);
    json_free(parsed);
}

int main(void)
{
    RUN(scalars);
    RUN(containers);
    RUN(errors);
    RUN(pretty_print);
    RUN(round_trip_and_depth);
    return TEST_REPORT();
}
