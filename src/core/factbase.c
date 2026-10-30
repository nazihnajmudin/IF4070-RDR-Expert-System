/**
 * @file factbase.c
 * @brief Implementasi ADT FactBase.
 */

#include "rdr/factbase.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "rdr/numset.h"

/**
 * @brief Ringkasan fakta untuk satu pasangan (jenis, variabel).
 */
typedef struct {
    ClauseType type;   /**< Jenis klausa. */
    char *var;         /**< Nama variabel (milik entry). */
    int has_pos;       /**< Boolean: ada fakta v. */
    int has_neg;       /**< Boolean: ada fakta !v. */
    char *eq_value;    /**< Literal: nilai fakta v = a, NULL jika tidak ada. */
    StrList ne_values; /**< Literal: nilai-nilai fakta v != a. */
    NumSet set;        /**< Numerik: irisan seluruh fakta numerik. */
} FactEntry;

struct FactBase {
    FactEntry *entries;  /**< Array entry. */
    size_t count;        /**< Jumlah entry. */
    size_t cap;          /**< Kapasitas array. */
    int consistent;      /**< 1 jika tidak ada kontradiksi. */
};

/* ===================================================================== */
/* Entry Management                                                      */
/* ===================================================================== */

static FactEntry *find_entry(const FactBase *fb, ClauseType type, const char *var)
{
    size_t i;

    for (i = 0; i < fb->count; i++) {
        if (fb->entries[i].type == type && strcmp(fb->entries[i].var, var) == 0) {
            return &fb->entries[i];
        }
    }
    return NULL;
}

static FactEntry *get_or_add_entry(FactBase *fb, ClauseType type, const char *var)
{
    FactEntry *e = find_entry(fb, type, var);

    if (e != NULL) {
        return e;
    }
    if (fb->count == fb->cap) {
        fb->cap = fb->cap ? fb->cap * 2 : 8;
        fb->entries = xrealloc(fb->entries, fb->cap * sizeof(FactEntry));
    }
    e = &fb->entries[fb->count++];
    memset(e, 0, sizeof(*e));
    e->type = type;
    e->var = str_dup(var);
    strlist_init(&e->ne_values);
    numset_init_all(&e->set);
    return e;
}

static void report(FactBase *fb, StrList *errors, const char *fmt, ...)
{
    va_list args;
    StrBuf sb;

    fb->consistent = 0;
    if (errors == NULL) {
        return;
    }
    strbuf_init(&sb);
    va_start(args, fmt);
    strbuf_appendv(&sb, fmt, args);
    va_end(args);
    strlist_push_owned(errors, strbuf_steal(&sb));
}

/* ===================================================================== */
/* Fact Insertion                                                        */
/* ===================================================================== */

static void add_bool_fact(FactBase *fb, const Clause *c, StrList *errors)
{
    FactEntry *e = get_or_add_entry(fb, CLAUSE_BOOL, c->var);
    int was_conflict = e->has_pos && e->has_neg;

    if (c->op == OP_EQ) {
        e->has_pos = 1;
    } else {
        e->has_neg = 1;
    }
    if (!was_conflict && e->has_pos && e->has_neg) {
        report(fb, errors, "Contradiction: '%s' and '!%s' cannot both be true", c->var, c->var);
    }
}

static void add_literal_fact(FactBase *fb, const Clause *c, StrList *errors)
{
    FactEntry *e = get_or_add_entry(fb, CLAUSE_LITERAL, c->var);

    if (c->op == OP_EQ) {
        if (e->eq_value != NULL && strcmp(e->eq_value, c->value) != 0) {
            report(fb, errors, "Contradiction: '%s' cannot equal both '%s' and '%s'",
                   c->var, e->eq_value, c->value);
            return;
        }
        if (strlist_index_of(&e->ne_values, c->value) >= 0) {
            report(fb, errors, "Contradiction: '%s = %s' and '%s != %s'",
                   c->var, c->value, c->var, c->value);
            return;
        }
        if (e->eq_value == NULL) {
            e->eq_value = str_dup(c->value);
        }
        return;
    }

    if (e->eq_value != NULL && strcmp(e->eq_value, c->value) == 0) {
        report(fb, errors, "Contradiction: '%s = %s' and '%s != %s'",
               c->var, c->value, c->var, c->value);
        return;
    }
    if (strlist_index_of(&e->ne_values, c->value) < 0) {
        strlist_push(&e->ne_values, c->value);
    }
}

