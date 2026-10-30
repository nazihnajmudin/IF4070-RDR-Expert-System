/**
 * @file numset.c
 * @brief Implementasi ADT NumSet.
 */

#include "rdr/numset.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* ===================================================================== */
/* Interval Helpers                                                      */
/* ===================================================================== */

static Interval make_interval(double lo, int lo_closed, double hi, int hi_closed)
{
    Interval iv;

    iv.lo = lo;
    iv.hi = hi;
    iv.lo_closed = isinf(lo) ? 0 : lo_closed;
    iv.hi_closed = isinf(hi) ? 0 : hi_closed;
    return iv;
}

static int interval_is_empty(const Interval *iv)
{
    if (iv->lo > iv->hi) {
        return 1;
    }
    if (iv->lo == iv->hi) {
        return !(iv->lo_closed && iv->hi_closed);
    }
    return 0;
}

static Interval interval_intersect(const Interval *a, const Interval *b)
{
    Interval r;

    /* Lower Bound */
    if (a->lo > b->lo) {
        r.lo = a->lo;
        r.lo_closed = a->lo_closed;
    } else if (a->lo < b->lo) {
        r.lo = b->lo;
        r.lo_closed = b->lo_closed;
    } else {
        r.lo = a->lo;
        r.lo_closed = a->lo_closed && b->lo_closed;
    }

    /* Upper Bound */
    if (a->hi < b->hi) {
        r.hi = a->hi;
        r.hi_closed = a->hi_closed;
    } else if (a->hi > b->hi) {
        r.hi = b->hi;
        r.hi_closed = b->hi_closed;
    } else {
        r.hi = a->hi;
        r.hi_closed = a->hi_closed && b->hi_closed;
    }
    return r;
}

/* Memeriksa apakah interval inner seluruhnya berada di dalam outer. */
static int interval_contains(const Interval *outer, const Interval *inner)
{
    int lo_ok = outer->lo < inner->lo ||
                (outer->lo == inner->lo && (outer->lo_closed || !inner->lo_closed));
    int hi_ok = outer->hi > inner->hi ||
                (outer->hi == inner->hi && (outer->hi_closed || !inner->hi_closed));
    return lo_ok && hi_ok;
}

static int interval_compare(const void *pa, const void *pb)
{
    const Interval *a = pa;
    const Interval *b = pb;

    if (a->lo < b->lo) {
        return -1;
    }
    if (a->lo > b->lo) {
        return 1;
    }
    return b->lo_closed - a->lo_closed;  /* batas tertutup didahulukan */
}

/* ===================================================================== */
/* Set Construction                                                      */
/* ===================================================================== */

static void numset_push(NumSet *set, Interval iv)
{
    if (interval_is_empty(&iv)) {
        return;
    }
    if (set->count == set->cap) {
        set->cap = set->cap ? set->cap * 2 : 4;
        set->items = xrealloc(set->items, set->cap * sizeof(Interval));
    }
    set->items[set->count++] = iv;
}

/* Mengurutkan dan menggabungkan interval yang tumpang tindih atau bersentuhan. */
static void numset_normalize(NumSet *set)
{
    size_t i;
    size_t w = 0;

    if (set->count < 2) {
        return;
    }
    qsort(set->items, set->count, sizeof(Interval), interval_compare);
    for (i = 1; i < set->count; i++) {
        Interval *cur = &set->items[w];
        const Interval *next = &set->items[i];
        int touches = next->lo < cur->hi ||
                      (next->lo == cur->hi && (next->lo_closed || cur->hi_closed));
        if (touches) {
            if (next->hi > cur->hi) {
                cur->hi = next->hi;
                cur->hi_closed = next->hi_closed;
            } else if (next->hi == cur->hi) {
                cur->hi_closed = cur->hi_closed || next->hi_closed;
            }
        } else {
            set->items[++w] = *next;
        }
    }
    set->count = w + 1;
}

