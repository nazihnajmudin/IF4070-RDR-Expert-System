/**
 * @file json.c
 * @brief Implementasi parser dan writer JSON iteratif.
 */

#include "rdr/json.h"

#include <ctype.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rdr/strutil.h"

/* ===================================================================== */
/* Construction                                                          */
/* ===================================================================== */

static JsonValue *json_alloc(JsonType type)
{
    JsonValue *v = xcalloc(1, sizeof(JsonValue));
    v->type = type;
    return v;
}

JsonValue *json_new_null(void)
{
    return json_alloc(JSON_NULL);
}

JsonValue *json_new_bool(int value)
{
    JsonValue *v = json_alloc(JSON_BOOL);
    v->boolean = value ? 1 : 0;
    return v;
}

JsonValue *json_new_number(double value)
{
    JsonValue *v = json_alloc(JSON_NUMBER);
    v->number = value;
    return v;
}

JsonValue *json_new_string(const char *value)
{
    JsonValue *v = json_alloc(JSON_STRING);
    v->string = str_dup(value ? value : "");
    return v;
}

JsonValue *json_new_array(void)
{
    return json_alloc(JSON_ARRAY);
}

JsonValue *json_new_object(void)
{
    return json_alloc(JSON_OBJECT);
}

static void container_grow(JsonValue *c)
{
    if (c->count < c->cap) {
        return;
    }
    c->cap = c->cap ? c->cap * 2 : 4;
    c->items = xrealloc(c->items, c->cap * sizeof(JsonValue *));
    if (c->type == JSON_OBJECT) {
        c->keys = xrealloc(c->keys, c->cap * sizeof(char *));
    }
}

void json_array_push(JsonValue *array, JsonValue *value)
{
    container_grow(array);
    array->items[array->count++] = value;
}

void json_object_set(JsonValue *object, const char *key, JsonValue *value)
{
    size_t i;

    for (i = 0; i < object->count; i++) {
        if (strcmp(object->keys[i], key) == 0) {
            json_free(object->items[i]);
            object->items[i] = value;
            return;
        }
    }
    container_grow(object);
    object->keys[object->count] = str_dup(key);
    object->items[object->count] = value;
    object->count++;
}

void json_free(JsonValue *value)
{
    JsonValue **stack;
    size_t sp = 0;
    size_t cap = 32;

    if (value == NULL) {
        return;
    }
    stack = xmalloc(cap * sizeof(JsonValue *));
    stack[sp++] = value;
    while (sp > 0) {
        JsonValue *v = stack[--sp];
        size_t i;
        for (i = 0; i < v->count; i++) {
            if (sp == cap) {
                cap *= 2;
                stack = xrealloc(stack, cap * sizeof(JsonValue *));
            }
            stack[sp++] = v->items[i];
            if (v->keys != NULL) {
                free(v->keys[i]);
            }
        }
        free(v->items);
        free(v->keys);
        free(v->string);
        free(v);
    }
    free(stack);
}

/* ===================================================================== */
/* Access                                                                */
/* ===================================================================== */

JsonValue *json_object_get(const JsonValue *object, const char *key)
{
    size_t i;

    if (object == NULL || object->type != JSON_OBJECT) {
        return NULL;
    }
    for (i = 0; i < object->count; i++) {
        if (strcmp(object->keys[i], key) == 0) {
            return object->items[i];
        }
    }
    return NULL;
}

int json_get_int(const JsonValue *value, int *out)
{
    if (value == NULL || value->type != JSON_NUMBER) {
        return 0;
    }
    if (value->number != floor(value->number) || value->number < INT_MIN || value->number > INT_MAX) {
        return 0;
    }
    if (out != NULL) {
        *out = (int)value->number;
    }
    return 1;
}

