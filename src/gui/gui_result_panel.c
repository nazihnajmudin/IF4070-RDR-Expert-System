/**
 * @file gui_result_panel.c
 * @brief Panel hasil (tab Result dan Tree).
 *
 * Kedua tab memakai tampilan teks read-only (gui_textview.c) sehingga isinya
 * dapat diseleksi dan disalin. Tab Result menampilkan kesimpulan akhir dan
 * tabel dua kolom berisi kesimpulan dan rule penghasilnya.
 */

#include "gui.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "rdr/query.h"

static const char *const RESULT_TABS[] = {"Result", "Tree"};

/** Simbol centang untuk kesimpulan akhir. */
#define CHECK_MARK "\xE2\x9C\x94"

/* ===================================================================== */
/* Text Helpers                                                          */
/* ===================================================================== */

static int columns_of(const char *s)
{
    int n = 0;

    for (; *s != '\0'; s++) {
        if (((unsigned char)*s & 0xC0) != 0x80) {
            n++;
        }
    }
    return n;
}

/* Menambahkan teks dengan spasi sampai width kolom. */
static void append_padded(StrBuf *sb, const char *text, int width)
{
    int n = columns_of(text);

    strbuf_append(sb, text);
    while (n++ < width) {
        strbuf_append_char(sb, ' ');
    }
}

static GuiTextLine line_style(Gui *gui, struct nk_color color, int font, float extra)
{
    GuiTextLine st;

    memset(&st, 0, sizeof(st));
    st.split = -1;
    st.font1 = font;
    st.font2 = font;
    st.color1 = color;
    st.color2 = color;
    st.height = gui_font(gui, font)->height + extra;
    st.node_id = -1;
    return st;
}

/* Membungkus teks per kata menjadi beberapa baris selebar cols kolom. */
static void add_wrapped(Gui *gui, GuiTextDoc *doc, const GuiTextLine *st, const char *text, int cols)
{
    StrBuf line;
    const char *p = text;
    int used = 0;

    strbuf_init(&line);
    while (*p != '\0') {
        const char *word_end = p;
        int wcols;
        char *word;
        while (*word_end != '\0' && *word_end != ' ') {
            word_end++;
        }
        word = str_ndup(p, (size_t)(word_end - p));
        wcols = columns_of(word);
        if (used > 0 && used + 1 + wcols > cols) {
            gui_textdoc_add(gui, doc, st, strbuf_cstr(&line));
            strbuf_clear(&line);
            used = 0;
        }
        if (used > 0) {
            strbuf_append_char(&line, ' ');
            used++;
        }
        strbuf_append(&line, word);
        used += wcols;
        free(word);
        p = *word_end == ' ' ? word_end + 1 : word_end;
    }
    gui_textdoc_add(gui, doc, st, strbuf_cstr(&line));
    strbuf_free(&line);
}

/* ===================================================================== */
/* Result Document                                                       */
/* ===================================================================== */

