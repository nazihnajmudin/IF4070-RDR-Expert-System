/**
 * @file rdr_print.c
 * @brief Implementasi render pohon RDR menjadi teks Unicode.
 */

#include "rdr/rdr_print.h"

#include <stdlib.h>
#include <string.h>

#include "rdr/query.h"

/* Box drawing characters in UTF-8 */
#define BRANCH_MID  "\xE2\x94\x9C\xE2\x94\x80"   /* ├─ */
#define BRANCH_LAST "\xE2\x94\x94\xE2\x94\x80"   /* └─ */
#define EDGE_DASH   "\xE2\x94\x80 "              /* ─ dan spasi */
#define PIPE_INDENT "\xE2\x94\x82    "           /* │ dan empat spasi */
#define BLANK_INDENT "     "
#define PIPE_SHORT  "\xE2\x94\x82 "              /* │ dan spasi */

/**
 * @brief Frame penelusuran iteratif.
 */
typedef struct {
    const RdrNode *node;  /**< Node, NULL untuk slot kosong. */
    int parent_id;        /**< Id induk, -1 untuk root. */
    int edge;             /**< -1 root, 0 true, 1 false. */
    int is_last;          /**< 1 jika anak terakhir yang ditampilkan dari induknya. */
    char *indent;         /**< Indentasi milik induk (milik frame). */
} Frame;

void rdr_print_default_options(RdrPrintOptions *opt)
{
    opt->show_empty_slots = 1;
    opt->show_node_id = 1;
    opt->show_cornerstone = 0;
}

char *rdr_node_label(const RdrNode *node, int show_id)
{
    StrBuf sb;
    char *rule = clauselist_rule_text(&node->rule);

    strbuf_init(&sb);
    if (show_id) {
        strbuf_appendf(&sb, "#%d ", node->id);
    }
    strbuf_appendf(&sb, "(%s >> %s)", rule, node->conclusion);
    if (node->revises != RDR_NO_REVISION) {
        strbuf_appendf(&sb, " [revises #%d]", node->revises);
    }
    free(rule);
    return strbuf_steal(&sb);
}

void rdr_lines_init(RdrLines *lines)
{
    lines->items = NULL;
    lines->count = 0;
    lines->cap = 0;
}

void rdr_lines_free(RdrLines *lines)
{
    size_t i;

    for (i = 0; i < lines->count; i++) {
        free(lines->items[i].prefix);
        free(lines->items[i].text);
    }
    free(lines->items);
    rdr_lines_init(lines);
}

static void push_line(RdrLines *lines, RdrLineKind kind, char *prefix, char *text,
                      int node_id, int parent_id, int edge)
{
    RdrLine *l;

    if (lines->count == lines->cap) {
        lines->cap = lines->cap ? lines->cap * 2 : 32;
        lines->items = xrealloc(lines->items, lines->cap * sizeof(RdrLine));
    }
    l = &lines->items[lines->count++];
    l->kind = kind;
    l->prefix = prefix;
    l->text = text;
    l->node_id = node_id;
    l->parent_id = parent_id;
    l->edge = edge;
}

static char *concat(const char *a, const char *b)
{
    return str_printf("%s%s", a, b);
}

/* Menghitung jumlah anak yang akan ditampilkan untuk sebuah node. */
static int visible_children(const RdrNode *n, const RdrPrintOptions *opt)
{
    if (opt->show_empty_slots) {
        return 2;
    }
    return (n->true_child != NULL) + (n->false_child != NULL);
}

void rdr_print_tree(const RdrKb *kb, const RdrPrintOptions *opt, RdrLines *out)
{
    Frame *stack;
    size_t sp = 0;
    size_t cap = 64;

    rdr_lines_free(out);
    if (kb == NULL || kb->root == NULL) {
        return;
    }
    stack = xmalloc(cap * sizeof(Frame));
    stack[sp].node = kb->root;
    stack[sp].parent_id = -1;
    stack[sp].edge = -1;
    stack[sp].is_last = 1;
    stack[sp].indent = str_dup("");
    sp++;

    while (sp > 0) {
        Frame f = stack[--sp];
        char *prefix;
        char *child_indent;
        const RdrNode *n = f.node;

        /* Line Prefix */
        if (f.edge < 0) {
            prefix = str_dup("");
            child_indent = str_dup("");
        } else {
            prefix = str_printf("%s%s%c%s", f.indent, f.is_last ? BRANCH_LAST : BRANCH_MID,
                                f.edge == 0 ? 'T' : 'F', EDGE_DASH);
            child_indent = concat(f.indent, f.is_last ? BLANK_INDENT : PIPE_INDENT);
        }

        /* Empty Slot */
        if (n == NULL) {
            push_line(out, RDR_LINE_EMPTY, prefix, str_dup(RDR_EMPTY_SLOT_SYMBOL), -1,
                      f.parent_id, f.edge);
            free(child_indent);
            free(f.indent);
            continue;
        }

        /* Node Line */
        push_line(out, RDR_LINE_NODE, prefix, rdr_node_label(n, opt->show_node_id), n->id,
                  f.parent_id, f.edge);
        if (opt->show_cornerstone && n != kb->root) {
            char *cs = n->cornerstone.count ? query_format(&n->cornerstone) : str_dup("(none)");
            push_line(out, RDR_LINE_CORNERSTONE,
                      concat(child_indent, visible_children(n, opt) ? PIPE_SHORT : "  "),
                      str_printf("cornerstone: %s", cs), n->id, f.parent_id, f.edge);
            free(cs);
        }

        /* Child Frames */
        if (sp + 2 > cap) {
            cap *= 2;
            stack = xrealloc(stack, cap * sizeof(Frame));
        }
        {
            int show_true = opt->show_empty_slots || n->true_child != NULL;
            int show_false = opt->show_empty_slots || n->false_child != NULL;
            if (show_false) {  /* false didorong dulu agar true dicetak lebih awal */
                stack[sp].node = n->false_child;
                stack[sp].parent_id = n->id;
                stack[sp].edge = 1;
                stack[sp].is_last = 1;
                stack[sp].indent = str_dup(child_indent);
                sp++;
            }
            if (show_true) {
                stack[sp].node = n->true_child;
                stack[sp].parent_id = n->id;
                stack[sp].edge = 0;
                stack[sp].is_last = !show_false;
                stack[sp].indent = str_dup(child_indent);
                sp++;
            }
        }
        free(child_indent);
        free(f.indent);
    }
    free(stack);
}

char *rdr_lines_join(const RdrLines *lines)
{
    StrBuf sb;
    size_t i;

    strbuf_init(&sb);
    for (i = 0; i < lines->count; i++) {
        strbuf_append(&sb, lines->items[i].prefix);
        strbuf_append(&sb, lines->items[i].text);
        strbuf_append_char(&sb, '\n');
    }
    return strbuf_steal(&sb);
}
