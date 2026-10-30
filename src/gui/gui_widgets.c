/**
 * @file gui_widgets.c
 * @brief Widget bantu yang dipakai bersama oleh modul GUI.
 */

#include "gui.h"

#include <string.h>

/* ===================================================================== */
/* Tabs and Buttons                                                      */
/* ===================================================================== */

int gui_tabs(Gui *gui, const char *const *labels, int count, int active)
{
    struct nk_context *ctx = &gui->ctx;
    struct nk_style_button saved = ctx->style.button;
    int result = active;
    int i;

    nk_layout_row_begin(ctx, NK_STATIC, gui->row, count);
    for (i = 0; i < count; i++) {
        int is_active = i == active;
        nk_layout_row_push(ctx, gui_text_width(gui, labels[i]) + 28.0f);
        ctx->style.button = saved;
        ctx->style.button.rounding = 0;
        ctx->style.button.normal = nk_style_item_color(is_active ? gui->pal.tab_active : gui->pal.tab_idle);
        ctx->style.button.hover = nk_style_item_color(is_active ? gui->pal.tab_active
                                                                : ctx->style.button.hover.data.color);
        ctx->style.button.text_normal = is_active ? gui->pal.accent : gui->pal.muted;
        ctx->style.button.text_hover = is_active ? gui->pal.accent : gui->pal.text;
        if (is_active) {
            nk_style_push_font(ctx, &gui->font_bold->handle);
        }
        if (nk_button_label(ctx, labels[i])) {
            result = i;
        }
        if (is_active) {
            nk_style_pop_font(ctx);
        }
    }
    ctx->style.button = saved;
    nk_layout_row_end(ctx);
    return result;
}

float gui_tab_width(Gui *gui, const char *label)
{
    return (float)(int)(gui_text_width(gui, label) + 28.0f);
}

int gui_tab_at(Gui *gui, struct nk_rect rect, const char *label, int mnemonic, int active)
{
    struct nk_context *ctx = &gui->ctx;
    struct nk_style_button saved = ctx->style.button;
    struct nk_rect bounds;
    int clicked;

    nk_layout_space_push(ctx, rect);
    bounds = nk_widget_bounds(ctx);
    ctx->style.button.rounding = 0;
    ctx->style.button.normal = nk_style_item_color(active ? gui->pal.tab_active : gui->pal.tab_idle);
    ctx->style.button.hover = nk_style_item_color(active ? gui->pal.tab_active : saved.hover.data.color);
    ctx->style.button.text_normal = active ? gui->pal.accent : gui->pal.muted;
    ctx->style.button.text_hover = active ? gui->pal.accent : gui->pal.text;
    if (active) {
        nk_style_push_font(ctx, &gui->font_bold->handle);
    }
    clicked = nk_button_label(ctx, label);
    if (active) {
        nk_style_pop_font(ctx);
    }
    ctx->style.button = saved;
    gui_mnemonic(gui, bounds, label, mnemonic, 1, active ? &gui->font_bold->handle : &gui->font->handle);
    return clicked;
}

void gui_mnemonic(Gui *gui, struct nk_rect bounds, const char *label, int index, int centered,
                  const struct nk_user_font *font)
{
    float cw;
    float text_w;
    float x;
    float y;

    if (index < 0 || !gui->alt_down || gui->dialog != GUI_DLG_NONE) {
        return;
    }
    cw = font->width(font->userdata, font->height, "M", 1);
    text_w = font->width(font->userdata, font->height, label, (int)strlen(label));
    x = centered ? bounds.x + (bounds.w - text_w) / 2.0f : bounds.x + gui->ctx.style.button.padding.x + 4.0f;
    y = bounds.y + (bounds.h + font->height) / 2.0f + 1.0f;
    nk_stroke_line(nk_window_get_canvas(&gui->ctx), x + (float)index * cw, y, x + (float)(index + 1) * cw, y,
                   1.5f, gui->pal.accent);
}