static void build_result_doc(Gui *gui, float width)
{
    App *app = gui->app;
    const RdrInference *inf = &app->inference;
    GuiTextDoc *doc = &gui->result_doc;
    const struct nk_user_font *f = &gui->font->handle;
    float cw = f->width(f->userdata, f->height, "M", 1);
    int cols = (int)((width - gui->ctx.style.window.scrollbar_size.x - 28.0f) / cw);
    int show_rule = prefs_get(&app->prefs, PREF_RESULT_SHOW_RULE);
    GuiTextLine muted = line_style(gui, gui->pal.muted, GUI_FONT_REGULAR, 6);
    GuiTextLine gap = line_style(gui, gui->pal.muted, GUI_FONT_REGULAR, 0);
    size_t i;

    if (gui->result_valid && gui->result_revision == app->revision && prefs_equal(&gui->result_prefs, &app->prefs) &&
        gui->result_font_size == gui->font_size && fabsf(gui->result_wrap - width) < 1.0f) {
        return;
    }
    gui_textdoc_clear(doc);
    if (cols < 16) {
        cols = 16;
    }
    gap.height = 8;

    if (!app_has_result(app)) {
        add_wrapped(gui, doc, &muted, "No result yet. Add clauses to the query and press Start (F5).", cols);
    } else {
        GuiTextLine head = line_style(gui, gui->pal.conclusion, GUI_FONT_LARGE, 14);
        GuiTextLine row = line_style(gui, gui->pal.text, GUI_FONT_REGULAR, 6);
        int colw = columns_of("Conclusion");
        char *text;
        char *query;

        /* Final Conclusion */
        head.bg = gui->pal.conclusion_bg;
        head.bg_full = 1;
        text = str_printf(CHECK_MARK " %s", app_conclusion(app));
        gui_textdoc_add(gui, doc, &head, text);
        free(text);
        gui_textdoc_add(gui, doc, &gap, "");

        /* Conclusion Table */
        for (i = 0; i < inf->trace.count; i++) {
            const RdrNode *n = rdr_kb_find(app->kb, inf->trace.items[i]);
            int c = columns_of(n->conclusion);
            colw = c > colw ? c : colw;
        }
        colw += 4;
        {
            GuiTextLine header = line_style(gui, gui->pal.muted, GUI_FONT_BOLD, 6);
            StrBuf sb;
            strbuf_init(&sb);
            append_padded(&sb, "Conclusion", colw);
            if (show_rule) {
                strbuf_append(&sb, "Rule");
            }
            header.split = colw;
            gui_textdoc_add(gui, doc, &header, strbuf_cstr(&sb));
            strbuf_free(&sb);
        }
        for (i = inf->trace.count; i-- > 0;) {
            const RdrNode *n = rdr_kb_find(app->kb, inf->trace.items[i]);
            int is_final = i + 1 == inf->trace.count;
            StrBuf sb;
            strbuf_init(&sb);
            append_padded(&sb, n->conclusion, colw);
            if (show_rule) {
                char *rule = clauselist_rule_text(&n->rule);
                strbuf_appendf(&sb, "#%d  %s", n->id, rule);
                free(rule);
            }
            row.split = colw;
            row.font1 = is_final ? GUI_FONT_BOLD : GUI_FONT_REGULAR;
            row.color1 = is_final ? gui->pal.conclusion : gui->pal.text;
            row.color2 = gui->pal.muted;
            gui_textdoc_add(gui, doc, &row, strbuf_cstr(&sb));
            strbuf_free(&sb);
        }

        /* Query and Hint */
        gui_textdoc_add(gui, doc, &gap, "");
        query = query_format(&inf->query);
        text = str_printf("Query: %s", inf->query.count ? query : "(empty)");
        add_wrapped(gui, doc, &muted, text, cols);
        free(text);
        free(query);
        gui_textdoc_add(gui, doc, &gap, "");
        add_wrapped(gui, doc, &muted, "Not the right conclusion? Use Edit > Edit last query to teach a new rule.",
                    cols);
    }

    gui->result_revision = app->revision;
    gui->result_prefs = app->prefs;
    gui->result_font_size = gui->font_size;
    gui->result_wrap = width;
    gui->result_valid = 1;
}

/* ===================================================================== */
/* Tree Document                                                         */
/* ===================================================================== */

struct nk_color gui_tree_line_color(Gui *gui, const RdrLine *line, int *is_conclusion)
{
    App *app = gui->app;
    RdrNodeStatus st;
    size_t i;
    size_t visible_steps = app->inference.step_count;

    *is_conclusion = 0;
    if (line->kind != RDR_LINE_NODE) {
        return gui->pal.muted;
    }
    if (!prefs_get(&app->prefs, PREF_TREE_HIGHLIGHT) || !app_has_result(app)) {
        return gui->pal.text;
    }
    if (gui->reasoning_revision == app->revision) {
        visible_steps = gui->reasoning_step;
    }
    st = RDR_NODE_IDLE;
    for (i = 0; i < visible_steps; i++) {
        if (app->inference.steps[i].node_id == line->node_id) {
            st = app->inference.steps[i].fired ? RDR_NODE_FIRED : RDR_NODE_FAILED;
        }
    }
    if (!gui->reasoning_active && line->node_id == app->inference.last_concluded) {
        *is_conclusion = 1;
        return gui->pal.conclusion;
    }
    if (st == RDR_NODE_FIRED) {
        return gui->pal.fired;
    }
    if (st == RDR_NODE_FAILED) {
        return gui->pal.failed;
    }
    return gui->pal.text;
}

