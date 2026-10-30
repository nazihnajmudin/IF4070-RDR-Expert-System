/**
 * @file test_io.c
 * @brief Unit test modul prefs, kb_io, csv, kb_csv, dan platform.
 */

#include <stdlib.h>
#include <string.h>

#include "rdr/csv.h"
#include "rdr/kb_csv.h"
#include "rdr/kb_io.h"
#include "rdr/platform.h"
#include "rdr/prefs.h"
#include "rdr/query.h"
#include "test.h"

#define TMP_DIR "build/test"

/* ===================================================================== */
/* Test Helpers                                                          */
/* ===================================================================== */

static const char *infer_text(const RdrKb *kb, const char *query, RdrInference *inf)
{
    ClauseList q;
    int ok;

    clauselist_init(&q);
    query_parse(query, &q, NULL);
    ok = rdr_infer(kb, &q, inf, NULL);
    clauselist_free(&q);
    return ok ? rdr_inference_conclusion(kb, inf) : NULL;
}

static void teach(RdrKb *kb, const char *query, const char *conclusion)
{
    RdrInference inf;

    rdr_inference_init(&inf);
    infer_text(kb, query, &inf);
    rdr_correct(kb, &inf, conclusion, NULL, RDR_REVISION_NODE);
    rdr_inference_free(&inf);
}

static int load_text(const char *json, RdrProject *p, StrList *warnings, char *err, size_t errlen)
{
    JsonValue *doc = json_parse(json, strlen(json), err, errlen);
    int ok;

    if (doc == NULL) {
        return 0;
    }
    ok = kb_from_json(doc, p, warnings, err, errlen);
    json_free(doc);
    return ok;
}

/* ===================================================================== */
/* Preference Tests                                                      */
/* ===================================================================== */

TEST(prefs_basics)
{
    Preferences p;
    Preferences q;
    PrefId id;
    char err[128];
    char *s;
    int i;

    prefs_init_defaults(&p);
    CHECK_INT_EQ(prefs_get(&p, PREF_TREE_SHOW_EMPTY_SLOTS), 1);
    CHECK_INT_EQ(prefs_get(&p, PREF_ACQ_REVISION_STRATEGY), RDR_REVISION_NODE);
    CHECK_INT_EQ(prefs_get(&p, PREF_UI_FONT_SIZE), 16);
    CHECK(!prefs_set(&p, PREF_UI_FONT_SIZE, 99));
    CHECK(prefs_set(&p, PREF_UI_FONT_SIZE, 20));
    CHECK(prefs_find("ui_theme", &id) && id == PREF_UI_THEME);
    CHECK(!prefs_find("nope", &id));
    CHECK(prefs_set_from_text(&p, PREF_UI_THEME, "dark", err, sizeof(err)));
    CHECK_INT_EQ(prefs_get(&p, PREF_UI_THEME), PREF_THEME_DARK);
    CHECK(!prefs_set_from_text(&p, PREF_UI_THEME, "blue", err, sizeof(err)));
    CHECK(prefs_set_from_text(&p, PREF_TREE_HIGHLIGHT, "false", err, sizeof(err)));
    s = prefs_value_text(PREF_ACQ_REVISION_STRATEGY, RDR_REVISION_REJECT);
    CHECK_STR_EQ(s, "reject");
    free(s);

    prefs_init_defaults(&q);  /* setiap deskriptor punya key unik dan default valid */
    for (i = 0; i < PREF_COUNT; i++) {
        CHECK(prefs_descriptor((PrefId)i)->key != NULL);
        CHECK(prefs_set(&q, (PrefId)i, prefs_descriptor((PrefId)i)->default_value));
        CHECK(prefs_find(prefs_descriptor((PrefId)i)->key, &id) && (int)id == i);
    }
}

TEST(prefs_json)
{
    Preferences p;
    Preferences q;
    StrList warnings;
    JsonValue *obj;
    const char *partial = "{\"ui_font_size\": 22, \"ui_theme\": \"neon\", \"bogus\": 1}";
    JsonValue *doc;

    prefs_init_defaults(&p);
    prefs_set(&p, PREF_UI_FONT_SIZE, 18);
    prefs_set(&p, PREF_ACQ_CLAUSE_SELECTION, PREF_CLAUSE_STRICT);
    obj = prefs_to_json(&p);
    prefs_init_defaults(&q);
    CHECK_INT_EQ(prefs_from_json(&q, obj, NULL), 2);
    CHECK(prefs_equal(&p, &q));
    json_free(obj);

    strlist_init(&warnings);  /* impor parsial */
    doc = json_parse(partial, strlen(partial), NULL, 0);
    CHECK_INT_EQ(prefs_from_json(&q, doc, &warnings), 1);
    CHECK_INT_EQ(prefs_get(&q, PREF_UI_FONT_SIZE), 22);
    CHECK_INT_EQ(prefs_get(&q, PREF_ACQ_CLAUSE_SELECTION), PREF_CLAUSE_STRICT);
    CHECK_INT_EQ(warnings.count, 2);
    json_free(doc);
    strlist_free(&warnings);
}

