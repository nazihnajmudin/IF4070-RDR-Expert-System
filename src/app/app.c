/**
 * @file app.c
 * @brief Implementasi controller aplikasi.
 */

#include "rdr/app.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "rdr/clause.h"
#include "rdr/json.h"
#include "rdr/platform.h"
#include "rdr/query.h"

/* ===================================================================== */
/* Internal Helpers                                                      */
/* ===================================================================== */

static void touch(App *app)
{
    app->revision++;
}

void app_set_status(App *app, AppStatusLevel level, const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    vsnprintf(app->status, sizeof(app->status), fmt, args);
    va_end(args);
    app->status_level = level;
    strlist_clear(&app->details);
    touch(app);
}

/* Menjalankan ulang inferensi pada kueri terakhir setelah tree berubah. */
static void refresh_inference(App *app)
{
    ClauseList query;

    if (!app->inference.valid) {
        return;
    }
    clauselist_init(&query);
    clauselist_copy(&query, &app->inference.query);
    if (!rdr_infer(app->kb, &query, &app->inference, NULL)) {
        rdr_inference_free(&app->inference);
    }
    clauselist_free(&query);
}

static void mark_changed(App *app)
{
    app->dirty = 1;
    refresh_inference(app);
    touch(app);
}

/* ===================================================================== */
/* History                                                               */
/* ===================================================================== */

static char *snapshot(const App *app)
{
    RdrSession session;
    JsonValue *doc;
    char *text;

    rdr_session_init(&session);
    app_build_session(app, &session);
    doc = kb_to_json(app->kb, &app->prefs, &session);
    text = json_write(doc);
    json_free(doc);
    rdr_session_free(&session);
    return text;
}

static void history_drop_oldest(AppHistory *h)
{
    if (h->count == 0) {
        return;
    }
    h->bytes -= h->items[0].size;
    free(h->items[0].state);
    memmove(&h->items[0], &h->items[1], (h->count - 1) * sizeof(AppHistoryEntry));
    h->count--;
}

static void history_push(AppHistory *h, char *state, int kb_change, const char *label)
{
    AppHistoryEntry *e;

    if (h->count == h->cap) {
        h->cap = h->cap ? h->cap * 2 : 16;
        h->items = xrealloc(h->items, h->cap * sizeof(AppHistoryEntry));
    }
    e = &h->items[h->count++];
    e->state = state;
    e->size = strlen(state);
    e->kb_change = kb_change;
    str_copy(e->label, sizeof(e->label), label);
    h->bytes += e->size;
    while (h->count > 1 && (h->count > APP_HISTORY_LIMIT || h->bytes > APP_HISTORY_BYTES)) {
        history_drop_oldest(h);
    }
}

/* Mengambil entri terbaru. Pemanggil mengambil alih kepemilikan state. */
static AppHistoryEntry history_pop(AppHistory *h)
{
    AppHistoryEntry e = h->items[--h->count];
    h->bytes -= e.size;
    return e;
}

static void history_clear(AppHistory *h)
{
    while (h->count > 0) {
        AppHistoryEntry e = history_pop(h);
        free(e.state);
    }
}

static void history_free(AppHistory *h)
{
    history_clear(h);
    free(h->items);
    memset(h, 0, sizeof(*h));
}

/* Menyimpan snapshot sebelum aksi yang mengubah state. */
static void begin_change(App *app, int kb_change, const char *label)
{
    history_push(&app->undo, snapshot(app), kb_change, label);
    history_clear(&app->redo);
}

/* Membatalkan snapshot terakhir jika aksi ternyata gagal atau tidak mengubah apa pun. */
static void cancel_change(App *app)
{
    if (app->undo.count > 0) {
        AppHistoryEntry e = history_pop(&app->undo);
        free(e.state);
    }
}