const char *json_type_name(JsonType type)
{
    switch (type) {
    case JSON_NULL:
        return "null";
    case JSON_BOOL:
        return "boolean";
    case JSON_NUMBER:
        return "number";
    case JSON_STRING:
        return "string";
    case JSON_ARRAY:
        return "array";
    case JSON_OBJECT:
        return "object";
    }
    return "unknown";
}

/* ===================================================================== */
/* Parser State                                                          */
/* ===================================================================== */

/**
 * @brief Kondisi parser yang menentukan token yang diharapkan berikutnya.
 */
typedef enum {
    EXPECT_VALUE,           /**< Awal dokumen atau setelah ':' atau ',' di array. */
    EXPECT_VALUE_OR_CLOSE,  /**< Tepat setelah '['. */
    EXPECT_KEY,             /**< Setelah ',' di object. */
    EXPECT_KEY_OR_CLOSE,    /**< Tepat setelah '{'. */
    EXPECT_COMMA_OR_CLOSE   /**< Setelah sebuah nilai di dalam container. */
} ParseState;

/**
 * @brief Konteks parser.
 */
typedef struct {
    const char *text;   /**< Awal teks. */
    const char *p;      /**< Posisi saat ini. */
    const char *end;    /**< Akhir teks. */
    char *err;          /**< Buffer pesan error. */
    size_t errlen;      /**< Ukuran buffer pesan error. */
    int failed;         /**< 1 jika error sudah dilaporkan. */
    const char *mark;   /**< Posisi terakhir yang baris dan kolomnya sudah dihitung. */
    int mark_line;      /**< Baris pada posisi mark. */
    int mark_col;       /**< Kolom pada posisi mark. */
} Parser;

/* Menghitung baris dan kolom secara inkremental dari posisi mark terakhir. */
static void position_of(Parser *ps, const char *at, int *line, int *col)
{
    const char *q;

    if (at < ps->mark) {
        ps->mark = ps->text;
        ps->mark_line = 1;
        ps->mark_col = 1;
    }
    for (q = ps->mark; q < at; q++) {
        if (*q == '\n') {
            ps->mark_line++;
            ps->mark_col = 1;
        } else if (((unsigned char)*q & 0xC0) != 0x80) {  /* hitung karakter, bukan byte lanjutan UTF-8 */
            ps->mark_col++;
        }
    }
    ps->mark = at;
    *line = ps->mark_line;
    *col = ps->mark_col;
}

static void parse_error(Parser *ps, const char *msg)
{
    int line;
    int col;

    if (ps->failed) {
        return;
    }
    ps->failed = 1;
    position_of(ps, ps->p, &line, &col);
    str_set_error(ps->err, ps->errlen, "line %d, column %d: %s", line, col, msg);
}

static void skip_ws(Parser *ps)
{
    while (ps->p < ps->end &&
           (*ps->p == ' ' || *ps->p == '\t' || *ps->p == '\n' || *ps->p == '\r')) {
        ps->p++;
    }
}

/* ===================================================================== */
/* Scalar Parsing                                                        */
/* ===================================================================== */

static void append_utf8(StrBuf *sb, unsigned long cp)
{
    if (cp < 0x80) {
        strbuf_append_char(sb, (char)cp);
    } else if (cp < 0x800) {
        strbuf_append_char(sb, (char)(0xC0 | (cp >> 6)));
        strbuf_append_char(sb, (char)(0x80 | (cp & 0x3F)));
    } else if (cp < 0x10000) {
        strbuf_append_char(sb, (char)(0xE0 | (cp >> 12)));
        strbuf_append_char(sb, (char)(0x80 | ((cp >> 6) & 0x3F)));
        strbuf_append_char(sb, (char)(0x80 | (cp & 0x3F)));
    } else {
        strbuf_append_char(sb, (char)(0xF0 | (cp >> 18)));
        strbuf_append_char(sb, (char)(0x80 | ((cp >> 12) & 0x3F)));
        strbuf_append_char(sb, (char)(0x80 | ((cp >> 6) & 0x3F)));
        strbuf_append_char(sb, (char)(0x80 | (cp & 0x3F)));
    }
}