/* ===================================================================== */
/* Project File Tests                                                    */
/* ===================================================================== */

TEST(project_round_trip)
{
    RdrKb *kb = rdr_kb_create("Sehat");
    RdrProject p;
    RdrSession s;
    Preferences prefs;
    RdrInference inf;
    StrList warnings;
    char err[256];
    char *text1;
    char *text2;
    JsonValue *doc;
    const char *path = TMP_DIR "/roundtrip.rdr.json";

    teach(kb, "demam", "Flu");
    teach(kb, "demam; batuk", "Bronkitis");
    teach(kb, "demam; ruam", "Campak");
    teach(kb, "demam", "Demam Biasa");  /* revisi */

    prefs_init_defaults(&prefs);
    prefs_set(&prefs, PREF_UI_THEME, PREF_THEME_DARK);
    rdr_session_init(&s);
    strlist_push(&s.query_draft, "demam");
    strlist_push(&s.query_draft, "x >");  /* draft boleh berisi klausa tidak valid */
    query_parse("demam; batuk", &s.last_query, NULL);
    s.has_last_query = 1;
    s.last_evaluated = 2;
    s.last_concluded = 2;
    s.result_tab = SESSION_RESULT_TAB_TREE;

    /* Save and Load */
    CHECK(kb_save_file(path, kb, &prefs, &s, err, sizeof(err)));
    rdr_project_init(&p);
    strlist_init(&warnings);
    CHECK(kb_load_file(path, &p, &warnings, err, sizeof(err)));
    CHECK_INT_EQ(warnings.count, 0);
    CHECK_INT_EQ(p.kb->node_count, kb->node_count);
    CHECK_INT_EQ(p.kb->next_id, kb->next_id);
    CHECK_INT_EQ(rdr_kb_find(p.kb, 4)->revises, 1);
    CHECK(prefs_equal(&p.prefs, &prefs));
    CHECK_INT_EQ(p.session.query_draft.count, 2);
    CHECK_STR_EQ(p.session.query_draft.items[1], "x >");
    CHECK(p.session.has_last_query);
    CHECK_INT_EQ(p.session.last_query.count, 2);
    CHECK_INT_EQ(p.session.last_concluded, 2);
    CHECK_INT_EQ(p.session.result_tab, SESSION_RESULT_TAB_TREE);
    CHECK_INT_EQ(p.session.query_tab, SESSION_QUERY_TAB_CLAUSE);

    /* Behavior Preservation */
    rdr_inference_init(&inf);
    CHECK_STR_EQ(infer_text(p.kb, "demam", &inf), "Demam Biasa");
    CHECK_STR_EQ(infer_text(p.kb, "demam; ruam", &inf), "Campak");
    rdr_inference_free(&inf);

    /* Stable Output */
    doc = kb_to_json(kb, &prefs, &s);
    text1 = json_write(doc);
    json_free(doc);
    doc = kb_to_json(p.kb, &p.prefs, &p.session);
    text2 = json_write(doc);
    json_free(doc);
    CHECK_STR_EQ(text1, text2);
    CHECK(strstr(text1, "\"rule\": [\"batuk\"]") != NULL);
    free(text1);
    free(text2);

    strlist_free(&warnings);
    rdr_session_free(&s);
    rdr_project_free(&p);
    rdr_kb_destroy(kb);
    remove(path);
}

