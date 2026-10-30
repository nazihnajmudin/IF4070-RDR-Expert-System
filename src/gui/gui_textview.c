/**
 * @file gui_textview.c
 * @brief Tampilan teks read-only yang dapat diseleksi dan disalin.
 *
 * Dipakai oleh tab Result dan Tree. Tampilan tidak menyerupai editor (tanpa
 * kursor ketik), tetapi teksnya dapat diseleksi dengan drag mouse atau
 * Shift+klik, diseleksi seluruhnya dengan Ctrl+A, dan disalin dengan Ctrl+C.
 */

#include "gui.h"

#include <stdlib.h>
#include <string.h>

/* ===================================================================== */
/* UTF-8 Helpers                                                         */
/* ===================================================================== */

static int utf8_columns(const char *s)
{
    int n = 0;

    for (; *s != '\0'; s++) {
        if (((unsigned char)*s & 0xC0) != 0x80) {
            n++;
        }
    }
    return n;
}

/* Mengembalikan offset byte untuk kolom (codepoint) tertentu. */
static size_t utf8_offset(const char *s, int column)
{
    const char *p = s;
    int col = 0;

    while (*p != '\0') {
        if (((unsigned char)*p & 0xC0) != 0x80) {
            if (col == column) {
                break;
            }
            col++;
        }
        p++;
    }
    return (size_t)(p - s);
}

/* ===================================================================== */
/* Document                                                              */
/* ===================================================================== */

void gui_textdoc_init(GuiTextDoc *doc)
{
    memset(doc, 0, sizeof(*doc));
}

void gui_textdoc_clear(GuiTextDoc *doc)
{
    size_t i;

    for (i = 0; i < doc->count; i++) {
        free(doc->items[i].text);
    }
    doc->count = 0;
    doc->width = 0;
}

void gui_textdoc_free(GuiTextDoc *doc)
{
    gui_textdoc_clear(doc);
    free(doc->items);
    gui_textdoc_init(doc);
}

const struct nk_user_font *gui_font(Gui *gui, int which)
{
    if (which == GUI_FONT_BOLD) {
        return &gui->font_bold->handle;
    }
    if (which == GUI_FONT_LARGE) {
        return &gui->font_large->handle;
    }
    if (which == GUI_FONT_TREE) {
        return &gui->font_tree->handle;
    }
    return &gui->font->handle;
}

static float char_width(Gui *gui, int font)
{
    const struct nk_user_font *f = gui_font(gui, font);
    return f->width(f->userdata, f->height, "M", 1);
}

GuiTextLine *gui_textdoc_add(Gui *gui, GuiTextDoc *doc, const GuiTextLine *style, const char *text)
{
    GuiTextLine *line;
    float w;

    if (doc->count == doc->cap) {
        doc->cap = doc->cap ? doc->cap * 2 : 64;
        doc->items = xrealloc(doc->items, doc->cap * sizeof(GuiTextLine));
    }
    line = &doc->items[doc->count++];
    *line = *style;
    line->text = str_dup(text);
    line->columns = utf8_columns(text);
    w = (float)line->columns * char_width(gui, line->font1) + 12.0f;
    if (w > doc->width) {
        doc->width = w;
    }
    return line;
}

void gui_textsel_reset(GuiTextSel *sel)
{
    sel->anchor_line = sel->cursor_line = 0;
    sel->anchor_col = sel->cursor_col = 0;
    sel->dragging = 0;
}

/* ===================================================================== */
/* Selection Helpers                                                     */
/* ===================================================================== */

/* Mengurutkan titik seleksi menjadi (l1, c1) sebelum (l2, c2). */
static void ordered(const GuiTextSel *sel, int *l1, int *c1, int *l2, int *c2)
{
    int before = sel->anchor_line < sel->cursor_line ||
                 (sel->anchor_line == sel->cursor_line && sel->anchor_col <= sel->cursor_col);

    *l1 = before ? sel->anchor_line : sel->cursor_line;
    *c1 = before ? sel->anchor_col : sel->cursor_col;
    *l2 = before ? sel->cursor_line : sel->anchor_line;
    *c2 = before ? sel->cursor_col : sel->anchor_col;
}

static int has_selection(const GuiTextSel *sel)
{
    return sel->anchor_line != sel->cursor_line || sel->anchor_col != sel->cursor_col;
}