static int read_hex4(Parser *ps, unsigned long *out)
{
    int i;
    unsigned long v = 0;

    if (ps->end - ps->p < 4) {
        return 0;
    }
    for (i = 0; i < 4; i++) {
        char c = ps->p[i];
        v <<= 4;
        if (c >= '0' && c <= '9') {
            v |= (unsigned long)(c - '0');
        } else if (c >= 'a' && c <= 'f') {
            v |= (unsigned long)(c - 'a' + 10);
        } else if (c >= 'A' && c <= 'F') {
            v |= (unsigned long)(c - 'A' + 10);
        } else {
            return 0;
        }
    }
    ps->p += 4;
    *out = v;
    return 1;
}

/* Mem-parse string yang diawali '"'. Hasil milik pemanggil, NULL jika gagal. */
static char *parse_string(Parser *ps)
{
    StrBuf sb;

    strbuf_init(&sb);
    strbuf_append(&sb, "");
    ps->p++;  /* lewati tanda kutip pembuka */
    while (ps->p < ps->end) {
        unsigned char c = (unsigned char)*ps->p;
        if (c == '"') {
            ps->p++;
            return strbuf_steal(&sb);
        }
        if (c < 0x20) {
            parse_error(ps, "control character inside string");
            strbuf_free(&sb);
            return NULL;
        }
        if (c != '\\') {
            strbuf_append_char(&sb, (char)c);
            ps->p++;
            continue;
        }

        /* Escape Sequence */
        ps->p++;
        if (ps->p >= ps->end) {
            break;
        }
        switch (*ps->p++) {
        case '"':
            strbuf_append_char(&sb, '"');
            break;
        case '\\':
            strbuf_append_char(&sb, '\\');
            break;
        case '/':
            strbuf_append_char(&sb, '/');
            break;
        case 'b':
            strbuf_append_char(&sb, '\b');
            break;
        case 'f':
            strbuf_append_char(&sb, '\f');
            break;
        case 'n':
            strbuf_append_char(&sb, '\n');
            break;
        case 'r':
            strbuf_append_char(&sb, '\r');
            break;
        case 't':
            strbuf_append_char(&sb, '\t');
            break;
        case 'u': {
            unsigned long cp;
            unsigned long low;
            if (!read_hex4(ps, &cp)) {
                parse_error(ps, "invalid \\u escape");
                strbuf_free(&sb);
                return NULL;
            }
            if (cp >= 0xD800 && cp <= 0xDBFF) {  /* surrogate pair */
                if (ps->end - ps->p < 6 || ps->p[0] != '\\' || ps->p[1] != 'u') {
                    parse_error(ps, "unpaired surrogate in \\u escape");
                    strbuf_free(&sb);
                    return NULL;
                }
                ps->p += 2;
                if (!read_hex4(ps, &low) || low < 0xDC00 || low > 0xDFFF) {
                    parse_error(ps, "invalid low surrogate in \\u escape");
                    strbuf_free(&sb);
                    return NULL;
                }
                cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
            } else if (cp >= 0xDC00 && cp <= 0xDFFF) {
                parse_error(ps, "unpaired surrogate in \\u escape");
                strbuf_free(&sb);
                return NULL;
            }
            append_utf8(&sb, cp);
            break;
        }
        default:
            ps->p--;
            parse_error(ps, "invalid escape sequence");
            strbuf_free(&sb);
            return NULL;
        }
    }
    parse_error(ps, "unterminated string");
    strbuf_free(&sb);
    return NULL;
}

