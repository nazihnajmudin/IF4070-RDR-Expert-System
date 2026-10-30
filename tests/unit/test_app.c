/**
 * @file test_app.c
 * @brief Unit test controller aplikasi.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rdr/app.h"
#include "rdr/query.h"
#include "test.h"

#define TMP_DIR "build/test"

static void run_query(App *app, const char *text)
{
    app_draft_set_text(app, text);
    app_start(app);
}

TEST(lifecycle)
{
    App *app = app_create("Sehat");
    char *title;

    CHECK(app != NULL);
    CHECK(app_create("  ") == NULL);
    CHECK(!app->dirty);
    title = app_title(app);
    CHECK_STR_EQ(title, "RDR Expert System - Untitled");
    free(title);
    CHECK(!app_new(app, ""));
    CHECK_INT_EQ(app->status_level, APP_STATUS_ERROR);
    CHECK(app_new(app, "Normal"));
    CHECK_STR_EQ(app->kb->root->conclusion, "Normal");
    CHECK(!app_save(app));  /* belum ada path */
    app_destroy(app);
}

TEST(draft_operations)
{
    App *app = app_create("X");
    char *text;
    char err[128];

    CHECK_INT_EQ(app_draft_add(app, "a; b = 1 ;\n c > 2"), 3);
    CHECK_INT_EQ(app_draft_add(app, "a; d"), 1);  /* "a" sudah ada */
    CHECK_INT_EQ(app->draft.count, 4);
    app_draft_set(app, 1, "  b = 2 ");
    CHECK_STR_EQ(app->draft.items[1], "b = 2");
    app_draft_set(app, 3, "   ");
    CHECK_INT_EQ(app->draft.count, 3);
    app_draft_remove(app, 0);
    text = app_draft_text(app);
    CHECK_STR_EQ(text, "b = 2; c > 2");
    free(text);
    app_start(app);
    CHECK(app_has_result(app));
    app_draft_add(app, "bad >");
    CHECK(!app_draft_item_valid(app, 2, err, sizeof(err)));
    CHECK(app_draft_item_valid(app, 0, err, sizeof(err)));
    CHECK(!app_start(app));
    CHECK(!app_has_result(app));  /* hasil lama dibuang saat Start gagal */
    CHECK_INT_EQ(app->status_level, APP_STATUS_ERROR);
    CHECK_INT_EQ(app->details.count, 1);
    app_clear(app);
    CHECK_INT_EQ(app->draft.count, 0);
    CHECK(!app_has_result(app));
    app_destroy(app);
}

TEST(reasoning_and_correction)
{
    App *app = app_create("Sehat");
    AppCorrectionPlan plan;
    RdrAcqResult r;
    ClauseList rule;

    run_query(app, "demam; !demam");
    CHECK(!app_has_result(app));
    CHECK(strstr(app->details.items[0], "Contradiction") != NULL);

    run_query(app, "demam; suhu = 39");
    CHECK(app_has_result(app));
    CHECK_STR_EQ(app_conclusion(app), "Sehat");
    CHECK(app_correction_plan(app, &plan));
    CHECK_INT_EQ(plan.difference.count, 2);
    CHECK(plan.attach_true);
    CHECK(!plan.is_revision);
    CHECK(plan.editable);
    app_correction_plan_free(&plan);

    clauselist_init(&rule);  /* rule kustom dipakai pada mode editable */
    query_parse("suhu > 38", &rule, NULL);
    r = app_correct(app, "Demam", &rule);
    CHECK_INT_EQ(r.status, RDR_ACQ_EXCEPTION_ADDED);
    CHECK(app->dirty);
    CHECK_STR_EQ(app_conclusion(app), "Demam");  /* inferensi dijalankan ulang */
    clauselist_free(&rule);

    app->prefs.values[PREF_ACQ_CLAUSE_SELECTION] = PREF_CLAUSE_STRICT;  /* mode strict mengabaikan rule kustom */
    run_query(app, "suhu = 40; batuk");
    clauselist_init(&rule);
    query_parse("batuk", &rule, NULL);
    r = app_correct(app, "Flu", &rule);
    CHECK_INT_EQ(r.status, RDR_ACQ_EXCEPTION_ADDED);
    CHECK_INT_EQ(rdr_kb_find(app->kb, r.node_id)->rule.count, 2);
    clauselist_free(&rule);

    app->prefs.values[PREF_ACQ_REVISION_STRATEGY] = RDR_REVISION_REJECT;  /* strategi reject dari preferensi */
    run_query(app, "suhu = 40; batuk");
    r = app_correct(app, "Covid", NULL);
    CHECK_INT_EQ(r.status, RDR_ACQ_REJECTED);
    CHECK_INT_EQ(app->status_level, APP_STATUS_WARNING);
    app_destroy(app);
}