static void copy_selection(Gui *gui, const GuiTextDoc *doc, const GuiTextSel *sel)
{
    StrBuf sb;
    int l1;
    int c1;
    int l2;
    int c2;
    int i;

    if (!has_selection(sel) || doc->count == 0) {
        return;
    }
    ordered(sel, &l1, &c1, &l2, &c2);
    strbuf_init(&sb);
    for (i = l1; i <= l2 && (size_t)i < doc->count; i++) {
        const char *t = doc->items[i].text;
        size_t from = i == l1 ? utf8_offset(t, c1) : 0;
        size_t to = i == l2 ? utf8_offset(t, c2) : strlen(t);
        if (to > from) {
            strbuf_append_n(&sb, t + from, to - from);
        }
        if (i < l2) {
            strbuf_append_char(&sb, '\n');
        }
    }
    glfwSetClipboardString(gui->window, strbuf_cstr(&sb));
    strbuf_free(&sb);
}

/* ===================================================================== */
/* Line Drawing                                                          */
/* ===================================================================== */

static void draw_line(Gui *gui, const GuiTextLine *line, struct nk_rect b, int index, const GuiTextSel *sel,
                      int focused, float view_w)
{
    struct nk_command_buffer *canvas = nk_window_get_canvas(&gui->ctx);
    const struct nk_user_font *f1 = gui_font(gui, line->font1);
    const struct nk_user_font *f2 = gui_font(gui, line->font2);
    float cw = char_width(gui, line->font1);
    float x = b.x + 6.0f;
    float ty = b.y + (b.h - f1->height) / 2.0f;
    struct nk_color clear = nk_rgba(0, 0, 0, 0);
    int split = (line->split >= 0 && line->split <= line->columns) ? line->split : line->columns;

    /* Background */
    if (line->bg.a > 0) {
        if (line->bg_full) {
            nk_fill_rect(canvas, nk_rect(b.x + 2, b.y + 1, (view_w > b.w ? view_w : b.w) - 8, b.h - 2), 4, line->bg);
        } else if (split < line->columns) {
            nk_fill_rect(canvas, nk_rect(x + (float)split * cw - 3, b.y, (float)(line->columns - split) * cw + 6, b.h),
                         3, line->bg);
        }
    }

    /* Selection Highlight */
    if (has_selection(sel)) {
        int l1;
        int c1;
        int l2;
        int c2;
        ordered(sel, &l1, &c1, &l2, &c2);
        if (index >= l1 && index <= l2) {
            int from = index == l1 ? c1 : 0;
            int to = index == l2 ? c2 : line->columns + 1;
            struct nk_color hl = gui->ctx.style.selectable.normal_active.data.color;
            if (!focused) {
                hl.a = 140;
            }
            if (to > from) {
                nk_fill_rect(canvas, nk_rect(x + (float)from * cw, b.y, (float)(to - from) * cw, b.h), 0, hl);
            }
        }
    }

    /* Visible Columns */
    {
        struct nk_rect clip = canvas->clip;
        int first = (int)((clip.x - x) / cw) - 1;
        int last = (int)((clip.x + clip.w - x) / cw) + 2;
        first = first < 0 ? 0 : first;
        last = last > line->columns ? line->columns : last;
        if (first >= last) {
            return;
        }

        /* Text Segments */
        if (first < split) {
            int to = last < split ? last : split;
            size_t from_b = utf8_offset(line->text, first);
            size_t to_b = utf8_offset(line->text, to);
            nk_draw_text(canvas, nk_rect(x + (float)first * cw, ty, (float)(to - first) * cw + 2, f1->height),
                         line->text + from_b, (int)(to_b - from_b), f1, clear, line->color1);
        }
        if (last > split) {
            int from = first > split ? first : split;
            size_t from_b = utf8_offset(line->text, from);
            size_t to_b = utf8_offset(line->text, last);
            nk_draw_text(canvas, nk_rect(x + (float)from * cw, ty + (f1->height - f2->height) / 2.0f,
                                         (float)(last - from) * cw + 2, f2->height),
                         line->text + from_b, (int)(to_b - from_b), f2, clear, line->color2);
        }
    }
}

/* ===================================================================== */
/* View                                                                  */
/* ===================================================================== */