static JsonValue *parse_number(Parser *ps)
{
    const char *start = ps->p;
    const char *q = ps->p;
    char *copy;
    JsonValue *v;

    /* Number Grammar */
    if (q < ps->end && *q == '-') {
        q++;
    }
    if (q < ps->end && *q == '0') {
        q++;
    } else if (q < ps->end && *q >= '1' && *q <= '9') {
        while (q < ps->end && isdigit((unsigned char)*q)) {
            q++;
        }
    } else {
        parse_error(ps, "invalid number");
        return NULL;
    }
    if (q < ps->end && *q == '.') {
        q++;
        if (q >= ps->end || !isdigit((unsigned char)*q)) {
            ps->p = q;
            parse_error(ps, "expected digits after decimal point");
            return NULL;
        }
        while (q < ps->end && isdigit((unsigned char)*q)) {
            q++;
        }
    }
    if (q < ps->end && (*q == 'e' || *q == 'E')) {
        q++;
        if (q < ps->end && (*q == '+' || *q == '-')) {
            q++;
        }
        if (q >= ps->end || !isdigit((unsigned char)*q)) {
            ps->p = q;
            parse_error(ps, "expected digits in exponent");
            return NULL;
        }
        while (q < ps->end && isdigit((unsigned char)*q)) {
            q++;
        }
    }

    /* Conversion */
    copy = str_ndup(start, (size_t)(q - start));
    v = json_new_number(strtod(copy, NULL));
    free(copy);
    ps->p = q;
    return v;
}

static int match_word(Parser *ps, const char *word)
{
    size_t n = strlen(word);

    if ((size_t)(ps->end - ps->p) >= n && strncmp(ps->p, word, n) == 0) {
        ps->p += n;
        return 1;
    }
    return 0;
}

/* ===================================================================== */
/* Document Parsing                                                      */
/* ===================================================================== */

/* Menyisipkan nilai ke container teratas, atau menjadikannya root. */
static void attach(JsonValue **root, JsonValue **stack, size_t sp, char **pending_key, JsonValue *v)
{
    if (sp == 0) {
        *root = v;
        return;
    }
    if (stack[sp - 1]->type == JSON_ARRAY) {
        json_array_push(stack[sp - 1], v);
    } else {
        json_object_set(stack[sp - 1], *pending_key, v);
        free(*pending_key);
        *pending_key = NULL;
    }
}

