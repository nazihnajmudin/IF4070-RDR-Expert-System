/**
 * @file query.c
 * @brief Implementasi parser teks kueri dan teks rule.
 */

#include "rdr/query.h"

#include <stdlib.h>
#include <string.h>

static int is_separator(char c)
{
    return c == ';' || c == '\n' || c == '\r';
}

void query_split(const char *text, StrList *out)
{
    const char *start;
    const char *p;

    if (text == NULL) {
        return;
    }
    start = text;
    for (p = text;; p++) {
        if (*p == '\0' || is_separator(*p)) {
            char *piece = str_trim_range(start, p);
            if (piece[0] != '\0') {
                strlist_push_owned(out, piece);
            } else {
                free(piece);
            }
            if (*p == '\0') {
                break;
            }
            start = p + 1;
        }
    }
}

size_t query_parse_list(const StrList *pieces, ClauseList *out, StrList *errors)
{
    size_t failures = 0;
    size_t i;
    char err[256];

    for (i = 0; i < pieces->count; i++) {
        Clause c;
        if (!clause_parse(pieces->items[i], &c, err, sizeof(err))) {
            failures++;
            if (errors != NULL) {
                strlist_pushf(errors, "Clause %lu: %s", (unsigned long)(i + 1), err);
            }
            continue;
        }
        if (clauselist_contains(out, &c)) {
            clause_free(&c);  /* duplikat dibuang */
            continue;
        }
        clauselist_push_move(out, &c);
    }
    return failures;
}

size_t query_parse(const char *text, ClauseList *out, StrList *errors)
{
    StrList pieces;
    size_t failures;

    strlist_init(&pieces);
    query_split(text, &pieces);
    failures = query_parse_list(&pieces, out, errors);
    strlist_free(&pieces);
    return failures;
}

size_t query_parse_rule(const char *text, ClauseList *out, StrList *errors)
{
    StrList pieces;
    size_t failures = 0;
    size_t i;
    int has_keyword = 0;

    strlist_init(&pieces);
    query_split(text, &pieces);

    /* Keyword Detection */
    for (i = 0; i < pieces.count; i++) {
        if (clause_is_true_keyword(pieces.items[i])) {
            has_keyword = 1;
        }
    }

    /* Rule Parsing */
    if (has_keyword) {
        if (pieces.count > 1) {
            failures++;
            if (errors != NULL) {
                strlist_push(errors, "'T' means an always-true rule and cannot be combined with other clauses");
            }
        }
    } else {
        failures = query_parse_list(&pieces, out, errors);
    }

    strlist_free(&pieces);
    return failures;
}

char *query_format(const ClauseList *list)
{
    return clauselist_join(list, QUERY_SEPARATOR);
}
