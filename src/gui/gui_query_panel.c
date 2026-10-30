/**
 * @file gui_query_panel.c
 * @brief Panel kueri (tab Clause dan Query) serta bar Q.
 */

#include "gui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *const QUERY_TABS[] = {"Clause", "Query"};

/* ===================================================================== */
/* Draft Synchronization                                                 */
/* ===================================================================== */

/* Menyalin draft App ke buffer edit setiap baris, kecuali baris yang sedang diedit. */
static void sync_rows(Gui *gui)
{
    App *app = gui->app;
    size_t i;

    if (gui->rows_revision == app->revision && gui->rows_count == app->draft.count) {
        return;
    }
    if (app->draft.count > gui->rows_cap) {
        gui->rows_cap = app->draft.count + 16;
        gui->rows = xrealloc(gui->rows, gui->rows_cap * sizeof(*gui->rows));
    }
    for (i = 0; i < app->draft.count; i++) {
        if ((int)i != gui->active_row || i >= gui->rows_count) {
            str_copy(gui->rows[i], GUI_FIELD_MAX, app->draft.items[i]);
        }
    }
    gui->rows_count = app->draft.count;
    gui->rows_revision = app->revision;
}

/* Menulis draft sebagai satu klausa per baris untuk kotak teks bebas. */
static void load_text(Gui *gui)
{
    char *text = strlist_join(&gui->app->draft, "\n");

    str_copy(gui->text, sizeof(gui->text), text);
    free(text);
    gui->text_revision = gui->app->revision;
}

void gui_query_commit_text(Gui *gui)
{
    App *app = gui->app;
    char *current;

    if (app->query_tab != SESSION_QUERY_TAB_TEXT) {
        return;
    }
    current = strlist_join(&app->draft, "\n");
    if (strcmp(current, gui->text) != 0) {
        app_draft_set_text(app, gui->text);
    }
    free(current);
    load_text(gui);
}

/* Menyalin baris yang sedang diedit ke draft sebelum aksi lain. */
static void commit_active_row(Gui *gui)
{
    if (gui->active_row >= 0 && (size_t)gui->active_row < gui->rows_count) {
        int row = gui->active_row;
        gui->active_row = -1;
        app_draft_set(gui->app, (size_t)row, gui->rows[row]);
    }
}

void gui_query_start(Gui *gui)
{
    App *app = gui->app;

    commit_active_row(gui);
    gui_query_commit_text(gui);
    if (!str_is_blank(gui->qbar)) {
        app_draft_add(app, gui->qbar);
        gui->qbar[0] = '\0';
    }
    if (app_start(app)) {
        gui_reasoning_start(gui);
    } else {
        gui->reasoning_active = 0;
        gui->tree_valid = 0;
    }
    if (app->query_tab == SESSION_QUERY_TAB_TEXT) {
        load_text(gui);
    }
}

/* ===================================================================== */
/* Clause Tab                                                            */
/* ===================================================================== */

static void clause_tab(Gui *gui)
{
    struct nk_context *ctx = &gui->ctx;
    App *app = gui->app;
    struct nk_style_edit saved = ctx->style.edit;
    char err[256];
    size_t i;
    int any_active = 0;

    sync_rows(gui);
    if (app->draft.count == 0) {
        gui_wrap_colored(gui, "No clauses yet. Type clauses in the Q bar below and press Enter.", gui->pal.muted);
        gui->active_row = -1;
        if (gui->focus_clause) {
            gui->focus_clause = 0;
            gui->qbar_focus = 1;
        }
        return;
    }

    for (i = 0; i < gui->rows_count; i++) {
        nk_flags flags;
        int valid = app_draft_item_valid(app, i, err, sizeof(err));
        struct nk_rect bounds;

        nk_layout_row_template_begin(ctx, gui->row);
        nk_layout_row_template_push_static(ctx, gui_text_width(gui, "99.") + 4);
        nk_layout_row_template_push_variable(ctx, 80);
        nk_layout_row_template_push_static(ctx, gui->row + 4);
        nk_layout_row_template_end(ctx);

        /* Row Number */
        {
            char num[32];
            sprintf(num, "%lu.", (unsigned long)(i + 1));
            nk_label_colored(ctx, num, NK_TEXT_RIGHT, gui->pal.muted);
        }

        /* Clause Field */
        bounds = nk_widget_bounds(ctx);
        if (!valid) {
            ctx->style.edit.text_normal = gui->pal.error;
            ctx->style.edit.text_hover = gui->pal.error;
            ctx->style.edit.text_active = gui->pal.error;
            ctx->style.edit.border_color = gui->pal.error;
        }
        if (i == 0 && gui->focus_clause == 1) {
            gui_focus_edit_end(gui, NK_EDIT_FIELD, gui->rows[0]);
            gui->focus_clause = 0;
        } else if (i == 0 && gui->focus_clause == 2) {
            gui->focus_clause = 1;
        }
        flags = gui_edit(gui, NK_EDIT_FIELD | NK_EDIT_SIG_ENTER, gui->rows[i], GUI_FIELD_MAX);
        ctx->style.edit = saved;
        if (!valid && nk_input_is_mouse_hovering_rect(&ctx->input, bounds)) {
            nk_tooltip(ctx, err);
        }
        if (flags & NK_EDIT_ACTIVE) {
            gui->active_row = (int)i;
            any_active = 1;
        }
        if (flags & (NK_EDIT_COMMITED | NK_EDIT_DEACTIVATED)) {
            gui->active_row = -1;
            app_draft_set(app, i, gui->rows[i]);
            break;  /* draft berubah, sisa baris digambar pada frame berikutnya */
        }

        /* Remove Button */
        if (nk_button_label(ctx, "\xC3\x97")) {
            gui->active_row = -1;
            app_draft_remove(app, i);
            break;
        }
    }
    if (!any_active && gui->active_row >= 0 && i == gui->rows_count) {
        commit_active_row(gui);  /* baris kehilangan fokus tanpa sinyal deactivated */
    }
}