JsonValue *json_parse(const char *text, size_t len, char *err, size_t errlen)
{
    Parser ps;
    JsonValue *root = NULL;
    JsonValue **stack;
    size_t sp = 0;
    size_t cap = 32;
    char *pending_key = NULL;
    ParseState state = EXPECT_VALUE;

    /* Parser Setup */
    ps.text = text;
    ps.p = text;
    ps.end = text + len;
    ps.err = err;
    ps.errlen = errlen;
    ps.failed = 0;
    ps.mark = text;
    ps.mark_line = 1;
    ps.mark_col = 1;
    if (len >= 3 && (unsigned char)text[0] == 0xEF && (unsigned char)text[1] == 0xBB &&
        (unsigned char)text[2] == 0xBF) {
        ps.p += 3;
    }
    stack = xmalloc(cap * sizeof(JsonValue *));

    /* Token Loop */
    for (;;) {
        skip_ws(&ps);
        if (state == EXPECT_COMMA_OR_CLOSE || (sp == 0 && root != NULL)) {
            char close;
            if (sp == 0) {
                break;  /* dokumen selesai */
            }
            close = stack[sp - 1]->type == JSON_ARRAY ? ']' : '}';
            if (ps.p < ps.end && *ps.p == ',') {
                ps.p++;
                state = stack[sp - 1]->type == JSON_ARRAY ? EXPECT_VALUE : EXPECT_KEY;
            } else if (ps.p < ps.end && *ps.p == close) {
                ps.p++;
                sp--;
                state = EXPECT_COMMA_OR_CLOSE;
            } else {
                parse_error(&ps, close == ']' ? "expected ',' or ']'" : "expected ',' or '}'");
                break;
            }
            continue;
        }

        if (ps.p >= ps.end) {
            parse_error(&ps, "unexpected end of input");
            break;
        }

        /* Object Key */
        if (state == EXPECT_KEY || state == EXPECT_KEY_OR_CLOSE) {
            if (state == EXPECT_KEY_OR_CLOSE && *ps.p == '}') {
                ps.p++;
                sp--;
                state = EXPECT_COMMA_OR_CLOSE;
                continue;
            }
            if (*ps.p != '"') {
                parse_error(&ps, "expected a string key");
                break;
            }
            pending_key = parse_string(&ps);
            if (pending_key == NULL) {
                break;
            }
            skip_ws(&ps);
            if (ps.p >= ps.end || *ps.p != ':') {
                parse_error(&ps, "expected ':' after object key");
                break;
            }
            ps.p++;
            state = EXPECT_VALUE;
            continue;
        }

        /* Value */
        if (state == EXPECT_VALUE_OR_CLOSE && *ps.p == ']') {
            ps.p++;
            sp--;
            state = EXPECT_COMMA_OR_CLOSE;
            continue;
        }
        {
            JsonValue *v = NULL;
            int line;
            int col;
            const char *at = ps.p;
            char c = *ps.p;

            if (c == '{' || c == '[') {
                v = json_alloc(c == '{' ? JSON_OBJECT : JSON_ARRAY);
                ps.p++;
            } else if (c == '"') {
                char *s = parse_string(&ps);
                if (s != NULL) {
                    v = json_alloc(JSON_STRING);
                    v->string = s;
                }
            } else if (c == '-' || isdigit((unsigned char)c)) {
                v = parse_number(&ps);
            } else if (match_word(&ps, "true")) {
                v = json_new_bool(1);
            } else if (match_word(&ps, "false")) {
                v = json_new_bool(0);
            } else if (match_word(&ps, "null")) {
                v = json_new_null();
            } else {
                parse_error(&ps, "expected a value");
            }
            if (v == NULL) {
                break;
            }
            position_of(&ps, at, &line, &col);
            v->line = line;
            v->column = col;
            attach(&root, stack, sp, &pending_key, v);
            if (v->type == JSON_OBJECT || v->type == JSON_ARRAY) {
                if (sp == cap) {
                    cap *= 2;
                    stack = xrealloc(stack, cap * sizeof(JsonValue *));
                }
                stack[sp++] = v;
                state = v->type == JSON_OBJECT ? EXPECT_KEY_OR_CLOSE : EXPECT_VALUE_OR_CLOSE;
            } else {
                state = EXPECT_COMMA_OR_CLOSE;
            }
        }
    }

    /* Trailing Content */
    if (!ps.failed) {
        skip_ws(&ps);
        if (ps.p < ps.end) {
            parse_error(&ps, "unexpected content after the document");
        }
    }

    free(stack);
    free(pending_key);
    if (ps.failed) {
        json_free(root);
        return NULL;
    }
    return root;
}

/* ===================================================================== */
/* Writing                                                               */
/* ===================================================================== */

static void write_string(StrBuf *sb, const char *s)
{
    const unsigned char *p;

    strbuf_append_char(sb, '"');
    for (p = (const unsigned char *)s; *p != '\0'; p++) {
        switch (*p) {
        case '"':
            strbuf_append(sb, "\\\"");
            break;
        case '\\':
            strbuf_append(sb, "\\\\");
            break;
        case '\n':
            strbuf_append(sb, "\\n");
            break;
        case '\r':
            strbuf_append(sb, "\\r");
            break;
        case '\t':
            strbuf_append(sb, "\\t");
            break;
        case '\b':
            strbuf_append(sb, "\\b");
            break;
        case '\f':
            strbuf_append(sb, "\\f");
            break;
        default:
            if (*p < 0x20) {
                strbuf_appendf(sb, "\\u%04x", (unsigned)*p);
            } else {
                strbuf_append_char(sb, (char)*p);
            }
        }
    }
    strbuf_append_char(sb, '"');
}