void gui_focus_edit_end(Gui *gui, nk_flags flags, const char *text)
{
    struct nk_window *win = gui->ctx.current;
    int glyphs = 0;

    nk_edit_focus(&gui->ctx, flags);
    for (; *text != '\0'; text++) {
        if (((unsigned char)*text & 0xC0) != 0x80) {
            glyphs++;
        }
    }
    win->edit.cursor = glyphs;
    win->edit.sel_start = glyphs;
    win->edit.sel_end = glyphs;
}

nk_flags gui_edit(Gui *gui, nk_flags flags, char *buffer, int max)
{
    nk_flags result = nk_edit_string_zero_terminated(&gui->ctx, flags, buffer, max, nk_filter_default);

    if (result & NK_EDIT_ACTIVE) {
        gui->edit_active_now = 1;
    }
    return result;
}

int gui_button(Gui *gui, const char *label, int enabled)
{
    struct nk_context *ctx = &gui->ctx;
    struct nk_style_button saved = ctx->style.button;

    if (enabled) {
        return nk_button_label(ctx, label);
    }

    /* Disabled Look */
    ctx->style.button.normal = nk_style_item_color(gui->pal.tab_idle);
    ctx->style.button.hover = ctx->style.button.normal;
    ctx->style.button.active = ctx->style.button.normal;
    ctx->style.button.text_normal = gui->pal.muted;
    ctx->style.button.text_hover = gui->pal.muted;
    ctx->style.button.text_active = gui->pal.muted;
    ctx->style.button.disabled_factor = 1.0f;
    nk_widget_disable_begin(ctx);
    nk_button_label(ctx, label);
    nk_widget_disable_end(ctx);
    ctx->style.button = saved;
    return 0;
}

void gui_edit_disabled_begin(Gui *gui)
{
    struct nk_context *ctx = &gui->ctx;

    ctx->style.edit.normal = nk_style_item_color(gui->pal.tab_idle);
    ctx->style.edit.text_normal = gui->pal.muted;
    ctx->style.edit.disabled_factor = 1.0f;
    nk_widget_disable_begin(ctx);
}

void gui_edit_disabled_end(Gui *gui)
{
    struct nk_context *ctx = &gui->ctx;
    struct nk_color table_edit = ctx->style.edit.active.data.color;

    nk_widget_disable_end(ctx);
    ctx->style.edit.normal = nk_style_item_color(table_edit);
    ctx->style.edit.text_normal = gui->pal.text;
    ctx->style.edit.disabled_factor = NK_WIDGET_DISABLED_FACTOR;
}

int gui_menu_item(Gui *gui, const char *label, const char *shortcut, int enabled)
{
    struct nk_context *ctx = &gui->ctx;
    struct nk_rect bounds;
    int clicked;

    nk_layout_row_dynamic(ctx, gui->row, 1);
    bounds = nk_widget_bounds(ctx);
    if (!enabled) {
        struct nk_style_button saved = ctx->style.contextual_button;
        ctx->style.contextual_button.text_normal = gui->pal.muted;
        ctx->style.contextual_button.text_hover = gui->pal.muted;
        ctx->style.contextual_button.hover = ctx->style.contextual_button.normal;
        ctx->style.contextual_button.disabled_factor = 1.0f;
        nk_widget_disable_begin(ctx);
        nk_menu_item_label(ctx, label, NK_TEXT_LEFT);
        nk_widget_disable_end(ctx);
        ctx->style.contextual_button = saved;
        clicked = 0;
    } else {
        clicked = nk_menu_item_label(ctx, label, NK_TEXT_LEFT);
    }
    if (shortcut != NULL) {
        struct nk_command_buffer *canvas = nk_window_get_canvas(ctx);
        float w = gui_text_width(gui, shortcut);
        struct nk_rect r = nk_rect(bounds.x + bounds.w - w - 10, bounds.y, w, bounds.h);
        r.y += (bounds.h - gui->font->handle.height) / 2.0f;
        r.h = gui->font->handle.height;
        nk_draw_text(canvas, r, shortcut, (int)strlen(shortcut), &gui->font->handle,
                     nk_rgba(0, 0, 0, 0), gui->pal.muted);
    }
    return clicked;
}

void gui_vspace(Gui *gui, float height)
{
    nk_layout_row_dynamic(&gui->ctx, height, 1);
    nk_spacer(&gui->ctx);
}

