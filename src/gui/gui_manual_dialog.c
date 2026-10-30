/**
 * @file gui_manual_dialog.c
 * @brief Dialog Manual Edit untuk mengubah tree secara langsung.
 *
 * Bagian kiri menampilkan tree yang dapat dipilih. Bagian kanan berisi form
 * node terpilih (rule, kesimpulan, cornerstone), aksi tambah anak, hapus
 * subtree, dan validasi KB. Klik pada slot kosong langsung menyiapkan form
 * tambah anak untuk slot tersebut.
 */

#include "gui.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "rdr/query.h"

/* ===================================================================== */
/* Form Loading                                                          */
/* ===================================================================== */

static char *clauses_text(const ClauseList *list, int rule_mode)
{
    if (list->count == 0) {
        return str_dup(rule_mode ? CLAUSE_TRUE_KEYWORD : "");
    }
    return query_format(list);
}

void gui_manual_dialog_load(Gui *gui, int node_id)
{
    GuiManualDialog *m = &gui->manual;
    const RdrNode *n = rdr_kb_find(gui->app->kb, node_id);
    char *text;

    if (n == NULL) {
        n = gui->app->kb->root;
    }
    m->selected = n->id;
    m->loaded = n->id;
    m->loaded_revision = gui->app->revision;
    text = clauses_text(&n->rule, 1);
    str_copy(m->rule, sizeof(m->rule), text);
    free(text);
    str_copy(m->conclusion, sizeof(m->conclusion), n->conclusion);
    text = clauses_text(&n->cornerstone, 0);
    str_copy(m->cornerstone, sizeof(m->cornerstone), text);
    free(text);
    m->confirm_delete = 0;
}

static void set_message(GuiManualDialog *m, int is_error, const char *text)
{
    str_copy(m->message, sizeof(m->message), text);
    m->message_error = is_error;
}

static size_t subtree_size(const RdrNode *n)
{
    const RdrNode **stack;
    size_t sp = 0;
    size_t cap = 64;
    size_t count = 0;

    stack = xmalloc(cap * sizeof(*stack));
    stack[sp++] = n;
    while (sp > 0) {
        const RdrNode *cur = stack[--sp];
        count++;
        if (sp + 2 > cap) {
            cap *= 2;
            stack = xrealloc(stack, cap * sizeof(*stack));
        }
        if (cur->true_child) {
            stack[sp++] = cur->true_child;
        }
        if (cur->false_child) {
            stack[sp++] = cur->false_child;
        }
    }
    free(stack);
    return count;
}

/* ===================================================================== */
/* Tree Selector                                                         */
/* ===================================================================== */

static void tree_selector(Gui *gui)
{
    struct nk_context *ctx = &gui->ctx;
    GuiManualDialog *m = &gui->manual;
    struct nk_list_view view;
    struct nk_vec2 saved = ctx->style.window.spacing;
    float line_h = gui->font->handle.height + 3.0f;
    int i;

    gui_tree_refresh(gui);
        ctx->style.window.spacing = nk_vec2(saved.x, 0);
    ctx->style.window.fixed_background = nk_style_item_color(gui->pal.panel);
    if (nk_list_view_begin(ctx, &view, "manual_tree", NK_WINDOW_BORDER, (int)line_h, (int)gui->tree.count)) {
        ctx->style.window.spacing = nk_vec2(saved.x, 0);
        nk_layout_row_static(ctx, line_h, (int)gui->tree_width + 24, 1);
        for (i = view.begin; i < view.end; i++) {
            const RdrLine *line = &gui->tree.items[i];
            struct nk_rect bounds;
            int selected = line->kind == RDR_LINE_NODE && line->node_id == m->selected;
            struct nk_color color = line->kind == RDR_LINE_NODE ? gui->pal.text : gui->pal.muted;
            if (nk_widget(&bounds, ctx) == NK_WIDGET_INVALID) {
                continue;
            }
            if (selected) {
                nk_fill_rect(nk_window_get_canvas(ctx), bounds, 2,
                             ctx->style.selectable.normal_active.data.color);
            }
            gui_draw_segments(gui, bounds, line->prefix, gui->pal.muted, line->text,
                              selected ? gui->pal.accent : color,
                              selected ? &gui->font_bold->handle : &gui->font->handle, nk_rgba(0, 0, 0, 0));
            if (line->kind == RDR_LINE_CORNERSTONE || !nk_input_mouse_clicked(&ctx->input, NK_BUTTON_LEFT, bounds)) {
                continue;
            }

            /* Selection */
            if (line->kind == RDR_LINE_NODE) {
                gui_manual_dialog_load(gui, line->node_id);
                m->adding = 0;
            } else if (line->parent_id >= 0) {
                gui_manual_dialog_load(gui, line->parent_id);
                m->adding = line->edge == 0 ? 1 : 2;
                m->new_rule[0] = '\0';
                m->new_conclusion[0] = '\0';
                m->new_case[0] = '\0';
            }
            m->message[0] = '\0';
        }
        nk_list_view_end(&view);
    }
    ctx->style.window.spacing = saved;
    ctx->style.window.fixed_background = nk_style_item_color(ctx->style.window.background);
}