TEST(project_minimal_and_errors)
{
    RdrProject p;
    StrList w;
    char err[256];

    rdr_project_init(&p);
    strlist_init(&w);

    /* Minimal Document */
    CHECK(load_text("{\"format\":\"rdr-kb\",\"version\":1,\"root\":{\"id\":0,\"rule\":[],"
                    "\"conclusion\":\"0%\",\"true\":null,\"false\":null}}", &p, &w, err, sizeof(err)));
    CHECK_STR_EQ(p.kb->root->conclusion, "0%");
    CHECK_INT_EQ(prefs_get(&p.prefs, PREF_UI_FONT_SIZE), 16);
    CHECK(!p.session.has_last_query);

    CHECK(!load_text("{\"format\":\"other\",\"version\":1}", &p, &w, err, sizeof(err)));
    CHECK(strstr(err, "not an RDR project") != NULL);
    CHECK(!load_text("{\"format\":\"rdr-kb\",\"version\":2}", &p, &w, err, sizeof(err)));
    CHECK(!load_text("{\"format\":\"rdr-kb\",\"version\":1,\"root\":{\"id\":0,\"rule\":[\"a\"],"
                     "\"conclusion\":\"x\"}}", &p, &w, err, sizeof(err)));
    CHECK(strstr(err, "root rule") != NULL);
    CHECK(!load_text("{\"format\":\"rdr-kb\",\"version\":1,\"root\":{\"id\":0,\"rule\":[],"
                     "\"conclusion\":\"x\",\"true\":{\"id\":1,\"rule\":[\"a >\"],\"conclusion\":\"y\"}}}",
                     &p, &w, err, sizeof(err)));
    CHECK(strstr(err, "node #1") != NULL);
    CHECK(!load_text("{\"format\":\"rdr-kb\",\"version\":1,\"root\":{\"id\":0,\"rule\":[],"
                     "\"conclusion\":\"x\",\"true\":{\"id\":1,\"rule\":[\"a\"],\"conclusion\":\"y\","
                     "\"true\":{\"id\":1,\"rule\":[\"b\"],\"conclusion\":\"z\"}}}}",
                     &p, &w, err, sizeof(err)));
    CHECK(strstr(err, "already used") != NULL);
    CHECK(!load_text("{\"format\":\"rdr-kb\",\"version\":1,\"root\":{\"id\":0,\"rule\":[],"
                     "\"conclusion\":\"  \"}}", &p, &w, err, sizeof(err)));
    CHECK(p.kb == NULL);

    strlist_clear(&w);
    CHECK(load_text("{\"format\":\"rdr-kb\",\"version\":1,\"root\":{\"id\":0,\"rule\":[],"
                    "\"conclusion\":\"x\",\"true\":{\"id\":5,\"rule\":[\"a\"],\"conclusion\":\"y\","
                    "\"revises\":9}},\"session\":{\"last_query\":[\"a >\"]}}", &p, &w, err, sizeof(err)));
    CHECK_INT_EQ(w.count, 2);
    CHECK_INT_EQ(rdr_kb_find(p.kb, 5)->revises, RDR_NO_REVISION);
    CHECK_INT_EQ(p.kb->next_id, 6);
    CHECK(!p.session.has_last_query);

    strlist_free(&w);
    rdr_project_free(&p);
}

TEST(import_prefs)
{
    RdrKb *kb = rdr_kb_create("A");
    Preferences src;
    Preferences dst;
    size_t changed = 0;
    char err[256];
    const char *path = TMP_DIR "/prefs_src.rdr.json";

    prefs_init_defaults(&src);
    prefs_set(&src, PREF_UI_FONT_SIZE, 24);
    prefs_set(&src, PREF_TREE_SHOW_EMPTY_SLOTS, 0);
    CHECK(kb_save_file(path, kb, &src, NULL, err, sizeof(err)));
    prefs_init_defaults(&dst);
    CHECK(kb_import_prefs_file(path, &dst, &changed, NULL, err, sizeof(err)));
    CHECK_INT_EQ(changed, 2);
    CHECK(prefs_equal(&src, &dst));
    CHECK(!kb_import_prefs_file(TMP_DIR "/does_not_exist.json", &dst, &changed, NULL, err, sizeof(err)));
    remove(path);
    rdr_kb_destroy(kb);
}

/* ===================================================================== */
/* CSV Tests                                                             */
/* ===================================================================== */

TEST(csv_parse)
{
    CsvTable t;
    char err[128];
    const char *text = "\xEF\xBB\xBF" "case,conclusion\r\n"
                       "\"demam; suhu = 39\",Flu\r\n"
                       "\n"
                       "\"a, b\",\"say \"\"hi\"\"\"\n"
                       "\"multi\nline\",x\n"
                       "last,";

    csv_table_init(&t);
    CHECK(csv_parse(text, strlen(text), &t, err, sizeof(err)));
    CHECK_INT_EQ(t.count, 5);
    CHECK_STR_EQ(t.rows[0].items[0], "case");
    CHECK_STR_EQ(t.rows[1].items[0], "demam; suhu = 39");
    CHECK_STR_EQ(t.rows[2].items[0], "a, b");
    CHECK_STR_EQ(t.rows[2].items[1], "say \"hi\"");
    CHECK_INT_EQ(t.lines[2], 4);
    CHECK_STR_EQ(t.rows[3].items[0], "multi\nline");
    CHECK_INT_EQ(t.lines[4], 7);
    CHECK_INT_EQ(t.rows[4].count, 2);
    CHECK_STR_EQ(t.rows[4].items[1], "");

    CHECK(!csv_parse("\"open", 5, &t, err, sizeof(err)));
    CHECK(strstr(err, "unterminated") != NULL);
    CHECK(!csv_parse("\"a\"b,c", 6, &t, err, sizeof(err)));
    csv_table_free(&t);
}

