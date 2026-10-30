/**
 * @file kb_io.c
 * @brief Implementasi penyimpanan dan pemuatan project RDR dalam JSON.
 */

#include "rdr/kb_io.h"

#include <stdlib.h>
#include <string.h>

#include "rdr/factbase.h"
#include "rdr/platform.h"
#include "rdr/query.h"

static const char *const QUERY_TAB_NAMES[] = {"clause", "query"};
static const char *const RESULT_TAB_NAMES[] = {"result", "tree"};

/* ===================================================================== */
/* Session and Project                                                   */
/* ===================================================================== */

void rdr_session_init(RdrSession *s)
{
    strlist_init(&s->query_draft);
    clauselist_init(&s->last_query);
    s->has_last_query = 0;
    s->last_evaluated = -1;
    s->last_concluded = -1;
    s->query_tab = SESSION_QUERY_TAB_CLAUSE;
    s->result_tab = SESSION_RESULT_TAB_RESULT;
    s->split = SESSION_DEFAULT_SPLIT;
}

void rdr_session_free(RdrSession *s)
{
    strlist_free(&s->query_draft);
    clauselist_free(&s->last_query);
    rdr_session_init(s);
}

void rdr_project_init(RdrProject *p)
{
    p->kb = NULL;
    prefs_init_defaults(&p->prefs);
    rdr_session_init(&p->session);
}

void rdr_project_free(RdrProject *p)
{
    rdr_kb_destroy(p->kb);
    rdr_session_free(&p->session);
    rdr_project_init(p);
}

/* ===================================================================== */
/* Serialization                                                         */
/* ===================================================================== */

static JsonValue *clauses_to_json(const ClauseList *list)
{
    JsonValue *arr = json_new_array();
    size_t i;

    for (i = 0; i < list->count; i++) {
        char *s = clause_to_string(&list->items[i]);
        json_array_push(arr, json_new_string(s));
        free(s);
    }
    return arr;
}

static JsonValue *strings_to_json(const StrList *list)
{
    JsonValue *arr = json_new_array();
    size_t i;

    for (i = 0; i < list->count; i++) {
        json_array_push(arr, json_new_string(list->items[i]));
    }
    return arr;
}

/* Mengisi object node. Object anak dibuat kosong dan dikembalikan lewat parameter. */
static void fill_node(JsonValue *obj, const RdrNode *n, JsonValue **true_obj, JsonValue **false_obj)
{
    json_object_set(obj, "id", json_new_number(n->id));
    json_object_set(obj, "rule", clauses_to_json(&n->rule));
    json_object_set(obj, "conclusion", json_new_string(n->conclusion));
    json_object_set(obj, "cornerstone", clauses_to_json(&n->cornerstone));
    if (n->revises != RDR_NO_REVISION) {
        json_object_set(obj, "revises", json_new_number(n->revises));
    }
    *true_obj = n->true_child ? json_new_object() : NULL;
    *false_obj = n->false_child ? json_new_object() : NULL;
    json_object_set(obj, "true", *true_obj ? *true_obj : json_new_null());
    json_object_set(obj, "false", *false_obj ? *false_obj : json_new_null());
}

static JsonValue *tree_to_json(const RdrKb *kb)
{
    typedef struct {
        const RdrNode *node;
        JsonValue *obj;
    } Frame;
    Frame *stack;
    size_t sp = 0;
    size_t cap = 64;
    JsonValue *root = json_new_object();

    stack = xmalloc(cap * sizeof(Frame));
    stack[sp].node = kb->root;
    stack[sp].obj = root;
    sp++;
    while (sp > 0) {
        Frame f = stack[--sp];
        JsonValue *t;
        JsonValue *fl;
        fill_node(f.obj, f.node, &t, &fl);
        if (sp + 2 > cap) {
            cap *= 2;
            stack = xrealloc(stack, cap * sizeof(Frame));
        }
        if (fl != NULL) {
            stack[sp].node = f.node->false_child;
            stack[sp].obj = fl;
            sp++;
        }
        if (t != NULL) {
            stack[sp].node = f.node->true_child;
            stack[sp].obj = t;
            sp++;
        }
    }
    free(stack);
    return root;
}