/* ===================================================================== */
/* Node Form                                                             */
/* ===================================================================== */

static void form_field(Gui *gui, const char *label, char *buffer, int size, int enabled)
{
    struct nk_context *ctx = &gui->ctx;

    nk_layout_row_dynamic(ctx, gui->row - 4, 1);
    gui_label_colored(gui, label, gui->pal.muted);
    nk_layout_row_dynamic(ctx, gui->row + 2, 1);
    if (!enabled) {
        gui_edit_disabled_begin(gui);
    }
    gui_edit(gui, NK_EDIT_FIELD, buffer, size);
    if (!enabled) {
        gui_edit_disabled_end(gui);
    }
}

static void save_node(Gui *gui, const RdrNode *n)
{
    GuiManualDialog *m = &gui->manual;
    App *app = gui->app;
    char err[512];
    char *rule = clauses_text(&n->rule, 1);
    char *cs = clauses_text(&n->cornerstone, 0);
    int id = n->id;
    int ok = 1;
    int changed = 0;

    if (n != app->kb->root && strcmp(rule, m->rule) != 0) {
        changed = 1;
        ok = app_edit_rule(app, id, m->rule, err, sizeof(err));
    }
    if (ok && strcmp(rdr_kb_find(app->kb, id)->conclusion, m->conclusion) != 0) {
        changed = 1;
        ok = app_edit_conclusion(app, id, m->conclusion, err, sizeof(err));
    }
    if (ok && rdr_kb_find(app->kb, id) != app->kb->root && strcmp(cs, m->cornerstone) != 0) {
        changed = 1;
        ok = app_edit_cornerstone(app, id, m->cornerstone, err, sizeof(err));
    }
    free(rule);
    free(cs);
    if (!changed) {
        set_message(m, 0, "Nothing to save.");
    } else if (ok) {
        gui_manual_dialog_load(gui, id);
        set_message(m, 0, "Node saved.");
    } else {
        set_message(m, 1, err);
    }
}

static void add_child_form(Gui *gui, const RdrNode *n)
{
    struct nk_context *ctx = &gui->ctx;
    GuiManualDialog *m = &gui->manual;
    char err[512];
    char *title = str_printf("New %s child of node #%d", m->adding == 1 ? "true" : "false", n->id);
    int new_id = -1;

    nk_layout_row_dynamic(ctx, gui->row, 1);
    gui_label_bold(gui, title, gui->pal.accent);
    free(title);
    form_field(gui, "Rule (clauses separated by ';', or T)", m->new_rule, GUI_LONG_FIELD_MAX, 1);
    form_field(gui, "Conclusion", m->new_conclusion, GUI_FIELD_MAX, 1);
    form_field(gui, "Cornerstone case (optional)", m->new_case, GUI_LONG_FIELD_MAX, 1);
    nk_layout_row_dynamic(ctx, gui->row + 4, 2);
    if (nk_button_label(ctx, "Add Node")) {
        if (app_add_child(gui->app, n->id, m->adding == 1 ? RDR_SIDE_TRUE : RDR_SIDE_FALSE, m->new_rule,
                          m->new_conclusion, m->new_case, &new_id, err, sizeof(err))) {
            m->adding = 0;
            gui_manual_dialog_load(gui, new_id);
            set_message(m, 0, gui->app->status);
        } else {
            set_message(m, 1, err);
        }
    }
    if (nk_button_label(ctx, "Cancel")) {
        m->adding = 0;
    }
}