TEST(csv_import)
{
    RdrKb *kb = rdr_kb_create("Sehat");
    CsvImportReport r;
    RdrInference inf;
    char err[256];
    char *summary;
    const char *data =
        "Case , Conclusion\n"
        "demam,Flu\n"
        "\"demam; batuk\",Bronkitis\n"
        "demam,Flu\n"
        "\"demam; ruam\",Campak\n"
        "batuk,Alergi\n"
        "demam,Demam Biasa\n"
        "\"x >\",Bad\n"
        "\"a; !a\",Bad\n"
        "only_one_column\n"
        "y,\n";

    csv_report_init(&r);
    CHECK(kb_csv_import_text(kb, data, strlen(data), RDR_REVISION_NODE, &r, err, sizeof(err)));
    CHECK_INT_EQ(r.rows, 10);
    CHECK_INT_EQ(r.exceptions_added, 4);
    CHECK_INT_EQ(r.revisions_added, 1);
    CHECK_INT_EQ(r.already_correct, 1);
    CHECK_INT_EQ(r.failed, 4);
    summary = csv_report_summary(&r);
    CHECK(strncmp(summary, "10 rows", 7) == 0);
    free(summary);

    rdr_inference_init(&inf);
    CHECK_STR_EQ(infer_text(kb, "demam", &inf), "Demam Biasa");
    CHECK_STR_EQ(infer_text(kb, "batuk", &inf), "Alergi");
    rdr_inference_free(&inf);

    CHECK(!kb_csv_import_text(kb, "a,b\n", 4, RDR_REVISION_NODE, &r, err, sizeof(err)));
    CHECK(strstr(err, "header") != NULL);
    csv_report_free(&r);
    rdr_kb_destroy(kb);
}

/* ===================================================================== */
/* Platform Tests                                                        */
/* ===================================================================== */

TEST(platform_paths)
{
    char *s;
    PlatformDirList list;
    char err[128];
    char *data = NULL;
    size_t len = 0;

    s = platform_path_join("/a/b", "c.json");
    CHECK_STR_EQ(s, "/a/b/c.json");
    free(s);
    s = platform_path_join("/", "c");
    CHECK_STR_EQ(s, "/c");
    free(s);
    s = platform_path_parent("/a/b/c.json");
    CHECK_STR_EQ(s, "/a/b");
    free(s);
    s = platform_path_parent("/a");
    CHECK_STR_EQ(s, "/");
    free(s);
    s = platform_path_parent("/");
    CHECK_STR_EQ(s, "/");
    free(s);
    CHECK_STR_EQ(platform_path_basename("/a/b/c.json"), "c.json");
    CHECK(platform_path_has_ext("x.RDR.JSON", ".json"));
    CHECK(!platform_path_has_ext("x.csv", ".json"));

    CHECK(platform_write_file(TMP_DIR "/plat.txt", "hello", 5, err, sizeof(err)));
    CHECK(platform_read_file(TMP_DIR "/plat.txt", &data, &len, err, sizeof(err)));
    CHECK_INT_EQ(len, 5);
    CHECK_STR_EQ(data, "hello");
    free(data);
    CHECK(platform_file_exists(TMP_DIR "/plat.txt"));
    CHECK(!platform_is_dir(TMP_DIR "/plat.txt"));
    CHECK(platform_is_dir(TMP_DIR));
    remove(TMP_DIR "/plat.txt");

    platform_dirlist_init(&list);
    CHECK(platform_list_dir("include", &list, err, sizeof(err)));
    CHECK(list.count >= 1);
    CHECK_STR_EQ(list.items[0].name, "rdr");
    CHECK(list.items[0].is_dir);
    CHECK(!platform_list_dir("no_such_dir_xyz", &list, err, sizeof(err)));
    platform_dirlist_free(&list);
}

int main(void)
{
    RUN(prefs_basics);
    RUN(prefs_json);
    RUN(project_round_trip);
    RUN(project_minimal_and_errors);
    RUN(import_prefs);
    RUN(csv_parse);
    RUN(csv_import);
    RUN(platform_paths);
    return TEST_REPORT();
}