/* ===================================================================== */
/* Query Tab                                                             */
/* ===================================================================== */

static void text_tab(Gui *gui, float height)
{
    struct nk_context *ctx = &gui->ctx;
    nk_flags flags;

    if (!gui->text_active && gui->text_revision != gui->app->revision) {
        load_text(gui);
    }
    nk_layout_row_dynamic(ctx, height, 1);
    if (gui->focus_text == 1) {
        gui_focus_edit_end(gui, NK_EDIT_BOX, gui->text);
        gui->focus_text = 0;
    } else if (gui->focus_text == 2) {
        gui->focus_text = 1;
    }
    flags = gui_edit(gui, NK_EDIT_BOX, gui->text, GUI_TEXT_MAX);
    {
        int was_active = gui->text_active;
        gui->text_active = (flags & NK_EDIT_ACTIVE) != 0;
        if ((flags & NK_EDIT_DEACTIVATED) || (was_active && !gui->text_active)) {
            gui_query_commit_text(gui);  /* fokus pindah tanpa sinyal */
        }
    }
}

/* ===================================================================== */
/* Panel Layout                                                          */
/* ===================================================================== */

void gui_query_panel(Gui *gui, struct nk_rect area, float body_y)
{
    struct nk_context *ctx = &gui->ctx;
    App *app = gui->app;
    struct nk_rect body = nk_rect(area.x, body_y, area.w, area.y + area.h - body_y);
    float pad = ctx->style.window.group_padding.y;
    float x = area.x;
    int tab = app->query_tab;
    int i;

    /* Tab Bar */
    for (i = 0; i < 2; i++) {
        float w = gui_tab_width(gui, QUERY_TABS[i]);
        if (gui_tab_at(gui, nk_rect(x, area.y, w, gui->row), QUERY_TABS[i], i == 0 ? 0 : 4, i == app->query_tab)) {
            tab = i;
        }
        x += w + 4;
    }
    if (tab != app->query_tab) {
        commit_active_row(gui);
        gui_query_commit_text(gui);
        app->query_tab = tab;
        if (tab == SESSION_QUERY_TAB_TEXT) {
            load_text(gui);
        }
    }

    /* Tab Body */
    nk_layout_space_push(ctx, body);
    ctx->style.window.fixed_background = nk_style_item_color(gui->pal.panel);
    if (nk_group_begin(ctx, "query_body", NK_WINDOW_BORDER)) {
        if (app->query_tab == SESSION_QUERY_TAB_TEXT) {
            text_tab(gui, body.h - 2.0f * pad - 6.0f);
        } else {
            clause_tab(gui);
        }
        nk_group_end(ctx);
    }
    ctx->style.window.fixed_background = nk_style_item_color(ctx->style.window.background);
}

/* ===================================================================== */
/* Q Bar                                                                 */
/* ===================================================================== */

void gui_query_bar(Gui *gui, float width)
{
    struct nk_context *ctx = &gui->ctx;
    float h = gui->row + 4;
    float label_w = gui->row + 8;
    float enter_w = (float)(int)(gui_text_width(gui, "Enter") + 30);
    struct nk_rect bounds;
    nk_flags flags;
    int submit = 0;

    /* Prompt Label */
    nk_layout_space_push(ctx, nk_rect(0, 0, label_w, h));
    bounds = nk_widget_bounds(ctx);
    nk_style_push_font(ctx, &gui->font_bold->handle);
    nk_label_colored(ctx, "Q", NK_TEXT_CENTERED, gui->pal.accent);
    nk_style_pop_font(ctx);
    gui_mnemonic(gui, bounds, "Q", 0, 1, &gui->font_bold->handle);

    /* Input Field */
    nk_layout_space_push(ctx, nk_rect(label_w + 6, 0, width - label_w - enter_w - 12, h));
    bounds = nk_widget_bounds(ctx);
    if (gui->qbar_focus) {
        gui_focus_edit_end(gui, NK_EDIT_FIELD, gui->qbar);
        gui->qbar_focus = 0;
    }
    flags = gui_edit(gui, NK_EDIT_FIELD | NK_EDIT_SIG_ENTER, gui->qbar, GUI_FIELD_MAX);
    if (flags & NK_EDIT_COMMITED) {
        submit = 1;
        gui->qbar_focus = 1;
    }
    if (gui->qbar[0] == '\0' && !(flags & NK_EDIT_ACTIVE)) {
        const char *hint = "Type clauses separated by ';' and press Enter, e.g. demam; suhu >= 38.5";
        struct nk_rect r = nk_rect(bounds.x + 8, bounds.y + (bounds.h - gui->font->handle.height) / 2,
                                   bounds.w - 16, gui->font->handle.height);
        nk_draw_text(nk_window_get_canvas(ctx), r, hint, (int)strlen(hint), &gui->font->handle,
                     nk_rgba(0, 0, 0, 0), gui->pal.muted);
    }

    /* Enter Button */
    nk_layout_space_push(ctx, nk_rect(width - enter_w, 0, enter_w, h));
    if (nk_button_label(ctx, "Enter")) {
        submit = 1;
        gui->qbar_focus = 1;
    }
    if (submit && !str_is_blank(gui->qbar)) {
        size_t added = app_draft_add(gui->app, gui->qbar);
        gui->qbar[0] = '\0';
        app_set_status(gui->app, APP_STATUS_INFO, "Added %lu clause(s) to the query.", (unsigned long)added);
    }
}