static void node_form(Gui *gui)
{
    struct nk_context *ctx = &gui->ctx;
    GuiManualDialog *m = &gui->manual;
    App *app = gui->app;
    const RdrNode *n = rdr_kb_find(app->kb, m->selected);
    int is_root;
    char *title;
    char err[512];
    size_t i;

        if (!nk_group_begin(ctx, "node_form", NK_WINDOW_BORDER)) {
        return;
    }
    if (n == NULL) {
        gui_wrap_colored(gui, "Select a node in the tree.", gui->pal.muted);
        nk_group_end(ctx);
        return;
    }
    is_root = n == app->kb->root;

    /* Node Header */
    title = is_root ? str_printf("Node #%d (root)", n->id)
                    : n->revises != RDR_NO_REVISION ? str_printf("Node #%d (revises #%d)", n->id, n->revises)
                                                    : str_printf("Node #%d", n->id);
    nk_layout_row_dynamic(ctx, gui->row, 1);
    gui_label_bold(gui, title, gui->pal.accent);
    free(title);

    /* Add Child Mode */
    if (m->adding) {
        add_child_form(gui, n);  /* hanya form tambah anak */
        if (m->message[0] != '\0') {
            gui_wrap_colored(gui, m->message, m->message_error ? gui->pal.error : gui->pal.success);
        }
        nk_group_end(ctx);
        return;
    }

    /* Node Fields */
    form_field(gui, is_root ? "Rule (always T for the root)" : "Rule (clauses separated by ';', or T)",
               m->rule, GUI_LONG_FIELD_MAX, !is_root);
    form_field(gui, is_root ? "Default conclusion" : "Conclusion", m->conclusion, GUI_FIELD_MAX, 1);
    form_field(gui, is_root ? "Cornerstone (always empty for the root)" : "Cornerstone case",
               m->cornerstone, GUI_LONG_FIELD_MAX, !is_root);
    nk_layout_row_dynamic(ctx, gui->row + 4, 2);
    if (nk_button_label(ctx, "Save Changes")) {
        save_node(gui, n);
        n = rdr_kb_find(app->kb, m->selected);
    }
    if (nk_button_label(ctx, "Revert")) {
        gui_manual_dialog_load(gui, n->id);
        set_message(m, 0, "Reverted.");
    }

    /* Structure Actions */
    nk_layout_row_dynamic(ctx, gui->row + 4, 3);
    if (gui_button(gui, "Add True Child", n->true_child == NULL)) {
        m->adding = 1;
        m->confirm_delete = 0;
        m->new_rule[0] = m->new_conclusion[0] = m->new_case[0] = '\0';
    }
    if (gui_button(gui, "Add False Child", n->false_child == NULL && !is_root)) {
        m->adding = 2;
        m->confirm_delete = 0;
        m->new_rule[0] = m->new_conclusion[0] = m->new_case[0] = '\0';
    }
    if (gui_button(gui, "Delete Subtree", !is_root)) {
        m->confirm_delete = 1;
        m->adding = 0;
    }
    if (m->confirm_delete) {
        char *q = str_printf("Delete node #%d and its %lu descendant(s)?", n->id,
                             (unsigned long)(subtree_size(n) - 1));
        gui_wrap_colored(gui, q, gui->pal.warning);
        free(q);
        nk_layout_row_dynamic(ctx, gui->row + 4, 2);
        if (nk_button_label(ctx, "Delete")) {
            int parent = n->parent ? n->parent->id : 0;
            if (app_delete_node(app, n->id, NULL, err, sizeof(err))) {
                gui_manual_dialog_load(gui, parent);
                set_message(m, 0, app->status);
            } else {
                set_message(m, 1, err);
            }
            m->confirm_delete = 0;
        }
        if (nk_button_label(ctx, "Keep")) {
            m->confirm_delete = 0;
        }
    }
    /* Message */
    if (m->message[0] != '\0') {
        gui_wrap_colored(gui, m->message, m->message_error ? gui->pal.error : gui->pal.success);
    }

    /* Validation */
    gui_vspace(gui, 8);
    nk_layout_row_dynamic(ctx, gui->row + 4, 1);
    if (nk_button_label(ctx, "Validate Knowledge Base")) {
        if (m->has_validation) {
            rdr_validation_free(&m->validation);
        }
        app_validate(app, &m->validation);
        m->has_validation = 1;
    }
    if (m->has_validation) {
        char *summary = str_printf("%lu node(s) checked, %lu problem(s), %lu note(s).",
                                   (unsigned long)m->validation.checked, (unsigned long)m->validation.errors,
                                   (unsigned long)(m->validation.count - m->validation.errors));
        gui_wrap_colored(gui, summary, m->validation.errors ? gui->pal.warning : gui->pal.success);
        free(summary);
        for (i = 0; i < m->validation.count; i++) {
            const RdrIssue *issue = &m->validation.items[i];
            int is_error = issue->kind == RDR_ISSUE_MISMATCH || issue->kind == RDR_ISSUE_INCONSISTENT_CASE;
            gui_wrap_colored(gui, issue->message, is_error ? gui->pal.error : gui->pal.muted);
        }
    }
    nk_group_end(ctx);
}

