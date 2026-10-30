/**
 * @file test_rdr.c
 * @brief Unit test engine RDR (inferensi, akuisisi, revisi, edit manual, validasi, render).
 */

#include <stdlib.h>
#include <string.h>

#include "rdr/query.h"
#include "rdr/rdr.h"
#include "rdr/rdr_print.h"
#include "test.h"

/* ===================================================================== */
/* Test Helpers                                                          */
/* ===================================================================== */

static ClauseList parse(const char *text)
{
    ClauseList l;
    clauselist_init(&l);
    query_parse(text, &l, NULL);
    return l;
}

/* Menjalankan inferensi dan mengembalikan kesimpulannya. */
static const char *infer(const RdrKb *kb, const char *query, RdrInference *inf)
{
    ClauseList q = parse(query);
    int ok = rdr_infer(kb, &q, inf, NULL);
    clauselist_free(&q);
    return ok ? rdr_inference_conclusion(kb, inf) : NULL;
}

/* Menjalankan inferensi lalu koreksi dalam satu langkah. */
static RdrAcqResult teach(RdrKb *kb, const char *query, const char *conclusion,
                          RdrRevisionStrategy strategy)
{
    RdrInference inf;
    RdrAcqResult r;

    rdr_inference_init(&inf);
    infer(kb, query, &inf);
    r = rdr_correct(kb, &inf, conclusion, NULL, strategy);
    rdr_inference_free(&inf);
    return r;
}

static char *rule_of(const RdrKb *kb, int id)
{
    return clauselist_rule_text(&rdr_kb_find(kb, id)->rule);
}

static void check_rule(const RdrKb *kb, int id, const char *expected)
{
    char *s = rule_of(kb, id);
    CHECK_STR_EQ(s, expected);
    free(s);
}

/* Membangun KB contoh diagnosis yang dipakai beberapa test. */
static RdrKb *build_medical_kb(void)
{
    RdrKb *kb = rdr_kb_create("Sehat");
    teach(kb, "demam", "Flu", RDR_REVISION_NODE);                   /* #1 */
    teach(kb, "demam; batuk", "Bronkitis", RDR_REVISION_NODE);      /* #2 */
    teach(kb, "demam; ruam", "Campak", RDR_REVISION_NODE);          /* #3 */
    teach(kb, "batuk", "Alergi", RDR_REVISION_NODE);                /* #4 */
    return kb;
}

/* ===================================================================== */
/* Knowledge Base Tests                                                  */
/* ===================================================================== */

TEST(create)
{
    RdrKb *kb = rdr_kb_create("  Sehat ");
    RdrInference inf;

    CHECK(kb != NULL);
    CHECK_STR_EQ(kb->root->conclusion, "Sehat");
    CHECK_INT_EQ(kb->root->id, 0);
    CHECK_INT_EQ(kb->node_count, 1);
    CHECK(rdr_kb_create("   ") == NULL);
    CHECK(rdr_kb_create(NULL) == NULL);

    rdr_inference_init(&inf);
    CHECK_STR_EQ(infer(kb, "apa_saja", &inf), "Sehat");
    CHECK_INT_EQ(inf.last_evaluated, 0);
    CHECK_INT_EQ(inf.last_concluded, 0);
    CHECK_INT_EQ(inf.trace.count, 1);
    rdr_inference_free(&inf);
    rdr_kb_destroy(kb);
}

TEST(acquisition_sides)
{
    RdrKb *kb = build_medical_kb();
    RdrNode *root = kb->root;

    CHECK_INT_EQ(kb->node_count, 5);
    CHECK(root->true_child != NULL && root->true_child->id == 1);  /* exception masuk slot true */
    CHECK(root->false_child == NULL);
    CHECK(rdr_kb_find(kb, 1)->true_child->id == 2);
    CHECK(rdr_kb_find(kb, 2)->false_child->id == 3);  /* #2 gagal, slot false */
    CHECK(rdr_kb_find(kb, 1)->false_child->id == 4);  /* #1 gagal, slot false */

    check_rule(kb, 1, "demam");
    check_rule(kb, 2, "batuk");
    check_rule(kb, 3, "ruam");
    check_rule(kb, 4, "batuk");
    CHECK_INT_EQ(rdr_kb_find(kb, 2)->cornerstone.count, 2);
    rdr_kb_destroy(kb);
}