static JsonValue *session_to_json(const RdrSession *s)
{
    JsonValue *obj = json_new_object();
    JsonValue *view = json_new_object();
    int qtab = (s->query_tab == SESSION_QUERY_TAB_TEXT) ? 1 : 0;
    int rtab = (s->result_tab == SESSION_RESULT_TAB_TREE) ? 1 : 0;

    json_object_set(obj, "query_draft", strings_to_json(&s->query_draft));
    if (s->has_last_query) {
        json_object_set(obj, "last_query", clauses_to_json(&s->last_query));
        json_object_set(obj, "last_evaluated", json_new_number(s->last_evaluated));
        json_object_set(obj, "last_concluded", json_new_number(s->last_concluded));
    } else {
        json_object_set(obj, "last_query", json_new_null());
        json_object_set(obj, "last_evaluated", json_new_null());
        json_object_set(obj, "last_concluded", json_new_null());
    }
    json_object_set(view, "query_tab", json_new_string(QUERY_TAB_NAMES[qtab]));
    json_object_set(view, "result_tab", json_new_string(RESULT_TAB_NAMES[rtab]));
    json_object_set(view, "split", json_new_number((double)(int)(s->split * 1000.0 + 0.5) / 1000.0));
    json_object_set(obj, "view", view);
    return obj;
}

JsonValue *kb_to_json(const RdrKb *kb, const Preferences *prefs, const RdrSession *session)
{
    JsonValue *doc = json_new_object();

    json_object_set(doc, "format", json_new_string(KB_FORMAT_NAME));
    json_object_set(doc, "version", json_new_number(KB_FORMAT_VERSION));
    json_object_set(doc, "next_id", json_new_number(kb->next_id));
    json_object_set(doc, "root", tree_to_json(kb));
    if (prefs != NULL) {
        json_object_set(doc, "preferences", prefs_to_json(prefs));
    }
    if (session != NULL) {
        json_object_set(doc, "session", session_to_json(session));
    }
    return doc;
}

/* ===================================================================== */
/* Deserialization Helpers                                               */
/* ===================================================================== */

/* Membaca array string klausa. Mengembalikan 0 dan mengisi err jika gagal. */
static int clauses_from_json(const JsonValue *arr, ClauseList *out, const char *what, int node_id,
                             char *err, size_t errlen)
{
    StrList pieces;
    StrList errors;
    size_t i;
    int ok = 1;

    clauselist_clear(out);
    if (arr == NULL || arr->type != JSON_ARRAY) {
        str_set_error(err, errlen, "node #%d: '%s' must be an array of clause strings", node_id, what);
        return 0;
    }
    strlist_init(&pieces);
    strlist_init(&errors);
    for (i = 0; i < arr->count; i++) {
        if (arr->items[i]->type != JSON_STRING) {
            str_set_error(err, errlen, "node #%d: '%s' item %lu must be a string",
                          node_id, what, (unsigned long)(i + 1));
            ok = 0;
            break;
        }
        strlist_push(&pieces, arr->items[i]->string);
    }
    if (ok && query_parse_list(&pieces, out, &errors) > 0) {
        str_set_error(err, errlen, "node #%d: '%s' %s", node_id, what, errors.items[0]);
        ok = 0;
    }
    strlist_free(&pieces);
    strlist_free(&errors);
    return ok;
}