/* Memulihkan KB, preferensi, draft, dan hasil inferensi dari snapshot. */
static int restore(App *app, const char *state)
{
    JsonValue *doc = json_parse(state, strlen(state), NULL, 0);
    RdrProject project;
    int ok;

    if (doc == NULL) {
        return 0;
    }
    rdr_project_init(&project);
    ok = kb_from_json(doc, &project, NULL, NULL, 0);
    json_free(doc);
    if (!ok) {
        return 0;
    }
    rdr_kb_destroy(app->kb);
    app->kb = project.kb;
    project.kb = NULL;
    app->prefs = project.prefs;
    strlist_copy(&app->draft, &project.session.query_draft);
    rdr_inference_free(&app->inference);
    if (project.session.has_last_query) {
        rdr_infer(app->kb, &project.session.last_query, &app->inference, NULL);
    }
    rdr_project_free(&project);
    return 1;
}

static int step_history(App *app, AppHistory *from, AppHistory *to, const char *verb)
{
    AppHistoryEntry e;

    if (from->count == 0) {
        app_set_status(app, APP_STATUS_INFO, "Nothing to %s.", verb);
        return 0;
    }
    e = history_pop(from);
    history_push(to, snapshot(app), e.kb_change, e.label);
    if (!restore(app, e.state)) {
        app_set_status(app, APP_STATUS_ERROR, "Cannot %s '%s'.", verb, e.label);
        free(e.state);
        return 0;
    }
    if (e.kb_change) {
        app->dirty = 1;
    }
    app_set_status(app, APP_STATUS_INFO, "%s: %s.", verb[0] == 'u' ? "Undid" : "Redid", e.label);
    free(e.state);
    return 1;
}

int app_can_undo(const App *app)
{
    return app->undo.count > 0;
}

int app_can_redo(const App *app)
{
    return app->redo.count > 0;
}

int app_undo(App *app)
{
    return step_history(app, &app->undo, &app->redo, "undo");
}

int app_redo(App *app)
{
    return step_history(app, &app->redo, &app->undo, "redo");
}

static RdrRevisionStrategy revision_strategy(const App *app)
{
    return (RdrRevisionStrategy)prefs_get(&app->prefs, PREF_ACQ_REVISION_STRATEGY);  /* urutan sama dengan enum */
}

/* ===================================================================== */
/* Lifecycle                                                             */
/* ===================================================================== */

App *app_create(const char *default_conclusion)
{
    App *app;
    RdrKb *kb = rdr_kb_create(default_conclusion);

    if (kb == NULL) {
        return NULL;
    }
    app = xcalloc(1, sizeof(App));
    app->kb = kb;
    prefs_init_defaults(&app->prefs);
    strlist_init(&app->draft);
    strlist_init(&app->details);
    rdr_inference_init(&app->inference);
    app->split_ratio = SESSION_DEFAULT_SPLIT;
    app_set_status(app, APP_STATUS_INFO, "New knowledge base with default conclusion '%s'.",
                   kb->root->conclusion);
    return app;
}

void app_destroy(App *app)
{
    if (app == NULL) {
        return;
    }
    rdr_kb_destroy(app->kb);
    free(app->file_path);
    strlist_free(&app->draft);
    strlist_free(&app->details);
    rdr_inference_free(&app->inference);
    history_free(&app->undo);
    history_free(&app->redo);
    free(app);
}

int app_new(App *app, const char *default_conclusion)
{
    RdrKb *kb = rdr_kb_create(default_conclusion);

    if (kb == NULL) {
        app_set_status(app, APP_STATUS_ERROR, "The default conclusion cannot be empty.");
        return 0;
    }
    rdr_kb_destroy(app->kb);
    app->kb = kb;
    free(app->file_path);
    app->file_path = NULL;
    app->dirty = 0;
    strlist_clear(&app->draft);
    rdr_inference_free(&app->inference);
    app->query_tab = SESSION_QUERY_TAB_CLAUSE;
    app->result_tab = SESSION_RESULT_TAB_RESULT;
    history_clear(&app->undo);
    history_clear(&app->redo);
    app_set_status(app, APP_STATUS_INFO, "New knowledge base with default conclusion '%s'.",
                   kb->root->conclusion);
    return 1;
}