TEST(inference_trace)
{
    RdrKb *kb = build_medical_kb();
    RdrInference inf;

    rdr_inference_init(&inf);
    CHECK_STR_EQ(infer(kb, "demam; batuk", &inf), "Bronkitis");
    CHECK_INT_EQ(inf.trace.count, 3);
    CHECK_INT_EQ(inf.trace.items[2], 2);
    CHECK_INT_EQ(inf.last_evaluated, 2);

    CHECK_STR_EQ(infer(kb, "demam; ruam", &inf), "Campak");
    CHECK_INT_EQ(rdr_inference_status(&inf, 2), RDR_NODE_FAILED);
    CHECK_INT_EQ(rdr_inference_status(&inf, 3), RDR_NODE_FIRED);
    CHECK_INT_EQ(rdr_inference_status(&inf, 4), RDR_NODE_IDLE);

    CHECK_STR_EQ(infer(kb, "demam; pusing", &inf), "Flu");
    CHECK_INT_EQ(inf.last_evaluated, 3);
    CHECK_INT_EQ(inf.last_concluded, 1);

    CHECK_STR_EQ(infer(kb, "pusing", &inf), "Sehat");
    CHECK_INT_EQ(inf.last_evaluated, 4);
    CHECK_INT_EQ(inf.last_concluded, 0);

    CHECK(infer(kb, "demam; !demam", &inf) == NULL);
    CHECK(!inf.valid);
    rdr_inference_free(&inf);
    rdr_kb_destroy(kb);
}

TEST(difference_list)
{
    ClauseList q = parse("demam; suhu = 37; batuk");
    ClauseList cs = parse("demam; suhu = 39");
    ClauseList d;
    char *s;

    clauselist_init(&d);
    rdr_difference(&q, &cs, &d);
    s = query_format(&d);
    CHECK_STR_EQ(s, "suhu = 37; batuk");
    free(s);
    clauselist_free(&q);
    clauselist_free(&cs);
    clauselist_free(&d);
}

/* ===================================================================== */
/* Correction Tests                                                      */
/* ===================================================================== */

TEST(revision_node)
{
    RdrKb *kb = build_medical_kb();
    RdrInference inf;
    RdrAcqResult r;
    RdrNode *rev;

    r = teach(kb, "demam", "Demam Biasa", RDR_REVISION_NODE);  /* D kosong */
    CHECK_INT_EQ(r.status, RDR_ACQ_REVISION_ADDED);
    rev = rdr_kb_find(kb, r.node_id);
    CHECK(rev != NULL);
    CHECK_INT_EQ(rev->rule.count, 0);
    CHECK_INT_EQ(rev->revises, 1);
    CHECK(rdr_kb_find(kb, 3)->false_child == rev);  /* ujung penelusuran #3 */

    rdr_inference_init(&inf);
    CHECK_STR_EQ(infer(kb, "demam", &inf), "Demam Biasa");
    CHECK_STR_EQ(infer(kb, "demam; batuk", &inf), "Bronkitis");
    CHECK_STR_EQ(infer(kb, "demam; ruam", &inf), "Campak");
    rdr_inference_free(&inf);
    rdr_kb_destroy(kb);
}

TEST(revision_on_leaf_uses_true_slot)
{
    RdrKb *kb = rdr_kb_create("Sehat");
    RdrAcqResult r;

    teach(kb, "demam", "Flu", RDR_REVISION_NODE);
    r = teach(kb, "demam", "Demam", RDR_REVISION_NODE);
    CHECK_INT_EQ(r.status, RDR_ACQ_REVISION_ADDED);
    CHECK(rdr_kb_find(kb, 1)->true_child->id == r.node_id);  /* #1 tanpa exception */
    rdr_kb_destroy(kb);
}

