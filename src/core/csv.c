/**
 * @file csv.c
 * @brief Implementasi pembaca CSV RFC 4180.
 */

#include "rdr/csv.h"

#include <stdlib.h>

void csv_table_init(CsvTable *t)
{
    t->rows = NULL;
    t->lines = NULL;
    t->count = 0;
    t->cap = 0;
}

void csv_table_free(CsvTable *t)
{
    size_t i;

    for (i = 0; i < t->count; i++) {
        strlist_free(&t->rows[i]);
    }
    free(t->rows);
    free(t->lines);
    csv_table_init(t);
}

/* Menambahkan record ke tabel, kecuali record kosong (satu field kosong). */
static void push_row(CsvTable *t, StrList *row, size_t line)
{
    if (row->count == 1 && row->items[0][0] == '\0') {
        strlist_free(row);
        return;
    }
    if (t->count == t->cap) {
        t->cap = t->cap ? t->cap * 2 : 16;
        t->rows = xrealloc(t->rows, t->cap * sizeof(StrList));
        t->lines = xrealloc(t->lines, t->cap * sizeof(size_t));
    }
    t->rows[t->count] = *row;
    t->lines[t->count] = line;
    t->count++;
    strlist_init(row);
}

int csv_parse(const char *text, size_t len, CsvTable *out, char *err, size_t errlen)
{
    const char *p = text;
    const char *end = text + len;
    StrList row;
    StrBuf field;
    size_t line = 1;
    size_t row_line = 1;

    csv_table_free(out);
    strlist_init(&row);
    strbuf_init(&field);
    if (len >= 3 && (unsigned char)p[0] == 0xEF && (unsigned char)p[1] == 0xBB &&
        (unsigned char)p[2] == 0xBF) {
        p += 3;
    }

    while (p < end) {
        /* Quoted Field */
        if (*p == '"') {
            size_t quote_line = line;
            p++;
            for (;;) {
                if (p >= end) {
                    str_set_error(err, errlen, "line %lu: unterminated quoted field", (unsigned long)quote_line);
                    strbuf_free(&field);
                    strlist_free(&row);
                    csv_table_free(out);
                    return 0;
                }
                if (*p == '"') {
                    if (p + 1 < end && p[1] == '"') {
                        strbuf_append_char(&field, '"');
                        p += 2;
                        continue;
                    }
                    p++;
                    break;
                }
                if (*p == '\n') {
                    line++;
                }
                strbuf_append_char(&field, *p++);
            }
            if (p < end && *p != ',' && *p != '\n' && *p != '\r') {
                str_set_error(err, errlen, "line %lu: unexpected character after a closing quote",
                              (unsigned long)line);
                strbuf_free(&field);
                strlist_free(&row);
                csv_table_free(out);
                return 0;
            }
        } else {
            /* Unquoted Field */
            while (p < end && *p != ',' && *p != '\n' && *p != '\r') {
                strbuf_append_char(&field, *p++);
            }
        }

        /* Field Terminator */
        strlist_push_owned(&row, strbuf_steal(&field));
        if (p < end && *p == ',') {
            p++;
            if (p == end) {
                strlist_push(&row, "");  /* koma di akhir teks berarti field kosong terakhir */
            }
            continue;
        }
        if (p < end && *p == '\r') {
            p++;
        }
        if (p < end && *p == '\n') {
            p++;
        }
        push_row(out, &row, row_line);
        line++;
        row_line = line;
    }
    if (row.count > 0) {
        push_row(out, &row, row_line);
    }

    strbuf_free(&field);
    strlist_free(&row);
    return 1;
}
