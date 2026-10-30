/**
 * @file prefs.c
 * @brief Tabel deskriptor dan implementasi ADT Preferences.
 */

#include "rdr/prefs.h"

#include <stdlib.h>
#include <string.h>

/* ===================================================================== */
/* Descriptor Table                                                      */
/* ===================================================================== */

static const char *const CLAUSE_SELECTION_OPTIONS[] = {"editable", "strict"};
static const char *const CLAUSE_SELECTION_LABELS[] = {"Editable", "Strict (always use every clause)"};

static const char *const REVISION_OPTIONS[] = {"revision_node", "edit_conclusion", "reject"};
static const char *const REVISION_LABELS[] = {"Revision node (T >> new conclusion)",
                                              "Edit conclusion (not RDR compliant)",
                                              "Reject"};

static const char *const THEME_OPTIONS[] = {"light", "dark"};
static const char *const THEME_LABELS[] = {"Light", "Dark"};

static const char *const HELP_LANGUAGE_OPTIONS[] = {"id", "en"};
static const char *const HELP_LANGUAGE_LABELS[] = {"Bahasa Indonesia", "English"};

/* Urutan entri wajib sama dengan enum PrefId. */
static const PrefDescriptor DESCRIPTORS[PREF_COUNT] = {
    {"tree_show_empty_slots", "Show empty slots", "Tree",
     "Show empty true and false slots in the tree view.",
     PREF_BOOL, 1, 0, 1, NULL, NULL, 0},
    {"tree_show_node_id", "Show node id", "Tree",
     "Show the #id of every node in the tree view.",
     PREF_BOOL, 1, 0, 1, NULL, NULL, 0},
    {"tree_show_cornerstone", "Show cornerstone", "Tree",
     "Show the cornerstone case below every node in the tree view.",
     PREF_BOOL, 0, 0, 1, NULL, NULL, 0},
    {"tree_highlight", "Highlight reasoning", "Tree",
     "Color fired and failed nodes after a query is started.",
     PREF_BOOL, 1, 0, 1, NULL, NULL, 0},
    {"result_show_rule", "Show rule in result", "Result",
     "Show the rule that produced each conclusion in the result view.",
     PREF_BOOL, 1, 0, 1, NULL, NULL, 0},
    {"acquisition_clause_selection", "Exception clauses", "Acquisition",
     "Whether the correction dialog lets you pick and edit the exception clauses.",
     PREF_ENUM, PREF_CLAUSE_EDITABLE, 0, 1, CLAUSE_SELECTION_OPTIONS, CLAUSE_SELECTION_LABELS, 2},
    {"acquisition_revision_strategy", "Revision strategy", "Acquisition",
     "What to do when no clause distinguishes the case from the cornerstone case.",
     PREF_ENUM, 0, 0, 2, REVISION_OPTIONS, REVISION_LABELS, 3},
    {"ui_font_size", "Font size", "Appearance",
     "Font size of the whole interface.",
     PREF_INT, 16, 12, 28, NULL, NULL, 0},
    {"ui_theme", "Theme", "Appearance",
     "Color theme of the interface.",
     PREF_ENUM, PREF_THEME_LIGHT, 0, 1, THEME_OPTIONS, THEME_LABELS, 2},
    {"ui_help_language", "Help language", "Appearance",
     "Language of the Help page.",
     PREF_ENUM, PREF_HELP_ID, 0, 1, HELP_LANGUAGE_OPTIONS, HELP_LANGUAGE_LABELS, 2},
};

/* ===================================================================== */
/* Value Access                                                          */
/* ===================================================================== */

const PrefDescriptor *prefs_descriptor(PrefId id)
{
    if ((int)id < 0 || id >= PREF_COUNT) {
        return NULL;
    }
    return &DESCRIPTORS[id];
}

void prefs_init_defaults(Preferences *prefs)
{
    int i;

    for (i = 0; i < PREF_COUNT; i++) {
        prefs->values[i] = DESCRIPTORS[i].default_value;
    }
}

int prefs_get(const Preferences *prefs, PrefId id)
{
    return prefs->values[id];
}

static int value_is_valid(const PrefDescriptor *d, int value)
{
    switch (d->type) {
    case PREF_BOOL:
        return value == 0 || value == 1;
    case PREF_INT:
        return value >= d->min_value && value <= d->max_value;
    case PREF_ENUM:
        return value >= 0 && value < d->option_count;
    }
    return 0;
}

int prefs_set(Preferences *prefs, PrefId id, int value)
{
    const PrefDescriptor *d = prefs_descriptor(id);

    if (d == NULL || !value_is_valid(d, value)) {
        return 0;
    }
    prefs->values[id] = value;
    return 1;
}

