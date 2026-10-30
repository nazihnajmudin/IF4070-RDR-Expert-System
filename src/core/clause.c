/**
 * @file clause.c
 * @brief Implementasi parser dan operasi ADT Clause serta ClauseList.
 */

#include "rdr/clause.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

/* ===================================================================== */
/* Lexical Helpers                                                       */
/* ===================================================================== */

static int is_ident_start(char c)
{
    return isalpha((unsigned char)c) || c == '_';
}

static int is_ident_char(char c)
{
    return isalnum((unsigned char)c) || c == '_';
}

static const char *skip_spaces(const char *p)
{
    while (*p != '\0' && isspace((unsigned char)*p)) {
        p++;
    }
    return p;
}

/* Membaca operator di posisi p, mengembalikan panjangnya atau 0 jika tidak ada. */
static size_t read_operator(const char *p, ClauseOp *op)
{
    if (p[0] == '!' && p[1] == '=') {
        *op = OP_NE;
        return 2;
    }
    if (p[0] == '>' && p[1] == '=') {
        *op = OP_GE;
        return 2;
    }
    if (p[0] == '<' && p[1] == '=') {
        *op = OP_LE;
        return 2;
    }
    if (p[0] == '=') {
        *op = OP_EQ;
        return 1;
    }
    if (p[0] == '>') {
        *op = OP_GT;
        return 1;
    }
    if (p[0] == '<') {
        *op = OP_LT;
        return 1;
    }
    return 0;
}

/* ===================================================================== */
/* Clause Parsing                                                        */
/* ===================================================================== */

int clause_parse(const char *text, Clause *out, char *err, size_t errlen)
{
    char *trimmed;
    const char *p;
    const char *ident_begin;
    const char *value_begin;
    const char *value_end;
    size_t op_len;
    int negated = 0;
    Clause c;
    int ok = 0;

    /* Empty Check */
    if (text == NULL || str_is_blank(text)) {
        str_set_error(err, errlen, "empty clause");
        return 0;
    }
    trimmed = str_trim_dup(text);
    if (strcmp(trimmed, CLAUSE_TRUE_KEYWORD) == 0) {
        str_set_error(err, errlen, "'T' is reserved for an always-true rule and cannot be used as a clause");
        free(trimmed);
        return 0;
    }

    memset(&c, 0, sizeof(c));
    p = trimmed;

    /* Negation Prefix */
    if (*p == '!') {
        negated = 1;
        p = skip_spaces(p + 1);
    }

    /* Variable Name */
    ident_begin = p;
    if (!is_ident_start(*p)) {
        str_set_error(err, errlen, "'%s': a variable name must start with a letter or '_'", trimmed);
        goto done;
    }
    while (is_ident_char(*p)) {
        p++;
    }
    c.var = str_ndup(ident_begin, (size_t)(p - ident_begin));
    if (strcmp(c.var, CLAUSE_TRUE_KEYWORD) == 0) {
        str_set_error(err, errlen, "'%s': 'T' is a reserved word and cannot be a variable name", trimmed);
        goto done;
    }
    p = skip_spaces(p);

    /* Boolean Clause */
    if (*p == '\0') {
        c.type = CLAUSE_BOOL;
        c.op = negated ? OP_NE : OP_EQ;
        ok = 1;
        goto done;
    }

    /* Operator */
    op_len = read_operator(p, &c.op);
    if (op_len == 0) {
        str_set_error(err, errlen, "'%s': expected an operator (=, !=, >, <, >=, <=) after '%s'",
                      trimmed, c.var);
        goto done;
    }
    if (negated) {
        str_set_error(err, errlen, "'%s': '!' can only negate a boolean variable, use '!=' instead",
                      trimmed);
        goto done;
    }
    p = skip_spaces(p + op_len);

    /* Value */
    if (*p == '\0') {
        str_set_error(err, errlen, "'%s': missing value after '%s'", trimmed, clause_op_symbol(c.op));
        goto done;
    }
    value_begin = p;
    while (*p != '\0' && !isspace((unsigned char)*p)) {
        p++;
    }
    value_end = p;
    if (*skip_spaces(p) != '\0') {
        str_set_error(err, errlen, "'%s': a value must be a single word without spaces (use '_' instead)",
                      trimmed);
        goto done;
    }
    c.value = str_ndup(value_begin, (size_t)(value_end - value_begin));
    if (str_parse_number(c.value, &c.number)) {
        c.type = CLAUSE_NUMERIC;
        ok = 1;
        goto done;
    }

    /* Literal Validation */
    if (c.value[0] == '!' || strpbrk(c.value, "=<>;") != NULL) {
        str_set_error(err, errlen, "'%s': invalid value '%s'", trimmed, c.value);
        goto done;
    }
    if (c.op != OP_EQ && c.op != OP_NE) {
        str_set_error(err, errlen, "'%s': operator '%s' requires a numeric value",
                      trimmed, clause_op_symbol(c.op));
        goto done;
    }
    c.type = CLAUSE_LITERAL;
    ok = 1;

done:
    free(trimmed);
    if (!ok) {
        clause_free(&c);
        return 0;
    }
    *out = c;
    return 1;
}

/* ===================================================================== */
/* Clause Operations                                                     */
/* ===================================================================== */

void clause_free(Clause *c)
{
    if (c == NULL) {
        return;
    }
    free(c->var);
    free(c->value);
    c->var = NULL;
    c->value = NULL;
}