static void add_numeric_fact(FactBase *fb, const Clause *c, StrList *errors)
{
    FactEntry *e = get_or_add_entry(fb, CLAUSE_NUMERIC, c->var);
    int was_empty = numset_is_empty(&e->set);

    numset_intersect_op(&e->set, c->op, c->number);
    if (!was_empty && numset_is_empty(&e->set)) {
        StrBuf sb;
        strbuf_init(&sb);
        clause_format(c, &sb);
        report(fb, errors, "Contradiction: no value of '%s' satisfies all numeric facts (conflict at '%s')",
               c->var, strbuf_cstr(&sb));
        strbuf_free(&sb);
    }
}

/* ===================================================================== */
/* Public Interface                                                      */
/* ===================================================================== */

FactBase *factbase_create(const ClauseList *facts, StrList *errors)
{
    FactBase *fb = xcalloc(1, sizeof(FactBase));
    size_t i;

    fb->consistent = 1;
    for (i = 0; i < facts->count; i++) {
        const Clause *c = &facts->items[i];
        switch (c->type) {
        case CLAUSE_BOOL:
            add_bool_fact(fb, c, errors);
            break;
        case CLAUSE_LITERAL:
            add_literal_fact(fb, c, errors);
            break;
        case CLAUSE_NUMERIC:
            add_numeric_fact(fb, c, errors);
            break;
        }
    }
    return fb;
}

void factbase_destroy(FactBase *fb)
{
    size_t i;

    if (fb == NULL) {
        return;
    }
    for (i = 0; i < fb->count; i++) {
        free(fb->entries[i].var);
        free(fb->entries[i].eq_value);
        strlist_free(&fb->entries[i].ne_values);
        numset_free(&fb->entries[i].set);
    }
    free(fb->entries);
    free(fb);
}

int factbase_is_consistent(const FactBase *fb)
{
    return fb->consistent;
}

int factbase_satisfies(const FactBase *fb, const Clause *rc)
{
    const FactEntry *e = find_entry(fb, rc->type, rc->var);

    if (e == NULL) {
        return 0;  /* tidak ada fakta, unknown */
    }
    switch (rc->type) {
    case CLAUSE_BOOL:
        return rc->op == OP_EQ ? e->has_pos : e->has_neg;

    case CLAUSE_LITERAL:
        if (rc->op == OP_EQ) {
            return e->eq_value != NULL && strcmp(e->eq_value, rc->value) == 0;
        }
        if (e->eq_value != NULL) {
            return strcmp(e->eq_value, rc->value) != 0;
        }
        return strlist_index_of(&e->ne_values, rc->value) >= 0;

    case CLAUSE_NUMERIC: {
        NumSet rule_set;
        int result;
        if (numset_is_empty(&e->set)) {
            return 0;  /* fakta kontradiktif tidak membuktikan apa pun */
        }
        numset_init_from_op(&rule_set, rc->op, rc->number);
        result = numset_is_subset(&e->set, &rule_set);
        numset_free(&rule_set);
        return result;
    }
    }
    return 0;
}

int factbase_satisfies_all(const FactBase *fb, const ClauseList *rule)
{
    size_t i;

    for (i = 0; i < rule->count; i++) {
        if (!factbase_satisfies(fb, &rule->items[i])) {
            return 0;
        }
    }
    return 1;
}

int factbase_check(const ClauseList *facts, StrList *errors)
{
    FactBase *fb = factbase_create(facts, errors);
    int ok = fb->consistent;

    factbase_destroy(fb);
    return ok;
}

int factbase_list_satisfies(const ClauseList *facts, const ClauseList *rule)
{
    FactBase *fb = factbase_create(facts, NULL);
    int ok = factbase_satisfies_all(fb, rule);

    factbase_destroy(fb);
    return ok;
}