TEST(manual_edit_refreshes_result)
{
    App *app = app_create("Sehat");
    char err[256];
    int id = -1;

    CHECK(app_add_child(app, 0, RDR_SIDE_TRUE, "demam", "Flu", "demam", &id, err, sizeof(err)));
    CHECK_INT_EQ(id, 1);
    run_query(app, "demam");
    CHECK_STR_EQ(app_conclusion(app), "Flu");
    CHECK(app_edit_conclusion(app, 1, "Influenza", err, sizeof(err)));
    CHECK_STR_EQ(app_conclusion(app), "Influenza");
    CHECK(app_edit_rule(app, 1, "batuk", err, sizeof(err)));
    CHECK_STR_EQ(app_conclusion(app), "Sehat");
    CHECK(!app_edit_rule(app, 1, "x >", err, sizeof(err)));
    CHECK_INT_EQ(app->status_level, APP_STATUS_ERROR);
    CHECK(app_edit_rule(app, 1, "T", err, sizeof(err)));
    CHECK_STR_EQ(app_conclusion(app), "Influenza");
    CHECK(!app_edit_cornerstone(app, 1, "a; !a", err, sizeof(err)));
    CHECK(app_delete_node(app, 1, NULL, err, sizeof(err)));
    CHECK_STR_EQ(app_conclusion(app), "Sehat");
    app_destroy(app);
}

TEST(save_open_restores_view)
{
    App *app = app_create("Sehat");
    App *other = app_create("Lain");
    const char *path = TMP_DIR "/app_session.rdr.json";
    char *title;

    run_query(app, "demam");
    app_correct(app, "Flu", NULL);
    app_draft_add(app, "batuk");
    app->result_tab = SESSION_RESULT_TAB_TREE;
    app->prefs.values[PREF_UI_FONT_SIZE] = 20;
    CHECK(app_save_as(app, TMP_DIR "/app_session"));
    CHECK(!app->dirty);
    title = app_title(app);
    CHECK_STR_EQ(title, "RDR Expert System - app_session.rdr.json");
    free(title);

    CHECK(app_open(other, path));
    CHECK_INT_EQ(other->status_level, APP_STATUS_INFO);
    CHECK_INT_EQ(other->draft.count, 2);
    CHECK_INT_EQ(other->result_tab, SESSION_RESULT_TAB_TREE);
    CHECK_INT_EQ(prefs_get(&other->prefs, PREF_UI_FONT_SIZE), 20);
    CHECK(app_has_result(other));
    CHECK_STR_EQ(app_conclusion(other), "Flu");
    CHECK(!app_open(other, TMP_DIR "/missing.rdr.json"));
    CHECK_STR_EQ(app_conclusion(other), "Flu");  /* state lama dipertahankan */

    app_set_prefs(other, &app->prefs);  /* impor membuat dirty tanpa menulis file */
    other->dirty = 0;
    other->prefs.values[PREF_UI_FONT_SIZE] = 12;
    CHECK(app_import_prefs(other, path));
    CHECK(other->dirty);
    CHECK_INT_EQ(prefs_get(&other->prefs, PREF_UI_FONT_SIZE), 20);

    remove(path);
    app_destroy(app);
    app_destroy(other);
}