static int read_node_fields(const JsonValue *obj, int *id, ClauseList *rule, const char **conclusion,
                            ClauseList *cornerstone, int *revises, char *err, size_t errlen)
{
    const JsonValue *v;

    /* Node Identity */
    if (obj == NULL || obj->type != JSON_OBJECT) {
        str_set_error(err, errlen, "line %d: a node must be a JSON object", obj ? obj->line : 0);
        return 0;
    }
    if (!json_get_int(json_object_get(obj, "id"), id) || *id < 0) {
        str_set_error(err, errlen, "line %d: node 'id' must be a non-negative integer", obj->line);
        return 0;
    }

    /* Node Content */
    v = json_object_get(obj, "conclusion");
    if (v == NULL || v->type != JSON_STRING || str_is_blank(v->string)) {
        str_set_error(err, errlen, "node #%d: 'conclusion' must be a non-empty string", *id);
        return 0;
    }
    *conclusion = v->string;
    if (!clauses_from_json(json_object_get(obj, "rule"), rule, "rule", *id, err, errlen)) {
        return 0;
    }
    v = json_object_get(obj, "cornerstone");
    if (v == NULL) {
        clauselist_clear(cornerstone);
    } else if (!clauses_from_json(v, cornerstone, "cornerstone", *id, err, errlen)) {
        return 0;
    }
    v = json_object_get(obj, "revises");
    *revises = RDR_NO_REVISION;
    if (v != NULL && v->type != JSON_NULL && (!json_get_int(v, revises) || *revises < 0)) {
        str_set_error(err, errlen, "node #%d: 'revises' must be a node id or null", *id);
        return 0;
    }

    /* Child Slots */
    v = json_object_get(obj, "true");
    if (v != NULL && v->type != JSON_NULL && v->type != JSON_OBJECT) {
        str_set_error(err, errlen, "node #%d: 'true' must be a node object or null", *id);
        return 0;
    }
    v = json_object_get(obj, "false");
    if (v != NULL && v->type != JSON_NULL && v->type != JSON_OBJECT) {
        str_set_error(err, errlen, "node #%d: 'false' must be a node object or null", *id);
        return 0;
    }
    return 1;
}

static const JsonValue *child_object(const JsonValue *obj, const char *key)
{
    const JsonValue *v = json_object_get(obj, key);
    return (v != NULL && v->type == JSON_OBJECT) ? v : NULL;
}

/* ===================================================================== */
/* Tree Deserialization                                                  */
/* ===================================================================== */

/* Menghapus tanda revises yang menunjuk node yang tidak ada. */
static void drop_dangling_revisions(RdrKb *kb, StrList *warnings)
{
    size_t i;

    for (i = 0; i < kb->index_cap; i++) {
        RdrNode *n = kb->index[i];
        if (n == NULL || n->revises == RDR_NO_REVISION || rdr_kb_find(kb, n->revises) != NULL) {
            continue;
        }
        if (warnings != NULL) {
            strlist_pushf(warnings, "node #%d revises missing node #%d, the mark was removed",
                          n->id, n->revises);
        }
        n->revises = RDR_NO_REVISION;
    }
}

static RdrKb *tree_from_json(const JsonValue *root_obj, StrList *warnings, char *err, size_t errlen)
{
    typedef struct {
        const JsonValue *obj;
        RdrNode *parent;
        RdrSide side;
    } Frame;
    Frame *stack = NULL;
    size_t sp = 0;
    size_t cap = 64;
    ClauseList rule;
    ClauseList cornerstone;
    const char *conclusion = NULL;
    int id = 0;
    int revises = RDR_NO_REVISION;
    RdrKb *kb = NULL;

    clauselist_init(&rule);
    clauselist_init(&cornerstone);

    /* Root Node */
    if (!read_node_fields(root_obj, &id, &rule, &conclusion, &cornerstone, &revises, err, errlen)) {
        goto fail;
    }
    if (id != 0) {
        str_set_error(err, errlen, "the root node must have id 0");
        goto fail;
    }
    if (rule.count > 0) {
        str_set_error(err, errlen, "the root rule must be empty (T)");
        goto fail;
    }
    if (cornerstone.count > 0 && warnings != NULL) {
        strlist_push(warnings, "the root cornerstone must be empty, it was ignored");
    }
    if (child_object(root_obj, "false") != NULL) {
        str_set_error(err, errlen, "the false slot of the root must be null because the root is always true");
        goto fail;
    }
    kb = rdr_kb_create(conclusion);

    /* Descendant Nodes */
    stack = xmalloc(cap * sizeof(Frame));
    if (child_object(root_obj, "true") != NULL) {
        stack[sp].obj = child_object(root_obj, "true");
        stack[sp].parent = kb->root;
        stack[sp].side = RDR_SIDE_TRUE;
        sp++;
    }
    while (sp > 0) {
        Frame f = stack[--sp];
        RdrNode *n;
        char ierr[256];

        if (!read_node_fields(f.obj, &id, &rule, &conclusion, &cornerstone, &revises, err, errlen)) {
            goto fail;
        }
        if (id == 0) {
            str_set_error(err, errlen, "line %d: id 0 is reserved for the root node", f.obj->line);
            goto fail;
        }
        if (cornerstone.count > 0 && !factbase_check(&cornerstone, NULL)) {
            str_set_error(err, errlen, "node #%d: the cornerstone contains contradicting facts", id);
            goto fail;
        }
        n = rdr_kb_insert(kb, f.parent, f.side, id, &rule, conclusion, &cornerstone, revises,
                          ierr, sizeof(ierr));
        if (n == NULL) {
            str_set_error(err, errlen, "line %d: %s", f.obj->line, ierr);
            goto fail;
        }
        if (sp + 2 > cap) {
            cap *= 2;
            stack = xrealloc(stack, cap * sizeof(Frame));
        }
        if (child_object(f.obj, "false") != NULL) {
            stack[sp].obj = child_object(f.obj, "false");
            stack[sp].parent = n;
            stack[sp].side = RDR_SIDE_FALSE;
            sp++;
        }
        if (child_object(f.obj, "true") != NULL) {
            stack[sp].obj = child_object(f.obj, "true");
            stack[sp].parent = n;
            stack[sp].side = RDR_SIDE_TRUE;
            sp++;
        }
    }

    /* Revision References */
    drop_dangling_revisions(kb, warnings);

    free(stack);
    clauselist_free(&rule);
    clauselist_free(&cornerstone);
    return kb;

fail:
    free(stack);
    clauselist_free(&rule);
    clauselist_free(&cornerstone);
    rdr_kb_destroy(kb);
    return NULL;
}