TEST(revision_edit_and_reject)
{
    RdrKb *kb = build_medical_kb();
    RdrInference inf;
    RdrAcqResult r;
    size_t before = kb->node_count;

    r = teach(kb, "demam", "Demam Biasa", RDR_REVISION_REJECT);
    CHECK_INT_EQ(r.status, RDR_ACQ_REJECTED);
    CHECK_INT_EQ(kb->node_count, before);

    r = teach(kb, "demam", "Demam Biasa", RDR_REVISION_EDIT_CONCLUSION);
    CHECK_INT_EQ(r.status, RDR_ACQ_CONCLUSION_EDITED);
    CHECK_INT_EQ(r.node_id, 1);
    CHECK_INT_EQ(kb->node_count, before);
    CHECK_STR_EQ(rdr_kb_find(kb, 1)->conclusion, "Demam Biasa");

    rdr_inference_init(&inf);
    CHECK_STR_EQ(infer(kb, "demam", &inf), "Demam Biasa");
    rdr_inference_free(&inf);

    r = teach(kb, "", "Tidak Diketahui", RDR_REVISION_EDIT_CONCLUSION);  /* mengubah default root */
    CHECK_INT_EQ(r.status, RDR_ACQ_CONCLUSION_EDITED);
    CHECK_STR_EQ(kb->root->conclusion, "Tidak Diketahui");
    CHECK_INT_EQ(kb->root->cornerstone.count, 0);
    rdr_kb_destroy(kb);
}

TEST(custom_rule)
{
    RdrKb *kb = rdr_kb_create("Normal");
    RdrInference inf;
    RdrAcqResult r;
    ClauseList rule;

    teach(kb, "suhu = 39", "Demam", RDR_REVISION_NODE);
    rdr_inference_init(&inf);
    CHECK_STR_EQ(infer(kb, "suhu = 37.5; lemas", &inf), "Normal");  /* #1 tidak menyala */

    rule = parse("suhu > 38");  /* rule yang tidak terpenuhi kueri ditolak */
    r = rdr_correct(kb, &inf, "Subfebris", &rule, RDR_REVISION_NODE);
    CHECK_INT_EQ(r.status, RDR_ACQ_ERROR);
    clauselist_free(&rule);

    clauselist_init(&rule);  /* rule kosong ditolak */
    r = rdr_correct(kb, &inf, "Subfebris", &rule, RDR_REVISION_NODE);
    CHECK_INT_EQ(r.status, RDR_ACQ_ERROR);

    rule = parse("suhu > 37; suhu < 38");  /* generalisasi yang valid diterima */
    r = rdr_correct(kb, &inf, "Subfebris", &rule, RDR_REVISION_NODE);
    CHECK_INT_EQ(r.status, RDR_ACQ_EXCEPTION_ADDED);
    clauselist_free(&rule);
    check_rule(kb, r.node_id, "suhu > 37 AND suhu < 38");

    CHECK_STR_EQ(infer(kb, "suhu = 37.2", &inf), "Subfebris");
    rdr_inference_free(&inf);
    rdr_kb_destroy(kb);
}

TEST(rule_must_exclude_cornerstone)
{
    ClauseList q = parse("demam; batuk");
    ClauseList cs = parse("demam");
    ClauseList rule = parse("demam");
    char err[256];

    CHECK(!rdr_validate_exception_rule(&q, &cs, &rule, err, sizeof(err)));
    CHECK(strstr(err, "cornerstone") != NULL);
    clauselist_free(&rule);
    rule = parse("batuk");
    CHECK(rdr_validate_exception_rule(&q, &cs, &rule, err, sizeof(err)));
    clauselist_free(&q);
    clauselist_free(&cs);
    clauselist_free(&rule);
}

TEST(stale_and_unchanged)
{
    RdrKb *kb = rdr_kb_create("Sehat");
    RdrInference inf;
    RdrAcqResult r;

    rdr_inference_init(&inf);
    r = rdr_correct(kb, &inf, "Flu", NULL, RDR_REVISION_NODE);
    CHECK_INT_EQ(r.status, RDR_ACQ_ERROR);
    CHECK(strstr(r.message, "no result") != NULL);
    infer(kb, "demam", &inf);
    r = rdr_correct(kb, &inf, "Sehat", NULL, RDR_REVISION_NODE);
    CHECK_INT_EQ(r.status, RDR_ACQ_UNCHANGED);
    r = rdr_correct(kb, &inf, "   ", NULL, RDR_REVISION_NODE);
    CHECK_INT_EQ(r.status, RDR_ACQ_ERROR);
    r = rdr_correct(kb, &inf, "Flu", NULL, RDR_REVISION_NODE);
    CHECK_INT_EQ(r.status, RDR_ACQ_EXCEPTION_ADDED);
    r = rdr_correct(kb, &inf, "Pilek", NULL, RDR_REVISION_NODE);  /* inferensi yang sama kini usang */
    CHECK_INT_EQ(r.status, RDR_ACQ_ERROR);
    CHECK(!rdr_inference_is_current(kb, &inf));
    rdr_inference_free(&inf);
    rdr_kb_destroy(kb);
}