static void write_scalar(StrBuf *sb, const JsonValue *v)
{
    switch (v->type) {
    case JSON_NULL:
        strbuf_append(sb, "null");
        break;
    case JSON_BOOL:
        strbuf_append(sb, v->boolean ? "true" : "false");
        break;
    case JSON_NUMBER:
        if (!isfinite(v->number)) {
            strbuf_append(sb, "null");
        } else if (v->number == floor(v->number) && fabs(v->number) < 1e15) {
            strbuf_appendf(sb, "%.0f", v->number);
        } else {
            /* Shortest Round-trip Representation */
            char buf[40];
            sprintf(buf, "%.15g", v->number);
            if (strtod(buf, NULL) != v->number) {
                sprintf(buf, "%.17g", v->number);
            }
            strbuf_append(sb, buf);
        }
        break;
    case JSON_STRING:
        write_string(sb, v->string);
        break;
    default:
        break;
    }
}

static int is_scalar(const JsonValue *v)
{
    return v->type != JSON_ARRAY && v->type != JSON_OBJECT;
}

static void write_indent(StrBuf *sb, int depth)
{
    int i;

    strbuf_append_char(sb, '\n');
    for (i = 0; i < depth; i++) {
        strbuf_append(sb, "  ");
    }
}

/* Menulis nilai sederhana secara langsung. Mengembalikan 1 jika container perlu ditelusuri. */
static int write_open(StrBuf *sb, const JsonValue *v)
{
    size_t i;
    int all_scalar = 1;

    if (is_scalar(v)) {
        write_scalar(sb, v);
        return 0;
    }
    if (v->count == 0) {
        strbuf_append(sb, v->type == JSON_ARRAY ? "[]" : "{}");
        return 0;
    }
    if (v->type == JSON_ARRAY) {
        for (i = 0; i < v->count && all_scalar; i++) {
            all_scalar = is_scalar(v->items[i]);
        }
        if (all_scalar) {
            strbuf_append_char(sb, '[');
            for (i = 0; i < v->count; i++) {
                if (i > 0) {
                    strbuf_append(sb, ", ");
                }
                write_scalar(sb, v->items[i]);
            }
            strbuf_append_char(sb, ']');
            return 0;
        }
    }
    strbuf_append_char(sb, v->type == JSON_ARRAY ? '[' : '{');
    return 1;
}

char *json_write(const JsonValue *value)
{
    typedef struct {
        const JsonValue *v;
        size_t next;
        int depth;
    } Frame;
    StrBuf sb;
    Frame *stack;
    size_t sp = 0;
    size_t cap = 32;

    strbuf_init(&sb);
    stack = xmalloc(cap * sizeof(Frame));
    if (write_open(&sb, value)) {
        stack[0].v = value;
        stack[0].next = 0;
        stack[0].depth = 0;
        sp = 1;
    }

    while (sp > 0) {
        Frame *f = &stack[sp - 1];
        if (f->next < f->v->count) {
            const JsonValue *child = f->v->items[f->next];
            int depth = f->depth;
            if (f->next > 0) {
                strbuf_append_char(&sb, ',');
            }
            write_indent(&sb, depth + 1);
            if (f->v->type == JSON_OBJECT) {
                write_string(&sb, f->v->keys[f->next]);
                strbuf_append(&sb, ": ");
            }
            f->next++;
            if (write_open(&sb, child)) {
                if (sp == cap) {
                    cap *= 2;
                    stack = xrealloc(stack, cap * sizeof(Frame));
                }
                stack[sp].v = child;
                stack[sp].next = 0;
                stack[sp].depth = depth + 1;
                sp++;
            }
        } else {
            write_indent(&sb, f->depth);
            strbuf_append_char(&sb, f->v->type == JSON_ARRAY ? ']' : '}');
            sp--;
        }
    }

    free(stack);
    strbuf_append_char(&sb, '\n');
    return strbuf_steal(&sb);
}
