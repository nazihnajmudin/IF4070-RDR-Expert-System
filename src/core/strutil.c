/**
 * @file strutil.c
 * @brief Implementasi utilitas string, buffer dinamis, dan daftar.
 */

#include "rdr/strutil.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ===================================================================== */
/* Memory Allocation                                                     */
/* ===================================================================== */

static void out_of_memory(void)
{
    fprintf(stderr, "fatal: out of memory\n");
    abort();
}

void *xmalloc(size_t size)
{
    void *p = malloc(size ? size : 1);
    if (p == NULL) {
        out_of_memory();
    }
    return p;
}

void *xcalloc(size_t count, size_t size)
{
    void *p = calloc(count ? count : 1, size ? size : 1);
    if (p == NULL) {
        out_of_memory();
    }
    return p;
}

void *xrealloc(void *ptr, size_t size)
{
    void *p = realloc(ptr, size ? size : 1);
    if (p == NULL) {
        out_of_memory();
    }
    return p;
}

/* ===================================================================== */
/* String Helpers                                                        */
/* ===================================================================== */

char *str_dup(const char *s)
{
    if (s == NULL) {
        return NULL;
    }
    return str_ndup(s, strlen(s));
}

char *str_ndup(const char *s, size_t n)
{
    size_t len = 0;
    char *copy;

    while (len < n && s[len] != '\0') {
        len++;
    }
    copy = xmalloc(len + 1);
    memcpy(copy, s, len);
    copy[len] = '\0';
    return copy;
}

char *str_trim_range(const char *begin, const char *end)
{
    while (begin < end && isspace((unsigned char)*begin)) {
        begin++;
    }
    while (end > begin && isspace((unsigned char)end[-1])) {
        end--;
    }
    return str_ndup(begin, (size_t)(end - begin));
}

char *str_trim_dup(const char *s)
{
    return str_trim_range(s, s + strlen(s));
}

int str_is_blank(const char *s)
{
    if (s == NULL) {
        return 1;
    }
    for (; *s != '\0'; s++) {
        if (!isspace((unsigned char)*s)) {
            return 0;
        }
    }
    return 1;
}

int str_iequals(const char *a, const char *b)
{
    if (a == NULL || b == NULL) {
        return a == b;
    }
    while (*a != '\0' && *b != '\0') {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) {
            return 0;
        }
        a++;
        b++;
    }
    return *a == *b;
}

int str_parse_number(const char *s, double *out)
{
    const char *p = s;
    int digits = 0;
    char *end = NULL;
    double value;

    /* Syntax Check */
    if (s == NULL) {
        return 0;
    }
    if (*p == '+' || *p == '-') {
        p++;
    }
    while (isdigit((unsigned char)*p)) {
        p++;
        digits++;
    }
    if (*p == '.') {
        p++;
        while (isdigit((unsigned char)*p)) {
            p++;
            digits++;
        }
    }
    if (digits == 0 || *p != '\0') {
        return 0;
    }

    /* Conversion */
    value = strtod(s, &end);
    if (end == NULL || *end != '\0') {
        return 0;
    }
    if (out != NULL) {
        *out = value;
    }
    return 1;
}

char *str_printf(const char *fmt, ...)
{
    StrBuf sb;
    va_list args;

    strbuf_init(&sb);
    va_start(args, fmt);
    strbuf_appendv(&sb, fmt, args);
    va_end(args);
    return strbuf_steal(&sb);
}

void str_copy(char *dst, size_t size, const char *src)
{
    size_t len;

    if (dst == NULL || size == 0) {
        return;
    }
    if (src == NULL) {
        dst[0] = '\0';
        return;
    }
    len = strlen(src);
    if (len >= size) {
        len = size - 1;
    }
    memcpy(dst, src, len);
    dst[len] = '\0';
}

void str_set_error(char *dst, size_t size, const char *fmt, ...)
{
    va_list args;

    if (dst == NULL || size == 0) {
        return;
    }
    va_start(args, fmt);
    vsnprintf(dst, size, fmt, args);
    va_end(args);
}

/* ===================================================================== */
/* Dynamic String Buffer                                                 */
/* ===================================================================== */

static void strbuf_reserve(StrBuf *sb, size_t extra)
{
    size_t needed = sb->len + extra + 1;
    size_t cap;

    if (needed <= sb->cap) {
        return;
    }
    cap = sb->cap ? sb->cap : 32;
    while (cap < needed) {
        cap *= 2;
    }
    sb->data = xrealloc(sb->data, cap);
    sb->cap = cap;
}

void strbuf_init(StrBuf *sb)
{
    sb->data = NULL;
    sb->len = 0;
    sb->cap = 0;
}

void strbuf_free(StrBuf *sb)
{
    free(sb->data);
    strbuf_init(sb);
}

void strbuf_clear(StrBuf *sb)
{
    sb->len = 0;
    if (sb->data != NULL) {
        sb->data[0] = '\0';
    }
}