/* ===================================================================== */
/* Session Deserialization                                               */
/* ===================================================================== */

static int tab_index(const JsonValue *v, const char *const *names, int fallback)
{
    if (v != NULL && v->type == JSON_STRING) {
        if (strcmp(v->string, names[0]) == 0) {
            return 0;
        }
        if (strcmp(v->string, names[1]) == 0) {
            return 1;
        }
    }
    return fallback;
}

static void session_from_json(const JsonValue *obj, RdrSession *s, StrList *warnings)
{
    const JsonValue *v;
    size_t i;

    rdr_session_free(s);
    if (obj == NULL || obj->type == JSON_NULL) {
        return;
    }
    if (obj->type != JSON_OBJECT) {
        if (warnings != NULL) {
            strlist_push(warnings, "'session' must be an object, it was ignored");
        }
        return;
    }

    /* Query Draft */
    v = json_object_get(obj, "query_draft");
    if (v != NULL && v->type == JSON_ARRAY) {
        for (i = 0; i < v->count; i++) {
            if (v->items[i]->type == JSON_STRING) {
                strlist_push(&s->query_draft, v->items[i]->string);
            }
        }
    }

    /* Last Query */
    v = json_object_get(obj, "last_query");
    if (v != NULL && v->type == JSON_ARRAY) {
        StrList pieces;
        size_t bad;
        size_t strings;
        strlist_init(&pieces);
        for (i = 0; i < v->count; i++) {
            if (v->items[i]->type == JSON_STRING) {
                strlist_push(&pieces, v->items[i]->string);
            }
        }
        strings = pieces.count;
        bad = query_parse_list(&pieces, &s->last_query, NULL);
        strlist_free(&pieces);
        if (bad > 0 || strings != v->count) {
            clauselist_clear(&s->last_query);
            if (warnings != NULL) {
                strlist_push(warnings, "the saved last query is invalid, the last result was not restored");
            }
        } else {
            s->has_last_query = 1;
            if (!json_get_int(json_object_get(obj, "last_evaluated"), &s->last_evaluated)) {
                s->last_evaluated = -1;
            }
            if (!json_get_int(json_object_get(obj, "last_concluded"), &s->last_concluded)) {
                s->last_concluded = -1;
            }
        }
    }

    /* View State */
    v = json_object_get(obj, "view");
    s->query_tab = tab_index(json_object_get(v, "query_tab"), QUERY_TAB_NAMES, SESSION_QUERY_TAB_CLAUSE);
    s->result_tab = tab_index(json_object_get(v, "result_tab"), RESULT_TAB_NAMES, SESSION_RESULT_TAB_RESULT);
    {
        const JsonValue *split = json_object_get(v, "split");
        if (split != NULL && split->type == JSON_NUMBER && split->number >= SESSION_MIN_SPLIT &&
            split->number <= SESSION_MAX_SPLIT) {
            s->split = split->number;
        }
    }
}