TEST(undo_redo)
{
    App *app = app_create("Sehat");
    const char *path = TMP_DIR "/undo.rdr.json";
    char err[256];

    CHECK(!app_can_undo(app));
    CHECK(!app_undo(app));

    /* Draft History */
    app_draft_add(app, "demam");
    app_draft_add(app, "batuk");
    app_draft_add(app, "batuk");  /* tidak berubah, tidak ada langkah baru */
    CHECK_INT_EQ(app->undo.count, 2);
    CHECK(app_undo(app));
    CHECK_INT_EQ(app->draft.count, 1);
    CHECK(app_can_redo(app));
    CHECK(app_redo(app));
    CHECK_INT_EQ(app->draft.count, 2);

    /* Knowledge History */
    app_start(app);
    CHECK_STR_EQ(app_conclusion(app), "Sehat");
    app->dirty = 0;
    app_correct(app, "Flu", NULL);
    CHECK_INT_EQ(app->kb->node_count, 2);
    CHECK_STR_EQ(app_conclusion(app), "Flu");
    CHECK(strstr(app->undo.items[app->undo.count - 1].label, "node #1") != NULL);
    app->dirty = 0;
    CHECK(app_undo(app));
    CHECK_INT_EQ(app->kb->node_count, 1);
    CHECK_STR_EQ(app_conclusion(app), "Sehat");  /* hasil inferensi ikut dipulihkan */
    CHECK(app->dirty);
    CHECK(app_redo(app));
    CHECK_INT_EQ(app->kb->node_count, 2);

    /* Failed Actions Leave No History */
    {
        size_t before = app->undo.count;
        CHECK(!app_edit_rule(app, 1, "x >", err, sizeof(err)));
        CHECK(!app_delete_node(app, 0, NULL, err, sizeof(err)));
        app_correct(app, "Flu", NULL);  /* tidak berubah */
        CHECK_INT_EQ(app->undo.count, before);
    }

    /* New Action Clears Redo */
    CHECK(app_delete_node(app, 1, NULL, err, sizeof(err)));
    CHECK(app_undo(app));
    CHECK(app_can_redo(app));
    app_draft_add(app, "ruam");
    CHECK(!app_can_redo(app));

    /* Preferences and Reset On Open */
    {
        Preferences p = app->prefs;
        p.values[PREF_UI_HELP_LANGUAGE] = PREF_HELP_EN;
        CHECK(app_set_prefs(app, &p));
        CHECK(app_undo(app));
        CHECK_INT_EQ(prefs_get(&app->prefs, PREF_UI_HELP_LANGUAGE), PREF_HELP_ID);
    }
    app->split_ratio = 0.6;
    CHECK(app_save_as(app, path));
    CHECK(app_open(app, path));
    CHECK(!app_can_undo(app));
    CHECK_DBL_EQ(app->split_ratio, 0.6);
    remove(path);
    app_destroy(app);
}

TEST(history_limit)
{
    App *app = app_create("X");
    char buf[32];
    int i;

    for (i = 0; i < APP_HISTORY_LIMIT + 10; i++) {
        sprintf(buf, "c%d", i);
        app_draft_add(app, buf);
    }
    CHECK_INT_EQ(app->undo.count, APP_HISTORY_LIMIT);
    for (i = 0; i < APP_HISTORY_LIMIT; i++) {
        CHECK(app_undo(app));
    }
    CHECK(!app_undo(app));
    CHECK_INT_EQ(app->draft.count, 10);
    app_destroy(app);
}

int main(void)
{
    RUN(lifecycle);
    RUN(draft_operations);
    RUN(reasoning_and_correction);
    RUN(manual_edit_refreshes_result);
    RUN(save_open_restores_view);
    RUN(undo_redo);
    RUN(history_limit);
    return TEST_REPORT();
}