void app_build_session(const App *app, RdrSession *out)
{
    rdr_session_free(out);
    strlist_copy(&out->query_draft, &app->draft);
    if (app->inference.valid) {
        out->has_last_query = 1;
        clauselist_copy(&out->last_query, &app->inference.query);
        out->last_evaluated = app->inference.last_evaluated;
        out->last_concluded = app->inference.last_concluded;
    }
    out->query_tab = app->query_tab;
    out->result_tab = app->result_tab;
    out->split = app->split_ratio;
}

int app_open(App *app, const char *path)
{
    RdrProject project;
    StrList warnings;
    char err[512];
    int mismatch = 0;
    size_t i;

    /* Project Loading */
    rdr_project_init(&project);
    strlist_init(&warnings);
    if (!kb_load_file(path, &project, &warnings, err, sizeof(err))) {
        app_set_status(app, APP_STATUS_ERROR, "Cannot open the project: %s.", err);
        strlist_free(&warnings);
        return 0;
    }

    /* State Replacement */
    rdr_kb_destroy(app->kb);
    app->kb = project.kb;
    project.kb = NULL;
    app->prefs = project.prefs;
    free(app->file_path);
    app->file_path = str_dup(path);
    app->dirty = 0;
    strlist_copy(&app->draft, &project.session.query_draft);
    app->query_tab = project.session.query_tab;
    app->result_tab = project.session.result_tab;
    app->split_ratio = project.session.split;
    history_clear(&app->undo);
    history_clear(&app->redo);
    rdr_inference_free(&app->inference);

    /* Result Restoration */
    if (project.session.has_last_query) {
        if (!rdr_infer(app->kb, &project.session.last_query, &app->inference, NULL)) {
            strlist_push(&warnings, "the saved last query has contradicting facts, the result was not restored");
        } else if (app->inference.last_concluded != project.session.last_concluded ||
                   app->inference.last_evaluated != project.session.last_evaluated) {
            mismatch = 1;
            strlist_push(&warnings, "the saved result differs from the current knowledge base, it was recomputed");
        }
    }

    if (warnings.count > 0) {
        app_set_status(app, APP_STATUS_WARNING, "Opened '%s' with %lu warning(s)%s.",
                       platform_path_basename(path), (unsigned long)warnings.count,
                       mismatch ? ", the result was recomputed" : "");
        for (i = 0; i < warnings.count; i++) {
            strlist_push(&app->details, warnings.items[i]);
        }
    } else {
        app_set_status(app, APP_STATUS_INFO, "Opened '%s' (%lu nodes).",
                       platform_path_basename(path), (unsigned long)app->kb->node_count);
    }
    strlist_free(&warnings);
    rdr_project_free(&project);
    return 1;
}

static int save_to(App *app, const char *path)
{
    RdrSession session;
    char err[512];
    int ok;

    rdr_session_init(&session);
    app_build_session(app, &session);
    ok = kb_save_file(path, app->kb, &app->prefs, &session, err, sizeof(err));
    rdr_session_free(&session);
    if (!ok) {
        app_set_status(app, APP_STATUS_ERROR, "Cannot save: %s.", err);
        return 0;
    }
    if (app->file_path != path) {
        free(app->file_path);
        app->file_path = str_dup(path);
    }
    app->dirty = 0;
    app_set_status(app, APP_STATUS_INFO, "Saved '%s'.", platform_path_basename(path));
    return 1;
}

int app_save(App *app)
{
    if (app->file_path == NULL) {
        app_set_status(app, APP_STATUS_ERROR, "The project has no file yet. Use Save As.");
        return 0;
    }
    return save_to(app, app->file_path);
}