/* ===================================================================== */
/* Dialog Drawing                                                        */
/* ===================================================================== */

void gui_manual_dialog(Gui *gui)
{
    struct nk_context *ctx = &gui->ctx;
    GuiManualDialog *m = &gui->manual;
    float w = (float)gui->width * 0.94f;
    float h = (float)gui->height * 0.92f;

    if (gui_dialog_begin(gui, "Manual Edit", w, h)) {
        struct nk_rect region = nk_window_get_content_region(ctx);
        float space_h = (float)(int)(region.h - 2 * ctx->style.window.spacing.y - 2);
        float button_h = gui->row + 4;
        float close_w = (float)(int)(gui_text_width(gui, "Close") + 40);
        float gap = 10;

        if (rdr_kb_find(gui->app->kb, m->selected) == NULL) {
            gui_manual_dialog_load(gui, 0);
        }

        nk_layout_space_begin(ctx, NK_STATIC, space_h, INT_MAX);
        {
            float width = (float)(int)nk_layout_space_bounds(ctx).w;
            float box_y = gui->row + 6;
            float box_h = space_h - box_y - button_h - 10;
            float left_w = (float)(int)((width - gap) * 0.52f);

            /* Hint */
            nk_layout_space_push(ctx, nk_rect(0, 0, width, gui->row));
            gui_label_colored(gui, "Click a node to edit it. Click an empty slot to add a child there. "
                                   "Manual edits bypass RDR acquisition, so validate afterwards.", gui->pal.muted);

            /* Tree and Form Boxes */
            nk_layout_space_push(ctx, nk_rect(0, box_y, left_w, box_h));
            tree_selector(gui);
            nk_layout_space_push(ctx, nk_rect(left_w + gap, box_y, width - left_w - gap, box_h));
            node_form(gui);

            /* Close Button */
            nk_layout_space_push(ctx, nk_rect(width - close_w, space_h - button_h, close_w, button_h));
            if (nk_button_label(ctx, "Close")) {
                gui_close_dialog(gui);
            }
        }
        nk_layout_space_end(ctx);
    }
    gui_dialog_end(gui);
}