void clause_copy(Clause *dst, const Clause *src)
{
    dst->type = src->type;
    dst->op = src->op;
    dst->var = str_dup(src->var);
    dst->value = str_dup(src->value);
    dst->number = src->number;
}

void clause_format(const Clause *c, StrBuf *sb)
{
    if (c->type == CLAUSE_BOOL) {
        if (c->op == OP_NE) {
            strbuf_append_char(sb, '!');
        }
        strbuf_append(sb, c->var);
        return;
    }
    strbuf_appendf(sb, "%s %s %s", c->var, clause_op_symbol(c->op), c->value);
}

char *clause_to_string(const Clause *c)
{
    StrBuf sb;

    strbuf_init(&sb);
    clause_format(c, &sb);
    return strbuf_steal(&sb);
}

int clause_equals(const Clause *a, const Clause *b)
{
    if (a->type != b->type || a->op != b->op || strcmp(a->var, b->var) != 0) {
        return 0;
    }
    switch (a->type) {
    case CLAUSE_BOOL:
        return 1;
    case CLAUSE_LITERAL:
        return strcmp(a->value, b->value) == 0;
    case CLAUSE_NUMERIC:
        return a->number == b->number;
    }
    return 0;
}

const char *clause_op_symbol(ClauseOp op)
{
    switch (op) {
    case OP_EQ:
        return "=";
    case OP_NE:
        return "!=";
    case OP_GT:
        return ">";
    case OP_LT:
        return "<";
    case OP_GE:
        return ">=";
    case OP_LE:
        return "<=";
    }
    return "?";
}

const char *clause_type_name(ClauseType type)
{
    switch (type) {
    case CLAUSE_BOOL:
        return "boolean";
    case CLAUSE_LITERAL:
        return "literal";
    case CLAUSE_NUMERIC:
        return "numeric";
    }
    return "unknown";
}

int clause_is_true_keyword(const char *text)
{
    char *trimmed;
    int result;

    if (text == NULL) {
        return 0;
    }
    trimmed = str_trim_dup(text);
    result = strcmp(trimmed, CLAUSE_TRUE_KEYWORD) == 0;
    free(trimmed);
    return result;
}

/* ===================================================================== */
/* Clause List                                                           */
/* ===================================================================== */

void clauselist_init(ClauseList *list)
{
    list->items = NULL;
    list->count = 0;
    list->cap = 0;
}

void clauselist_free(ClauseList *list)
{
    clauselist_clear(list);
    free(list->items);
    clauselist_init(list);
}

void clauselist_clear(ClauseList *list)
{
    size_t i;

    for (i = 0; i < list->count; i++) {
        clause_free(&list->items[i]);
    }
    list->count = 0;
}

static void clauselist_grow(ClauseList *list)
{
    if (list->count == list->cap) {
        list->cap = list->cap ? list->cap * 2 : 4;
        list->items = xrealloc(list->items, list->cap * sizeof(Clause));
    }
}

void clauselist_push_move(ClauseList *list, Clause *c)
{
    clauselist_grow(list);
    list->items[list->count++] = *c;
    c->var = NULL;
    c->value = NULL;
}

void clauselist_push_copy(ClauseList *list, const Clause *c)
{
    clauselist_grow(list);
    clause_copy(&list->items[list->count], c);
    list->count++;
}

void clauselist_remove_at(ClauseList *list, size_t index)
{
    if (index >= list->count) {
        return;
    }
    clause_free(&list->items[index]);
    memmove(&list->items[index], &list->items[index + 1],
            (list->count - index - 1) * sizeof(Clause));
    list->count--;
}

void clauselist_copy(ClauseList *dst, const ClauseList *src)
{
    size_t i;

    if (dst == src) {
        return;
    }
    clauselist_clear(dst);
    for (i = 0; i < src->count; i++) {
        clauselist_push_copy(dst, &src->items[i]);
    }
}

int clauselist_contains(const ClauseList *list, const Clause *c)
{
    size_t i;

    for (i = 0; i < list->count; i++) {
        if (clause_equals(&list->items[i], c)) {
            return 1;
        }
    }
    return 0;
}

int clauselist_equals(const ClauseList *a, const ClauseList *b)
{
    size_t i;

    for (i = 0; i < a->count; i++) {
        if (!clauselist_contains(b, &a->items[i])) {
            return 0;
        }
    }
    for (i = 0; i < b->count; i++) {
        if (!clauselist_contains(a, &b->items[i])) {
            return 0;
        }
    }
    return 1;
}

char *clauselist_join(const ClauseList *list, const char *sep)
{
    StrBuf sb;
    size_t i;

    strbuf_init(&sb);
    for (i = 0; i < list->count; i++) {
        if (i > 0) {
            strbuf_append(&sb, sep);
        }
        clause_format(&list->items[i], &sb);
    }
    return strbuf_steal(&sb);
}

char *clauselist_rule_text(const ClauseList *list)
{
    if (list->count == 0) {
        return str_dup(CLAUSE_TRUE_KEYWORD);
    }
    return clauselist_join(list, " AND ");
}

void clauselist_to_strlist(const ClauseList *list, StrList *out)
{
    size_t i;

    for (i = 0; i < list->count; i++) {
        strlist_push_owned(out, clause_to_string(&list->items[i]));
    }
}