int app_save_as(App *app, const char *path)
{
    char *full;
    int ok;

    if (path == NULL || str_is_blank(path)) {
        app_set_status(app, APP_STATUS_ERROR, "The file name cannot be empty.");
        return 0;
    }
    full = platform_path_has_ext(path, ".json") ? str_dup(path) : str_printf("%s%s", path, APP_FILE_EXT);
    ok = save_to(app, full);
    free(full);
    return ok;
}

char *app_title(const App *app)
{
    const char *name = app->file_path ? platform_path_basename(app->file_path) : "Untitled";

    return str_printf("%s - %s%s", APP_NAME, name, app->dirty ? "*" : "");
}

/* ===================================================================== */
/* Query Draft                                                           */
/* ===================================================================== */

size_t app_draft_add(App *app, const char *text)
{
    StrList pieces;
    size_t added = 0;
    size_t i;

    strlist_init(&pieces);
    query_split(text, &pieces);
    for (i = 0; i < pieces.count; i++) {
        if (strlist_index_of(&app->draft, pieces.items[i]) < 0) {
            if (added == 0) {
                begin_change(app, 0, "add clauses");
            }
            strlist_push(&app->draft, pieces.items[i]);
            added++;
        }
    }
    strlist_free(&pieces);
    touch(app);
    return added;
}

void app_draft_set(App *app, size_t index, const char *text)
{
    char *trimmed;

    if (index >= app->draft.count) {
        return;
    }
    trimmed = str_trim_dup(text ? text : "");
    if (strcmp(trimmed, app->draft.items[index]) == 0) {
        free(trimmed);
        return;
    }
    begin_change(app, 0, trimmed[0] == '\0' ? "remove clause" : "edit clause");
    if (trimmed[0] == '\0') {
        strlist_remove_at(&app->draft, index);
    } else {
        strlist_set(&app->draft, index, trimmed);
    }
    free(trimmed);
    touch(app);
}

void app_draft_remove(App *app, size_t index)
{
    if (index >= app->draft.count) {
        return;
    }
    begin_change(app, 0, "remove clause");
    strlist_remove_at(&app->draft, index);
    touch(app);
}

void app_draft_clear(App *app)
{
    if (app->draft.count > 0) {
        begin_change(app, 0, "clear query");
    }
    strlist_clear(&app->draft);
    touch(app);
}

void app_draft_set_text(App *app, const char *text)
{
    StrList pieces;
    size_t i;
    int same;

    strlist_init(&pieces);
    query_split(text, &pieces);
    same = pieces.count == app->draft.count;
    for (i = 0; same && i < pieces.count; i++) {
        same = strcmp(pieces.items[i], app->draft.items[i]) == 0;
    }
    if (!same) {
        begin_change(app, 0, "edit query");
        strlist_copy(&app->draft, &pieces);
    }
    strlist_free(&pieces);
    touch(app);
}

char *app_draft_text(const App *app)
{
    return strlist_join(&app->draft, QUERY_SEPARATOR);
}

int app_draft_item_valid(const App *app, size_t index, char *err, size_t errlen)
{
    Clause c;

    if (index >= app->draft.count) {
        return 0;
    }
    if (!clause_parse(app->draft.items[index], &c, err, errlen)) {
        return 0;
    }
    clause_free(&c);
    return 1;
}

/* ===================================================================== */
/* Reasoning                                                             */
/* ===================================================================== */

int app_start(App *app)
{
    ClauseList query;
    StrList errors;
    size_t i;
    int ok = 0;

    clauselist_init(&query);
    strlist_init(&errors);

    /* Draft Parsing */
    if (query_parse_list(&app->draft, &query, &errors) > 0) {
        rdr_inference_free(&app->inference);  /* hasil lama tidak lagi mewakili draft */
        app_set_status(app, APP_STATUS_ERROR, "The query has %lu invalid clause(s).",
                       (unsigned long)errors.count);
    } else if (!rdr_infer(app->kb, &query, &app->inference, &errors)) {
        /* Contradiction Report */
        app_set_status(app, APP_STATUS_ERROR, "The query contains contradicting facts.");
    } else {
        app_set_status(app, APP_STATUS_INFO, "Conclusion: %s", app_conclusion(app));
        ok = 1;
    }
    for (i = 0; i < errors.count; i++) {
        strlist_push(&app->details, errors.items[i]);
    }

    clauselist_free(&query);
    strlist_free(&errors);
    return ok;
}