void numset_init_empty(NumSet *set)
{
    set->items = NULL;
    set->count = 0;
    set->cap = 0;
}

void numset_init_all(NumSet *set)
{
    numset_init_empty(set);
    numset_push(set, make_interval(-INFINITY, 0, INFINITY, 0));
}

void numset_init_from_op(NumSet *set, ClauseOp op, double value)
{
    numset_init_empty(set);
    switch (op) {
    case OP_EQ:
        numset_push(set, make_interval(value, 1, value, 1));
        break;
    case OP_NE:
        numset_push(set, make_interval(-INFINITY, 0, value, 0));
        numset_push(set, make_interval(value, 0, INFINITY, 0));
        break;
    case OP_GT:
        numset_push(set, make_interval(value, 0, INFINITY, 0));
        break;
    case OP_LT:
        numset_push(set, make_interval(-INFINITY, 0, value, 0));
        break;
    case OP_GE:
        numset_push(set, make_interval(value, 1, INFINITY, 0));
        break;
    case OP_LE:
        numset_push(set, make_interval(-INFINITY, 0, value, 1));
        break;
    }
}

void numset_free(NumSet *set)
{
    free(set->items);
    numset_init_empty(set);
}

void numset_copy(NumSet *dst, const NumSet *src)
{
    if (dst == src) {
        return;
    }
    dst->count = 0;
    if (src->count > dst->cap) {
        dst->cap = src->count;
        dst->items = xrealloc(dst->items, dst->cap * sizeof(Interval));
    }
    if (src->count > 0) {
        memcpy(dst->items, src->items, src->count * sizeof(Interval));
    }
    dst->count = src->count;
}

/* ===================================================================== */
/* Set Operations                                                        */
/* ===================================================================== */

void numset_intersect(const NumSet *a, const NumSet *b, NumSet *out)
{
    NumSet result;
    size_t i;
    size_t j;

    numset_init_empty(&result);
    for (i = 0; i < a->count; i++) {
        for (j = 0; j < b->count; j++) {
            numset_push(&result, interval_intersect(&a->items[i], &b->items[j]));
        }
    }
    numset_normalize(&result);
    numset_free(out);
    *out = result;
}

void numset_intersect_op(NumSet *set, ClauseOp op, double value)
{
    NumSet other;

    numset_init_from_op(&other, op, value);
    numset_intersect(set, &other, set);
    numset_free(&other);
}

int numset_is_empty(const NumSet *set)
{
    return set->count == 0;
}

int numset_is_subset(const NumSet *a, const NumSet *b)
{
    size_t i;
    size_t j;

    for (i = 0; i < a->count; i++) {
        int contained = 0;
        for (j = 0; j < b->count && !contained; j++) {
            contained = interval_contains(&b->items[j], &a->items[i]);
        }
        if (!contained) {
            return 0;
        }
    }
    return 1;
}

static void format_bound(StrBuf *sb, double v)
{
    if (isinf(v)) {
        strbuf_append(sb, v < 0 ? "-inf" : "+inf");
    } else {
        strbuf_appendf(sb, "%g", v);
    }
}

void numset_format(const NumSet *set, StrBuf *sb)
{
    size_t i;

    if (set->count == 0) {
        strbuf_append(sb, "{}");
        return;
    }
    for (i = 0; i < set->count; i++) {
        const Interval *iv = &set->items[i];
        if (i > 0) {
            strbuf_append(sb, " U ");
        }
        if (iv->lo == iv->hi) {
            strbuf_append_char(sb, '{');
            format_bound(sb, iv->lo);
            strbuf_append_char(sb, '}');
            continue;
        }
        strbuf_append_char(sb, iv->lo_closed ? '[' : '(');
        format_bound(sb, iv->lo);
        strbuf_append(sb, ", ");
        format_bound(sb, iv->hi);
        strbuf_append_char(sb, iv->hi_closed ? ']' : ')');
    }
}