/* ===================================================================== */
/* Text                                                                  */
/* ===================================================================== */

float gui_text_width(Gui *gui, const char *text)
{
    const struct nk_user_font *f = &gui->font->handle;
    return f->width(f->userdata, f->height, text, (int)strlen(text));
}

void gui_label_colored(Gui *gui, const char *text, struct nk_color color)
{
    nk_label_colored(&gui->ctx, text, NK_TEXT_LEFT, color);
}

void gui_label_bold(Gui *gui, const char *text, struct nk_color color)
{
    nk_style_push_font(&gui->ctx, &gui->font_bold->handle);
    nk_label_colored(&gui->ctx, text, NK_TEXT_LEFT, color);
    nk_style_pop_font(&gui->ctx);
}

float gui_wrap_height(Gui *gui, const char *text, float width)
{
    float char_w = gui_text_width(gui, "M");
    int per_line = char_w > 0 ? (int)(width / char_w) : 80;
    int lines = 1;
    int col = 0;
    const char *p;

    if (per_line < 1) {
        per_line = 1;
    }
    for (p = text; *p != '\0'; p++) {
        if ((*p & 0xC0) == 0x80) {
            continue;  /* byte lanjutan UTF-8 */
        }
        if (*p == '\n' || col >= per_line) {
            lines++;
            col = (*p == '\n') ? 0 : 1;
        } else {
            col++;
        }
    }
    return (float)lines * (gui->font->handle.height + 2.0f) + 4.0f;
}

void gui_wrap_colored(Gui *gui, const char *text, struct nk_color color)
{
    struct nk_context *ctx = &gui->ctx;
    float width = nk_window_get_panel(ctx)->bounds.w - 24.0f;

    nk_layout_row_dynamic(ctx, gui_wrap_height(gui, text, width), 1);
    nk_label_colored_wrap(ctx, text, color);
}

void gui_draw_segments(Gui *gui, struct nk_rect bounds, const char *prefix, struct nk_color prefix_color,
                       const char *text, struct nk_color text_color, const struct nk_user_font *text_font,
                       struct nk_color background)
{
    struct nk_command_buffer *canvas = nk_window_get_canvas(&gui->ctx);
    const struct nk_user_font *f = &gui->font->handle;
    float pw = f->width(f->userdata, f->height, prefix, (int)strlen(prefix));
    float tw = text_font->width(text_font->userdata, text_font->height, text, (int)strlen(text));
    struct nk_rect r = bounds;
    struct nk_color clear = nk_rgba(0, 0, 0, 0);

    r.y += (bounds.h - f->height) / 2.0f;
    r.h = f->height;
    if (background.a > 0) {
        nk_fill_rect(canvas, nk_rect(bounds.x + pw - 3, bounds.y + 1, tw + 6, bounds.h - 2), 3, background);
    }
    if (prefix[0] != '\0') {
        nk_draw_text(canvas, nk_rect(r.x, r.y, pw + 1, r.h), prefix, (int)strlen(prefix), f, clear, prefix_color);
    }
    nk_draw_text(canvas, nk_rect(r.x + pw, r.y, tw + 1, r.h), text, (int)strlen(text), text_font, clear,
                 text_color);
}

/* ===================================================================== */
/* Dialog Window                                                         */
/* ===================================================================== */

int gui_dialog_begin(Gui *gui, const char *title, float width, float height)
{
    struct nk_context *ctx = &gui->ctx;
    struct nk_rect r;

    if (width > gui->width - 20) {
        width = (float)gui->width - 20;
    }
    if (height > gui->height - 20) {
        height = (float)gui->height - 20;
    }
    r = nk_rect(((float)gui->width - width) / 2.0f, ((float)gui->height - height) / 2.0f, width, height);
    if (nk_begin(ctx, title, r, NK_WINDOW_BORDER | NK_WINDOW_TITLE | NK_WINDOW_NO_SCROLLBAR)) {
        nk_window_set_bounds(ctx, title, r);
        nk_window_set_focus(ctx, title);
        return 1;
    }
    return 0;
}

void gui_dialog_end(Gui *gui)
{
    nk_end(&gui->ctx);
}