void gui_tree_refresh(Gui *gui)
{
    App *app = gui->app;
    RdrPrintOptions opt;
    GuiTextLine st;
    size_t i;

    if (gui->tree_valid && gui->tree_revision == app->revision && prefs_equal(&gui->tree_prefs, &app->prefs) &&
        gui->tree_font_size == gui->font_size) {
        return;
    }

    /* Line Structure */
    opt.show_empty_slots = prefs_get(&app->prefs, PREF_TREE_SHOW_EMPTY_SLOTS);
    opt.show_node_id = prefs_get(&app->prefs, PREF_TREE_SHOW_NODE_ID);
    opt.show_cornerstone = prefs_get(&app->prefs, PREF_TREE_SHOW_CORNERSTONE);
    rdr_print_tree(app->kb, &opt, &gui->tree);
    gui->tree_width = 0;
    for (i = 0; i < gui->tree.count; i++) {
        float w = gui_text_width(gui, gui->tree.items[i].prefix) + gui_text_width(gui, gui->tree.items[i].text);
        gui->tree_width = w > gui->tree_width ? w : gui->tree_width;
    }

    /* Colored Document */
    gui_textdoc_clear(&gui->tree_doc);
    st = line_style(gui, gui->pal.muted, GUI_FONT_REGULAR, 0);
    st.font1 = GUI_FONT_TREE;
    st.color1 = gui->pal.accent;
    st.height = (float)(int)gui->font->handle.height;
    for (i = 0; i < gui->tree.count; i++) {
        const RdrLine *l = &gui->tree.items[i];
        int is_conclusion;
        char *text = str_printf("%s%s", l->prefix, l->text);
        GuiTextLine *line;
        st.color2 = gui_tree_line_color(gui, l, &is_conclusion);
        st.font2 = is_conclusion ? GUI_FONT_BOLD : GUI_FONT_REGULAR;
        st.bg = is_conclusion ? gui->pal.conclusion_bg : nk_rgba(0, 0, 0, 0);
        line = gui_textdoc_add(gui, &gui->tree_doc, &st, text);
        line->split = columns_of(l->prefix);
        line->node_id = l->kind == RDR_LINE_NODE ? l->node_id : -1;
        free(text);
    }

    gui->tree_revision = app->revision;
    gui->tree_prefs = app->prefs;
    gui->tree_font_size = gui->font_size;
    gui->tree_valid = 1;
}

/* ===================================================================== */
/* Panel Layout                                                          */
/* ===================================================================== */

/* Menggambar legenda warna rata kanan pada baris tab jika masih ada ruang setelah tab. */
static void legend(Gui *gui, struct nk_rect area, float tabs_end)
{
    struct nk_context *ctx = &gui->ctx;
    static const char *const LABELS[] = {"conclusion", "fired", "failed"};
    struct nk_color colors[3];
    float x = area.x + area.w;
    int i;

    colors[0] = gui->pal.conclusion;
    colors[1] = gui->pal.fired;
    colors[2] = gui->pal.failed;
    {
        float total = 2 * 14;
        for (i = 0; i < 3; i++) {
            total += (float)(int)(gui_text_width(gui, LABELS[i]) + 2);
        }
        if (area.x + area.w - total < tabs_end + 16) {
            return;  /* panel terlalu sempit, legenda tidak boleh menimpa tab */
        }
    }
    for (i = 2; i >= 0; i--) {
        float w = (float)(int)(gui_text_width(gui, LABELS[i]) + 2);
        x -= w;
        nk_layout_space_push(ctx, nk_rect(x, area.y, w, gui->row));
        if (i == 0) {
            gui_label_bold(gui, LABELS[i], colors[i]);
        } else {
            gui_label_colored(gui, LABELS[i], colors[i]);
        }
        x -= 14;
    }
}

void gui_result_panel(Gui *gui, struct nk_rect area, float body_y)
{
    App *app = gui->app;
    struct nk_rect body = nk_rect(area.x, body_y, area.w, area.y + area.h - body_y);
    float x = area.x;
    int i;

    /* Tab Bar */
    for (i = 0; i < 2; i++) {
        float w = gui_tab_width(gui, RESULT_TABS[i]);
        if (gui_tab_at(gui, nk_rect(x, area.y, w, gui->row), RESULT_TABS[i], 0, i == app->result_tab)) {
            app->result_tab = i;
        }
        x += w + 4;
    }
    if (app->result_tab == SESSION_RESULT_TAB_TREE && app_has_result(app) &&
        prefs_get(&app->prefs, PREF_TREE_HIGHLIGHT)) {
        legend(gui, area, x);
    }

    /* Tab Body */
    nk_layout_space_push(&gui->ctx, body);
    if (app->result_tab == SESSION_RESULT_TAB_TREE) {
        GuiTextEvent ev;
        gui_tree_refresh(gui);
        ev = gui_textview(gui, "tree_view", &gui->tree_doc, &gui->tree_sel, GUI_FOCUS_TREE);
        if (ev.double_clicked_line >= 0 && gui->tree_doc.items[ev.double_clicked_line].node_id >= 0) {
            gui_action_manual(gui, gui->tree_doc.items[ev.double_clicked_line].node_id);
        }
    } else {
        build_result_doc(gui, body.w);
        gui_textview(gui, "result_view", &gui->result_doc, &gui->result_sel, GUI_FOCUS_RESULT);
    }
}