void app_clear(App *app)
{
    if (app->draft.count > 0 || app->inference.valid) {
        begin_change(app, 0, "clear query");
    }
    strlist_clear(&app->draft);
    rdr_inference_free(&app->inference);
    app_set_status(app, APP_STATUS_INFO, "Query and result cleared.");
}

int app_has_result(const App *app)
{
    return app->inference.valid;
}

const char *app_conclusion(const App *app)
{
    return rdr_inference_conclusion(app->kb, &app->inference);
}

/* ===================================================================== */
/* Knowledge Acquisition                                                 */
/* ===================================================================== */

int app_correction_plan(const App *app, AppCorrectionPlan *plan)
{
    memset(plan, 0, sizeof(*plan));
    clauselist_init(&plan->difference);
    plan->concluded_id = -1;
    plan->evaluated_id = -1;
    plan->strategy = revision_strategy(app);
    plan->editable = prefs_get(&app->prefs, PREF_ACQ_CLAUSE_SELECTION) == PREF_CLAUSE_EDITABLE;
    if (!rdr_inference_is_current(app->kb, &app->inference)) {
        return 0;
    }
    plan->available = 1;
    plan->conclusion = app_conclusion(app);
    plan->concluded_id = app->inference.last_concluded;
    plan->evaluated_id = app->inference.last_evaluated;
    plan->attach_true = plan->concluded_id == plan->evaluated_id;
    rdr_inference_difference(app->kb, &app->inference, &plan->difference);
    plan->is_revision = plan->difference.count == 0;
    return 1;
}

void app_correction_plan_free(AppCorrectionPlan *plan)
{
    clauselist_free(&plan->difference);
    memset(plan, 0, sizeof(*plan));
}

RdrAcqResult app_correct(App *app, const char *conclusion, const ClauseList *custom_rule)
{
    RdrAcqResult r;
    int editable = prefs_get(&app->prefs, PREF_ACQ_CLAUSE_SELECTION) == PREF_CLAUSE_EDITABLE;

    begin_change(app, 1, "correction");
    r = rdr_correct(app->kb, &app->inference, conclusion, editable ? custom_rule : NULL,
                    revision_strategy(app));
    if (r.status != RDR_ACQ_EXCEPTION_ADDED && r.status != RDR_ACQ_REVISION_ADDED &&
        r.status != RDR_ACQ_CONCLUSION_EDITED) {
        cancel_change(app);
    } else if (app->undo.count > 0) {
        str_set_error(app->undo.items[app->undo.count - 1].label, sizeof(app->undo.items[0].label),
                      r.status == RDR_ACQ_EXCEPTION_ADDED ? "add exception node #%d"
                      : r.status == RDR_ACQ_REVISION_ADDED ? "add revision node #%d"
                                                           : "edit conclusion of node #%d",
                      r.node_id);
    }
    switch (r.status) {
    case RDR_ACQ_EXCEPTION_ADDED:
    case RDR_ACQ_REVISION_ADDED:
    case RDR_ACQ_CONCLUSION_EDITED:
        mark_changed(app);
        app_set_status(app, APP_STATUS_INFO, "%s", r.message);
        break;
    case RDR_ACQ_UNCHANGED:
        app_set_status(app, APP_STATUS_INFO, "%s", r.message);
        break;
    case RDR_ACQ_REJECTED:
        app_set_status(app, APP_STATUS_WARNING, "%s", r.message);
        break;
    case RDR_ACQ_ERROR:
        app_set_status(app, APP_STATUS_ERROR, "%s", r.message);
        break;
    }
    return r;
}