/* ===================================================================== */
/* Project Deserialization                                               */
/* ===================================================================== */

static int check_header(const JsonValue *doc, char *err, size_t errlen)
{
    const JsonValue *v;
    int version;

    if (doc == NULL || doc->type != JSON_OBJECT) {
        str_set_error(err, errlen, "the file must contain a JSON object");
        return 0;
    }
    v = json_object_get(doc, "format");
    if (v == NULL || v->type != JSON_STRING || strcmp(v->string, KB_FORMAT_NAME) != 0) {
        str_set_error(err, errlen, "not an RDR project file (\"format\" must be \"%s\")", KB_FORMAT_NAME);
        return 0;
    }
    if (!json_get_int(json_object_get(doc, "version"), &version) || version != KB_FORMAT_VERSION) {
        str_set_error(err, errlen, "unsupported file version (expected %d)", KB_FORMAT_VERSION);
        return 0;
    }
    return 1;
}

int kb_from_json(const JsonValue *doc, RdrProject *out, StrList *warnings, char *err, size_t errlen)
{
    const JsonValue *v;
    int next_id;

    rdr_project_free(out);
    if (!check_header(doc, err, errlen)) {
        return 0;
    }

    /* Tree */
    v = json_object_get(doc, "root");
    if (v == NULL || v->type != JSON_OBJECT) {
        str_set_error(err, errlen, "missing 'root' node object");
        return 0;
    }
    out->kb = tree_from_json(v, warnings, err, errlen);
    if (out->kb == NULL) {
        return 0;
    }
    if (json_get_int(json_object_get(doc, "next_id"), &next_id) && next_id > out->kb->next_id) {
        out->kb->next_id = next_id;
    }

    /* Preferences and Session */
    v = json_object_get(doc, "preferences");
    if (v != NULL) {
        prefs_from_json(&out->prefs, v, warnings);
    }
    session_from_json(json_object_get(doc, "session"), &out->session, warnings);
    out->kb->stamp = 0;
    return 1;
}

/* ===================================================================== */
/* File Operations                                                       */
/* ===================================================================== */

int kb_save_file(const char *path, const RdrKb *kb, const Preferences *prefs,
                 const RdrSession *session, char *err, size_t errlen)
{
    JsonValue *doc = kb_to_json(kb, prefs, session);
    char *text = json_write(doc);
    int ok = platform_write_file(path, text, strlen(text), err, errlen);

    free(text);
    json_free(doc);
    return ok;
}

static JsonValue *load_document(const char *path, char *err, size_t errlen)
{
    char *text = NULL;
    size_t len = 0;
    char perr[256];
    JsonValue *doc;

    if (!platform_read_file(path, &text, &len, err, errlen)) {
        return NULL;
    }
    doc = json_parse(text, len, perr, sizeof(perr));
    free(text);
    if (doc == NULL) {
        str_set_error(err, errlen, "invalid JSON in '%s' (%s)", platform_path_basename(path), perr);
    }
    return doc;
}

int kb_load_file(const char *path, RdrProject *out, StrList *warnings, char *err, size_t errlen)
{
    JsonValue *doc = load_document(path, err, errlen);
    int ok;

    if (doc == NULL) {
        return 0;
    }
    ok = kb_from_json(doc, out, warnings, err, errlen);
    json_free(doc);
    return ok;
}

int kb_import_prefs_file(const char *path, Preferences *prefs, size_t *changed,
                         StrList *warnings, char *err, size_t errlen)
{
    JsonValue *doc = load_document(path, err, errlen);
    const JsonValue *v;
    size_t n;

    if (doc == NULL) {
        return 0;
    }
    if (!check_header(doc, err, errlen)) {
        json_free(doc);
        return 0;
    }
    v = json_object_get(doc, "preferences");
    if (v == NULL || v->type != JSON_OBJECT) {
        str_set_error(err, errlen, "'%s' has no preferences section", platform_path_basename(path));
        json_free(doc);
        return 0;
    }
    n = prefs_from_json(prefs, v, warnings);
    if (changed != NULL) {
        *changed = n;
    }
    json_free(doc);
    return 1;
}