/* ===================================================================== */
/* Manual Edit and Validation Tests                                      */
/* ===================================================================== */

TEST(manual_edit)
{
    RdrKb *kb = build_medical_kb();
    ClauseList rule = parse("x");
    ClauseList empty;
    char err[256];
    int id = -1;
    size_t removed = 0;

    clauselist_init(&empty);
    CHECK(!rdr_node_set_rule(kb, 0, &rule, err, sizeof(err)));
    CHECK(rdr_node_set_rule(kb, 0, &empty, err, sizeof(err)));
    CHECK(rdr_node_set_rule(kb, 4, &rule, err, sizeof(err)));
    check_rule(kb, 4, "x");
    CHECK(!rdr_node_set_conclusion(kb, 4, " ", err, sizeof(err)));
    CHECK(rdr_node_set_conclusion(kb, 0, "Baik", err, sizeof(err)));
    CHECK_STR_EQ(kb->root->conclusion, "Baik");
    CHECK(!rdr_node_set_cornerstone(kb, 0, &rule, err, sizeof(err)));
    CHECK(!rdr_node_set_conclusion(kb, 99, "Z", err, sizeof(err)));

    CHECK(!rdr_node_add_child(kb, 0, RDR_SIDE_FALSE, &rule, "Z", NULL, &id, err, sizeof(err)));
    CHECK(!rdr_node_add_child(kb, 0, RDR_SIDE_TRUE, &rule, "Z", NULL, &id, err, sizeof(err)));
    CHECK(rdr_node_add_child(kb, 4, RDR_SIDE_TRUE, &empty, "Z", NULL, &id, err, sizeof(err)));
    CHECK_INT_EQ(id, 5);

    CHECK(!rdr_node_delete(kb, 0, &removed, err, sizeof(err)));
    CHECK(rdr_node_delete(kb, 1, &removed, err, sizeof(err)));
    CHECK_INT_EQ(removed, 5);
    CHECK_INT_EQ(kb->node_count, 1);
    CHECK(rdr_kb_find(kb, 3) == NULL);
    CHECK(kb->root->true_child == NULL);
    CHECK(rdr_node_add_child(kb, 0, RDR_SIDE_TRUE, &rule, "Y", NULL, &id, err, sizeof(err)));  /* id tidak dipakai ulang */
    CHECK_INT_EQ(id, 6);

    clauselist_free(&rule);
    rdr_kb_destroy(kb);
}

TEST(validation)
{
    RdrKb *kb = build_medical_kb();
    RdrValidation v;
    ClauseList rule;
    char err[256];

    rdr_validate(kb, &v);
    CHECK_INT_EQ(v.checked, 4);
    CHECK_INT_EQ(v.count, 0);
    rdr_validation_free(&v);

    teach(kb, "demam", "Demam Biasa", RDR_REVISION_NODE);  /* revisi menjadi superseded */
    rdr_validate(kb, &v);
    CHECK_INT_EQ(v.errors, 0);
    CHECK_INT_EQ(v.count, 1);
    CHECK_INT_EQ(v.items[0].kind, RDR_ISSUE_SUPERSEDED);
    CHECK_INT_EQ(v.items[0].node_id, 1);
    rdr_validation_free(&v);

    rule = parse("tidak_ada");  /* merusak cornerstone #2 */
    rdr_node_set_rule(kb, 2, &rule, err, sizeof(err));
    clauselist_free(&rule);
    rdr_validate(kb, &v);
    CHECK_INT_EQ(v.errors, 1);
    rdr_validation_free(&v);
    rdr_kb_destroy(kb);
}

/* ===================================================================== */
/* Rendering and Scale Tests                                             */
/* ===================================================================== */