/* ===================================================================== */
/* Manual Editing                                                        */
/* ===================================================================== */

/* Mem-parse teks menjadi ClauseList. rule_mode mengizinkan kata kunci T. */
static int parse_clauses(const char *text, int rule_mode, ClauseList *out, char *err, size_t errlen)
{
    StrList errors;
    size_t bad;

    strlist_init(&errors);
    bad = rule_mode ? query_parse_rule(text ? text : "", out, &errors)
                    : query_parse(text ? text : "", out, &errors);
    if (bad > 0) {
        str_set_error(err, errlen, "%s", errors.items[0]);
    }
    strlist_free(&errors);
    return bad == 0;
}

static int finish_edit(App *app, int ok, const char *what, int id, char *err)
{
    if (ok && app->undo.count > 0) {
        str_set_error(app->undo.items[app->undo.count - 1].label, sizeof(app->undo.items[0].label),
                      "edit %s of node #%d", what, id);
    } else if (!ok) {
        cancel_change(app);
    }
    if (ok) {
        mark_changed(app);
        app_set_status(app, APP_STATUS_INFO, "Updated the %s of node #%d.", what, id);
    } else {
        app_set_status(app, APP_STATUS_ERROR, "Cannot update node #%d: %s.", id, err);
    }
    return ok;
}

int app_edit_rule(App *app, int id, const char *rule_text, char *err, size_t errlen)
{
    ClauseList rule;
    int ok;

    clauselist_init(&rule);
    begin_change(app, 1, "edit rule");
    ok = parse_clauses(rule_text, 1, &rule, err, errlen) &&
         rdr_node_set_rule(app->kb, id, &rule, err, errlen);
    clauselist_free(&rule);
    return finish_edit(app, ok, "rule", id, err);
}

int app_edit_conclusion(App *app, int id, const char *conclusion, char *err, size_t errlen)
{
    int ok;

    begin_change(app, 1, "edit conclusion");
    ok = rdr_node_set_conclusion(app->kb, id, conclusion, err, errlen);
    return finish_edit(app, ok, "conclusion", id, err);
}

int app_edit_cornerstone(App *app, int id, const char *case_text, char *err, size_t errlen)
{
    ClauseList cs;
    int ok;

    clauselist_init(&cs);
    begin_change(app, 1, "edit cornerstone");
    ok = parse_clauses(case_text, 0, &cs, err, errlen) &&
         rdr_node_set_cornerstone(app->kb, id, &cs, err, errlen);
    clauselist_free(&cs);
    return finish_edit(app, ok, "cornerstone", id, err);
}

int app_add_child(App *app, int parent_id, RdrSide side, const char *rule_text,
                  const char *conclusion, const char *case_text, int *new_id,
                  char *err, size_t errlen)
{
    ClauseList rule;
    ClauseList cs;
    int id = -1;
    int ok;

    clauselist_init(&rule);
    clauselist_init(&cs);
    begin_change(app, 1, "add node");
    ok = parse_clauses(rule_text, 1, &rule, err, errlen) &&
         parse_clauses(case_text, 0, &cs, err, errlen) &&
         rdr_node_add_child(app->kb, parent_id, side, &rule, conclusion, &cs, &id, err, errlen);
    clauselist_free(&rule);
    clauselist_free(&cs);
    if (!ok) {
        cancel_change(app);
    }
    if (ok) {
        mark_changed(app);
        app_set_status(app, APP_STATUS_INFO, "Added node #%d as the %s child of node #%d.",
                       id, side == RDR_SIDE_TRUE ? "true" : "false", parent_id);
        if (new_id != NULL) {
            *new_id = id;
        }
    } else {
        app_set_status(app, APP_STATUS_ERROR, "Cannot add a child to node #%d: %s.", parent_id, err);
    }
    return ok;
}