void strbuf_append_n(StrBuf *sb, const char *s, size_t n)
{
    strbuf_reserve(sb, n);
    memcpy(sb->data + sb->len, s, n);
    sb->len += n;
    sb->data[sb->len] = '\0';
}

void strbuf_append(StrBuf *sb, const char *s)
{
    if (s != NULL) {
        strbuf_append_n(sb, s, strlen(s));
    }
}

void strbuf_append_char(StrBuf *sb, char c)
{
    strbuf_append_n(sb, &c, 1);
}

void strbuf_appendv(StrBuf *sb, const char *fmt, va_list args)
{
    va_list copy;
    int needed;

    va_copy(copy, args);
    needed = vsnprintf(NULL, 0, fmt, copy);
    va_end(copy);
    if (needed <= 0) {
        return;
    }
    strbuf_reserve(sb, (size_t)needed);
    vsnprintf(sb->data + sb->len, (size_t)needed + 1, fmt, args);
    sb->len += (size_t)needed;
}

void strbuf_appendf(StrBuf *sb, const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    strbuf_appendv(sb, fmt, args);
    va_end(args);
}

char *strbuf_steal(StrBuf *sb)
{
    char *result = sb->data;

    if (result == NULL) {
        result = str_dup("");
    }
    strbuf_init(sb);
    return result;
}

const char *strbuf_cstr(const StrBuf *sb)
{
    return sb->data ? sb->data : "";
}

/* ===================================================================== */
/* String List                                                           */
/* ===================================================================== */

void strlist_init(StrList *list)
{
    list->items = NULL;
    list->count = 0;
    list->cap = 0;
}

void strlist_free(StrList *list)
{
    strlist_clear(list);
    free(list->items);
    strlist_init(list);
}

void strlist_clear(StrList *list)
{
    size_t i;

    for (i = 0; i < list->count; i++) {
        free(list->items[i]);
    }
    list->count = 0;
}

void strlist_push_owned(StrList *list, char *s)
{
    if (list->count == list->cap) {
        list->cap = list->cap ? list->cap * 2 : 8;
        list->items = xrealloc(list->items, list->cap * sizeof(char *));
    }
    list->items[list->count++] = s;
}

void strlist_push(StrList *list, const char *s)
{
    strlist_push_owned(list, str_dup(s ? s : ""));
}

void strlist_pushf(StrList *list, const char *fmt, ...)
{
    StrBuf sb;
    va_list args;

    strbuf_init(&sb);
    va_start(args, fmt);
    strbuf_appendv(&sb, fmt, args);
    va_end(args);
    strlist_push_owned(list, strbuf_steal(&sb));
}

void strlist_remove_at(StrList *list, size_t index)
{
    if (index >= list->count) {
        return;
    }
    free(list->items[index]);
    memmove(&list->items[index], &list->items[index + 1],
            (list->count - index - 1) * sizeof(char *));
    list->count--;
}

void strlist_set(StrList *list, size_t index, const char *s)
{
    if (index >= list->count) {
        return;
    }
    free(list->items[index]);
    list->items[index] = str_dup(s ? s : "");
}

void strlist_copy(StrList *dst, const StrList *src)
{
    size_t i;

    if (dst == src) {
        return;
    }
    strlist_clear(dst);
    for (i = 0; i < src->count; i++) {
        strlist_push(dst, src->items[i]);
    }
}

long strlist_index_of(const StrList *list, const char *s)
{
    size_t i;

    for (i = 0; i < list->count; i++) {
        if (strcmp(list->items[i], s) == 0) {
            return (long)i;
        }
    }
    return -1;
}

char *strlist_join(const StrList *list, const char *sep)
{
    StrBuf sb;
    size_t i;

    strbuf_init(&sb);
    for (i = 0; i < list->count; i++) {
        if (i > 0) {
            strbuf_append(&sb, sep);
        }
        strbuf_append(&sb, list->items[i]);
    }
    return strbuf_steal(&sb);
}

/* ===================================================================== */
/* Integer List                                                          */
/* ===================================================================== */

void intlist_init(IntList *list)
{
    list->items = NULL;
    list->count = 0;
    list->cap = 0;
}

void intlist_free(IntList *list)
{
    free(list->items);
    intlist_init(list);
}

void intlist_clear(IntList *list)
{
    list->count = 0;
}

void intlist_push(IntList *list, int value)
{
    if (list->count == list->cap) {
        list->cap = list->cap ? list->cap * 2 : 8;
        list->items = xrealloc(list->items, list->cap * sizeof(int));
    }
    list->items[list->count++] = value;
}

void intlist_copy(IntList *dst, const IntList *src)
{
    size_t i;

    if (dst == src) {
        return;
    }
    intlist_clear(dst);
    for (i = 0; i < src->count; i++) {
        intlist_push(dst, src->items[i]);
    }
}

long intlist_index_of(const IntList *list, int value)
{
    size_t i;

    for (i = 0; i < list->count; i++) {
        if (list->items[i] == value) {
            return (long)i;
        }
    }
    return -1;
}