GuiTextEvent gui_textview(Gui *gui, const char *id, GuiTextDoc *doc, GuiTextSel *sel, GuiFocus focus_id)
{
    struct nk_context *ctx = &gui->ctx;
    const struct nk_input *in = &ctx->input;
    struct nk_rect area = nk_widget_bounds(ctx);
    struct nk_vec2 saved_spacing = ctx->style.window.spacing;
    float scroll = ctx->style.window.scrollbar_size.x;
    int interactive = !(ctx->current->flags & NK_WINDOW_ROM);
    int pressed = interactive && nk_input_is_mouse_pressed(in, NK_BUTTON_LEFT);
    int inside = NK_INBOX(in->mouse.pos.x, in->mouse.pos.y, area.x, area.y, area.w - scroll - 2, area.h - scroll - 2);
    float probe_y;
    int hit_line = -1;
    int hit_col = 0;
    GuiTextEvent ev;
    size_t i;

    ev.clicked_line = -1;
    ev.double_clicked_line = -1;

    /* Focus Handling */
    if (pressed) {
        if (inside) {
            gui->text_focus = focus_id;
        } else if (gui->text_focus == focus_id) {
            gui->text_focus = GUI_FOCUS_NONE;
        }
    }

    /* Line Layout and Hit Test */
    probe_y = in->mouse.pos.y;
    if (probe_y < area.y + 2) {
        probe_y = area.y + 2;
    }
    if (probe_y > area.y + area.h - scroll - 3) {
        probe_y = area.y + area.h - scroll - 3;
    }
    ctx->style.window.fixed_background = nk_style_item_color(gui->pal.panel);
    ctx->style.window.spacing = nk_vec2(0, 0);
    if (nk_group_begin(ctx, id, NK_WINDOW_BORDER)) {
        float row_w = doc->width > area.w - scroll - 16 ? doc->width : area.w - scroll - 16;
        for (i = 0; i < doc->count; i++) {
            const GuiTextLine *line = &doc->items[i];
            struct nk_rect b;
            enum nk_widget_layout_states st;
            nk_layout_row_static(ctx, line->height, (int)row_w, 1);
            st = nk_widget(&b, ctx);
            if (probe_y >= b.y && probe_y < b.y + b.h) {
                float cw = char_width(gui, line->font1);
                int col = (int)((in->mouse.pos.x - b.x - 6.0f) / cw + 0.5f);
                hit_line = (int)i;
                hit_col = col < 0 ? 0 : (col > line->columns ? line->columns : col);
            } else if (hit_line < 0 && probe_y >= b.y + b.h && i + 1 == doc->count) {
                hit_line = (int)i;
                hit_col = line->columns;
            }
            if (st != NK_WIDGET_INVALID) {
                draw_line(gui, line, b, (int)i, sel, gui->text_focus == focus_id, area.w);
            }
        }
        nk_group_end(ctx);
    }
    ctx->style.window.spacing = saved_spacing;
    ctx->style.window.fixed_background = nk_style_item_color(ctx->style.window.background);

    /* Mouse Selection */
    if (pressed && inside && hit_line >= 0) {
        double now = glfwGetTime();
        if (!gui->shift_down) {
            sel->anchor_line = hit_line;
            sel->anchor_col = hit_col;
        }
        sel->cursor_line = hit_line;
        sel->cursor_col = hit_col;
        sel->dragging = 1;
        ev.clicked_line = hit_line;
        if (sel->last_click_line == hit_line && now - sel->last_click < 0.45) {
            ev.double_clicked_line = hit_line;
            sel->last_click_line = -1;
        } else {
            sel->last_click_line = hit_line;
            sel->last_click = now;
        }
    } else if (sel->dragging && hit_line >= 0 && nk_input_is_mouse_down(in, NK_BUTTON_LEFT)) {
        sel->cursor_line = hit_line;
        sel->cursor_col = hit_col;
    }
    if (!nk_input_is_mouse_down(in, NK_BUTTON_LEFT)) {
        sel->dragging = 0;
    }

    /* Keyboard Commands */
    if (interactive && gui->text_focus == focus_id && !gui->edit_active && doc->count > 0) {
        if (nk_input_is_key_pressed(in, NK_KEY_TEXT_SELECT_ALL)) {
            sel->anchor_line = 0;
            sel->anchor_col = 0;
            sel->cursor_line = (int)doc->count - 1;
            sel->cursor_col = doc->items[doc->count - 1].columns;
        }
        if (nk_input_is_key_pressed(in, NK_KEY_COPY)) {
            copy_selection(gui, doc, sel);
            app_set_status(gui->app, APP_STATUS_INFO, "Copied the selected text.");
        }
    }
    if ((size_t)sel->anchor_line >= doc->count || (size_t)sel->cursor_line >= doc->count) {
        gui_textsel_reset(sel);
    }
    return ev;
}