int app_delete_node(App *app, int id, size_t *removed, char *err, size_t errlen)
{
    size_t count = 0;
    int ok;

    begin_change(app, 1, "delete node");
    ok = rdr_node_delete(app->kb, id, &count, err, errlen);
    if (!ok) {
        cancel_change(app);
    } else {
        str_set_error(app->undo.items[app->undo.count - 1].label, sizeof(app->undo.items[0].label),
                      "delete node #%d", id);
    }
    if (ok) {
        mark_changed(app);
        app_set_status(app, APP_STATUS_INFO, "Deleted node #%d and %lu descendant(s).",
                       id, (unsigned long)(count - 1));
        if (removed != NULL) {
            *removed = count;
        }
    } else {
        app_set_status(app, APP_STATUS_ERROR, "Cannot delete node #%d: %s.", id, err);
    }
    return ok;
}

void app_validate(App *app, RdrValidation *out)
{
    size_t i;

    rdr_validate(app->kb, out);
    if (out->errors > 0) {
        app_set_status(app, APP_STATUS_WARNING, "Validation found %lu problem(s) in %lu node(s).",
                       (unsigned long)out->errors, (unsigned long)out->checked);
    } else {
        app_set_status(app, APP_STATUS_INFO, "Validation passed for %lu node(s) with %lu note(s).",
                       (unsigned long)out->checked, (unsigned long)out->count);
    }
    for (i = 0; i < out->count; i++) {
        strlist_push(&app->details, out->items[i].message);
    }
}

/* ===================================================================== */
/* Import and Preferences                                                */
/* ===================================================================== */

int app_import_csv(App *app, const char *path, CsvImportReport *report)
{
    char err[512];
    char *summary;
    unsigned long before = app->kb->stamp;

    begin_change(app, 1, "insert CSV");
    if (!kb_csv_import_file(app->kb, path, revision_strategy(app), report, err, sizeof(err))) {
        cancel_change(app);
        app_set_status(app, APP_STATUS_ERROR, "Cannot import the CSV: %s.", err);
        return 0;
    }
    if (app->kb->stamp != before) {
        mark_changed(app);
    } else {
        cancel_change(app);
    }
    summary = csv_report_summary(report);
    app_set_status(app, report->failed > 0 ? APP_STATUS_WARNING : APP_STATUS_INFO,
                   "Imported '%s': %s.", platform_path_basename(path), summary);
    free(summary);
    return 1;
}

int app_import_prefs(App *app, const char *path)
{
    StrList warnings;
    char err[512];
    size_t changed = 0;
    size_t i;

    strlist_init(&warnings);
    begin_change(app, 1, "import preferences");
    if (!kb_import_prefs_file(path, &app->prefs, &changed, &warnings, err, sizeof(err))) {
        cancel_change(app);
        app_set_status(app, APP_STATUS_ERROR, "Cannot import preferences: %s.", err);
        strlist_free(&warnings);
        return 0;
    }
    if (changed > 0) {
        app->dirty = 1;
    } else {
        cancel_change(app);
    }
    app_set_status(app, warnings.count ? APP_STATUS_WARNING : APP_STATUS_INFO,
                   "Imported preferences from '%s': %lu value(s) changed. Save the project to keep them.",
                   platform_path_basename(path), (unsigned long)changed);
    for (i = 0; i < warnings.count; i++) {
        strlist_push(&app->details, warnings.items[i]);
    }
    strlist_free(&warnings);
    return 1;
}

int app_set_prefs(App *app, const Preferences *prefs)
{
    if (prefs_equal(&app->prefs, prefs)) {
        return 0;
    }
    begin_change(app, 1, "change preferences");
    app->prefs = *prefs;
    app->dirty = 1;
    app_set_status(app, APP_STATUS_INFO, "Preferences updated. Save the project to keep them.");
    return 1;
}