int prefs_equal(const Preferences *a, const Preferences *b)
{
    return memcmp(a->values, b->values, sizeof(a->values)) == 0;
}

int prefs_find(const char *key, PrefId *out)
{
    int i;

    for (i = 0; i < PREF_COUNT; i++) {
        if (strcmp(DESCRIPTORS[i].key, key) == 0) {
            *out = (PrefId)i;
            return 1;
        }
    }
    return 0;
}

/* ===================================================================== */
/* Text Conversion                                                       */
/* ===================================================================== */

char *prefs_value_text(PrefId id, int value)
{
    const PrefDescriptor *d = prefs_descriptor(id);

    if (d->type == PREF_BOOL) {
        return str_dup(value ? "true" : "false");
    }
    if (d->type == PREF_ENUM && value >= 0 && value < d->option_count) {
        return str_dup(d->options[value]);
    }
    return str_printf("%d", value);
}

int prefs_set_from_text(Preferences *prefs, PrefId id, const char *text, char *err, size_t errlen)
{
    const PrefDescriptor *d = prefs_descriptor(id);
    double number;
    int i;

    switch (d->type) {
    case PREF_BOOL:
        if (str_iequals(text, "true") || strcmp(text, "1") == 0) {
            return prefs_set(prefs, id, 1);
        }
        if (str_iequals(text, "false") || strcmp(text, "0") == 0) {
            return prefs_set(prefs, id, 0);
        }
        str_set_error(err, errlen, "'%s' expects true or false", d->key);
        return 0;
    case PREF_INT:
        if (str_parse_number(text, &number) && number == (int)number &&
            prefs_set(prefs, id, (int)number)) {
            return 1;
        }
        str_set_error(err, errlen, "'%s' expects an integer between %d and %d",
                      d->key, d->min_value, d->max_value);
        return 0;
    case PREF_ENUM:
        for (i = 0; i < d->option_count; i++) {
            if (strcmp(text, d->options[i]) == 0) {
                return prefs_set(prefs, id, i);
            }
        }
        str_set_error(err, errlen, "'%s' has no option '%s'", d->key, text);
        return 0;
    }
    return 0;
}

/* ===================================================================== */
/* JSON Conversion                                                       */
/* ===================================================================== */

JsonValue *prefs_to_json(const Preferences *prefs)
{
    JsonValue *obj = json_new_object();
    int i;

    for (i = 0; i < PREF_COUNT; i++) {
        const PrefDescriptor *d = &DESCRIPTORS[i];
        int v = prefs->values[i];
        switch (d->type) {
        case PREF_BOOL:
            json_object_set(obj, d->key, json_new_bool(v));
            break;
        case PREF_INT:
            json_object_set(obj, d->key, json_new_number(v));
            break;
        case PREF_ENUM:
            json_object_set(obj, d->key, json_new_string(d->options[v]));
            break;
        }
    }
    return obj;
}

/* Mengonversi satu nilai JSON. Mengembalikan 1 jika valid. */
static int value_from_json(const PrefDescriptor *d, const JsonValue *v, int *out)
{
    int i;

    switch (d->type) {
    case PREF_BOOL:
        if (v->type != JSON_BOOL) {
            return 0;
        }
        *out = v->boolean;
        return 1;
    case PREF_INT:
        return json_get_int(v, out) && value_is_valid(d, *out);
    case PREF_ENUM:
        if (v->type != JSON_STRING) {
            return 0;
        }
        for (i = 0; i < d->option_count; i++) {
            if (strcmp(v->string, d->options[i]) == 0) {
                *out = i;
                return 1;
            }
        }
        return 0;
    }
    return 0;
}

size_t prefs_from_json(Preferences *prefs, const JsonValue *object, StrList *warnings)
{
    size_t changed = 0;
    size_t i;

    if (object == NULL || object->type != JSON_OBJECT) {
        if (warnings != NULL) {
            strlist_push(warnings, "preferences must be a JSON object, values were not changed");
        }
        return 0;
    }
    for (i = 0; i < object->count; i++) {
        PrefId id;
        int value;
        if (!prefs_find(object->keys[i], &id)) {
            if (warnings != NULL) {
                strlist_pushf(warnings, "unknown preference '%s' was ignored", object->keys[i]);
            }
            continue;
        }
        if (!value_from_json(&DESCRIPTORS[id], object->items[i], &value)) {
            if (warnings != NULL) {
                strlist_pushf(warnings, "invalid value for preference '%s' was ignored", object->keys[i]);
            }
            continue;
        }
        if (prefs->values[id] != value) {
            prefs->values[id] = value;
            changed++;
        }
    }
    return changed;
}