TEST(print_tree)
{
    RdrKb *kb = rdr_kb_create("Miskin");
    RdrPrintOptions opt;
    RdrLines lines;
    char *text;

    teach(kb, "uang > 100", "Butuh Duit", RDR_REVISION_NODE);
    teach(kb, "uang > 100; budi = kaya", "Banyak Duit", RDR_REVISION_NODE);

    rdr_lines_init(&lines);
    rdr_print_default_options(&opt);
    rdr_print_tree(kb, &opt, &lines);
    text = rdr_lines_join(&lines);
    CHECK_STR_EQ(text,
        "#0 (T >> Miskin)\n"
        "├─T─ #1 (uang > 100 >> Butuh Duit)\n"
        "│    ├─T─ #2 (budi = kaya >> Banyak Duit)\n"
        "│    │    ├─T─ ∅\n"
        "│    │    └─F─ ∅\n"
        "│    └─F─ ∅\n"
        "└─F─ ∅\n");
    free(text);
    CHECK_INT_EQ(lines.items[2].node_id, 2);
    CHECK_INT_EQ(lines.items[3].kind, RDR_LINE_EMPTY);

    opt.show_empty_slots = 0;
    opt.show_node_id = 0;
    rdr_print_tree(kb, &opt, &lines);
    text = rdr_lines_join(&lines);
    CHECK_STR_EQ(text,
        "(T >> Miskin)\n"
        "└─T─ (uang > 100 >> Butuh Duit)\n"
        "     └─T─ (budi = kaya >> Banyak Duit)\n");
    free(text);

    opt.show_cornerstone = 1;
    rdr_print_tree(kb, &opt, &lines);
    CHECK_INT_EQ(lines.count, 5);
    CHECK_INT_EQ(lines.items[2].kind, RDR_LINE_CORNERSTONE);
    CHECK_STR_EQ(lines.items[2].text, "cornerstone: uang > 100");

    rdr_lines_free(&lines);
    rdr_kb_destroy(kb);
}

static int count_visit(const RdrNode *node, int depth, void *user)
{
    (void)node;
    (void)depth;
    (*(size_t *)user)++;
    return 0;
}

TEST(deep_chain)
{
    RdrKb *kb = rdr_kb_create("Base");
    RdrNode *cur;
    RdrInference inf;
    RdrLines lines;
    RdrPrintOptions opt;
    size_t visited = 0;
    int i;
    char buf[32];

    /* Deep False Chain */
    cur = rdr_kb_insert(kb, kb->root, RDR_SIDE_TRUE, -1, NULL, "n", NULL, RDR_NO_REVISION, NULL, 0);
    for (i = 0; i < 20000; i++) {
        ClauseList rule;
        sprintf(buf, "v%d", i);
        rule = parse(buf);
        cur = rdr_kb_insert(kb, cur, RDR_SIDE_FALSE, -1, &rule, buf, NULL, RDR_NO_REVISION, NULL, 0);
        clauselist_free(&rule);
    }
    CHECK_INT_EQ(kb->node_count, 20002);
    rdr_kb_foreach(kb, count_visit, &visited);
    CHECK_INT_EQ(visited, 20002);

    rdr_inference_init(&inf);
    CHECK_STR_EQ(infer(kb, "v19999", &inf), "n");  /* node T pertama langsung menyala */
    rdr_inference_free(&inf);

    rdr_lines_init(&lines);
    rdr_print_default_options(&opt);
    opt.show_empty_slots = 0;
    rdr_print_tree(kb, &opt, &lines);
    CHECK_INT_EQ(lines.count, 20002);
    rdr_lines_free(&lines);
    rdr_kb_destroy(kb);
}

int main(void)
{
    RUN(create);
    RUN(acquisition_sides);
    RUN(inference_trace);
    RUN(difference_list);
    RUN(revision_node);
    RUN(revision_on_leaf_uses_true_slot);
    RUN(revision_edit_and_reject);
    RUN(custom_rule);
    RUN(rule_must_exclude_cornerstone);
    RUN(stale_and_unchanged);
    RUN(manual_edit);
    RUN(validation);
    RUN(print_tree);
    RUN(deep_chain);
    return TEST_REPORT();
}
