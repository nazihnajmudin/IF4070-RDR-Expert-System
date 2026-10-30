/**
 * @file gui_correct_dialog.c
 * @brief Dialog Edit last query untuk mengoreksi kesimpulan hasil terakhir.
 *
 * Jika difference list tidak kosong, dialog menampilkan klausa exception yang
 * dapat dipilih dan diedit (mode editable) beserta validasi langsung. Jika
 * kosong, dialog menjelaskan strategi revisi yang aktif dan dampaknya.
 */

#include "gui.h"

#include <stdlib.h>
#include <string.h>

#include "rdr/query.h"

/* ===================================================================== */
/* State Preparation                                                     */
/* ===================================================================== */

void gui_correct_dialog_reset(Gui *gui)
{
    GuiCorrectDialog *d = &gui->correct;

    app_correction_plan_free(&d->plan);
    free(d->clauses);
    free(d->enabled);
    d->clauses = NULL;
    d->enabled = NULL;
    d->count = 0;
}

void gui_correct_dialog_prepare(Gui *gui)
{
    GuiCorrectDialog *d = &gui->correct;
    App *app = gui->app;
    const RdrNode *concluded;
    char *text;
    size_t i;

    gui_correct_dialog_reset(gui);
    d->conclusion[0] = '\0';
    d->check[0] = '\0';
    d->query[0] = '\0';
    d->cornerstone[0] = '\0';
    if (!app_correction_plan(app, &d->plan)) {
        return;
    }

    /* Clause Buffers */
    d->count = d->plan.difference.count;
    d->clauses = xcalloc(d->count ? d->count : 1, sizeof(*d->clauses));
    d->enabled = xcalloc(d->count ? d->count : 1, sizeof(int));
    for (i = 0; i < d->count; i++) {
        char *s = clause_to_string(&d->plan.difference.items[i]);
        str_copy(d->clauses[i], GUI_FIELD_MAX, s);
        free(s);
        d->enabled[i] = 1;
    }

    /* Context Texts */
    text = query_format(&app->inference.query);
    str_copy(d->query, sizeof(d->query), text[0] ? text : "(empty)");
    free(text);
    concluded = rdr_kb_find(app->kb, d->plan.concluded_id);
    text = concluded->cornerstone.count ? query_format(&concluded->cornerstone) : str_dup("(empty)");
    str_copy(d->cornerstone, sizeof(d->cornerstone), text);
    free(text);
}

/* ===================================================================== */
/* Validation                                                            */
/* ===================================================================== */

/* Menyusun rule dari klausa tercentang. Mengembalikan 0 jika ada klausa tidak valid. */
static int build_rule(GuiCorrectDialog *d, ClauseList *rule, char *err, size_t errlen)
{
    size_t i;

    for (i = 0; i < d->count; i++) {
        Clause c;
        if (!d->enabled[i]) {
            continue;
        }
        if (!clause_parse(d->clauses[i], &c, err, errlen)) {
            return 0;
        }
        if (!clauselist_contains(rule, &c)) {
            clauselist_push_move(rule, &c);
        } else {
            clause_free(&c);
        }
    }
    return 1;
}

static void validate(Gui *gui, ClauseList *rule)
{
    GuiCorrectDialog *d = &gui->correct;
    App *app = gui->app;
    const RdrNode *concluded = rdr_kb_find(app->kb, d->plan.concluded_id);
    char err[256];

    d->valid = 0;
    if (str_is_blank(d->conclusion)) {
        str_copy(d->check, sizeof(d->check), "Type the correct conclusion.");
        return;
    }
    {
        char *wanted = str_trim_dup(d->conclusion);
        int same = strcmp(wanted, d->plan.conclusion) == 0;
        free(wanted);
        if (same) {
            str_copy(d->check, sizeof(d->check), "The correct conclusion must differ from the current one.");
            return;
        }
    }
    if (d->plan.is_revision) {
        d->valid = d->plan.strategy != RDR_REVISION_REJECT;
        str_copy(d->check, sizeof(d->check), d->valid ? "Ready." : "The active strategy rejects this correction.");
        return;
    }
    if (!build_rule(d, rule, err, sizeof(err))) {
        str_set_error(d->check, sizeof(d->check), "Invalid clause: %s", err);
        return;
    }
    if (!rdr_validate_exception_rule(&app->inference.query, &concluded->cornerstone, rule, err, sizeof(err))) {
        str_set_error(d->check, sizeof(d->check), "Invalid rule: %s.", err);
        return;
    }
    d->valid = 1;
    str_copy(d->check, sizeof(d->check), "The rule fires for this case and not for the cornerstone case.");
}

/* ===================================================================== */
/* Dialog Drawing                                                        */
/* ===================================================================== */

static void labeled_value(Gui *gui, const char *label, const char *value, struct nk_color color)
{
    struct nk_context *ctx = &gui->ctx;
    float lw = gui_text_width(gui, "Current conclusion") + 16;
    float vw = nk_window_get_panel(ctx)->bounds.w - lw - 30;

    nk_layout_row_template_begin(ctx, gui_wrap_height(gui, value, vw));
    nk_layout_row_template_push_static(ctx, lw);
    nk_layout_row_template_push_variable(ctx, 100);
    nk_layout_row_template_end(ctx);
    nk_label_colored(ctx, label, NK_TEXT_LEFT, gui->pal.muted);
    nk_label_colored_wrap(ctx, value, color);
}

static void revision_explanation(Gui *gui)
{
    GuiCorrectDialog *d = &gui->correct;
    char *text = NULL;
    const char *wanted = str_is_blank(d->conclusion) ? "<new conclusion>" : d->conclusion;

    gui_wrap_colored(gui, "No clause of this query distinguishes it from the cornerstone case, so a normal "
                          "exception cannot be added (revision case).", gui->pal.warning);
    switch (d->plan.strategy) {
    case RDR_REVISION_NODE:
        text = str_printf("Strategy: revision node. A new node 'T >> %s' becomes the %s child of node #%d. "
                          "Every case that currently ends at node #%d with '%s' will conclude '%s'.",
                          wanted, d->plan.attach_true ? "true" : "false", d->plan.evaluated_id,
                          d->plan.concluded_id, d->plan.conclusion, wanted);
        break;
    case RDR_REVISION_EDIT_CONCLUSION:
        text = str_printf("Strategy: edit conclusion. The conclusion of node #%d changes from '%s' to '%s'. "
                          "This is not RDR compliant because it rewrites existing knowledge.",
                          d->plan.concluded_id, d->plan.conclusion, wanted);
        break;
    default:
        text = str_dup("Strategy: reject. The correction is not applied. Add a fact that distinguishes this "
                       "case to the query and run it again.");
        break;
    }
    gui_wrap_colored(gui, text, gui->pal.text);
    gui_wrap_colored(gui, "The strategy can be changed in Preference > Edit Preference.", gui->pal.muted);
    free(text);
}

static void clause_editor(Gui *gui, float height)
{
    struct nk_context *ctx = &gui->ctx;
    GuiCorrectDialog *d = &gui->correct;
    size_t i;
    char *where = str_printf("New exception rule. The new node becomes the %s child of node #%d:",
                             d->plan.attach_true ? "true" : "false", d->plan.evaluated_id);

    gui_wrap_colored(gui, where, gui->pal.text);
    free(where);
    nk_layout_row_dynamic(ctx, height, 1);
    ctx->style.window.fixed_background = nk_style_item_color(gui->pal.panel);
    if (nk_group_begin(ctx, "exception_clauses", NK_WINDOW_BORDER)) {
        for (i = 0; i < d->count; i++) {
            nk_bool on = d->enabled[i];
            nk_layout_row_template_begin(ctx, gui->row);
            nk_layout_row_template_push_static(ctx, gui->row + 4);
            nk_layout_row_template_push_variable(ctx, 100);
            nk_layout_row_template_end(ctx);
            if (d->plan.editable) {
                nk_checkbox_label(ctx, "", &on);
                d->enabled[i] = on;
                gui_edit(gui, NK_EDIT_FIELD, d->clauses[i], GUI_FIELD_MAX);
            } else {
                gui_label_colored(gui, "\xE2\x80\xA2", gui->pal.muted);
                gui_label_colored(gui, d->clauses[i], gui->pal.text);
            }
        }
        nk_group_end(ctx);
    }
    ctx->style.window.fixed_background = nk_style_item_color(ctx->style.window.background);
    gui_wrap_colored(gui, d->plan.editable
                              ? "Untick clauses or generalise them (for example 'suhu = 37' to 'suhu < 38'). "
                                "The rule must still fire for this case but not for the cornerstone case."
                              : "Strict mode: every clause of the difference list is used.",
                     gui->pal.muted);
}

void gui_correct_dialog(Gui *gui)
{
    struct nk_context *ctx = &gui->ctx;
    GuiCorrectDialog *d = &gui->correct;
    App *app = gui->app;
    float h = d->plan.available && !d->plan.is_revision ? (float)gui->height * 0.86f : 470.0f;
    ClauseList rule;
    nk_flags flags;

    clauselist_init(&rule);
    if (gui_dialog_begin(gui, "Edit Last Query", 860, h)) {
        if (!d->plan.available || !rdr_inference_is_current(app->kb, &app->inference)) {
            gui_wrap_colored(gui, "There is no current result to correct. Add clauses to the query and press "
                                  "Start first.", gui->pal.muted);
            nk_layout_row_dynamic(ctx, gui->row + 4, 4);
            nk_spacing(ctx, 3);
            if (nk_button_label(ctx, "Close")) {
                gui_close_dialog(gui);
            }
        } else {
            char *current = str_printf("%s  (node #%d)", d->plan.conclusion, d->plan.concluded_id);
            char *cs_label = str_printf("Cornerstone #%d", d->plan.concluded_id);

            /* Case Summary */
            labeled_value(gui, "Query", d->query, gui->pal.text);
            labeled_value(gui, "Current conclusion", current, gui->pal.conclusion);
            labeled_value(gui, cs_label, d->cornerstone, gui->pal.muted);
            free(current);
            free(cs_label);

            /* Correct Conclusion */
            nk_layout_row_template_begin(ctx, gui->row + 2);
            nk_layout_row_template_push_static(ctx, gui_text_width(gui, "Current conclusion") + 16);
            nk_layout_row_template_push_variable(ctx, 100);
            nk_layout_row_template_end(ctx);
            gui_label_bold(gui, "Correct conclusion", gui->pal.text);
            if (gui->dialog_opened) {
                nk_edit_focus(ctx, NK_EDIT_FIELD);
            }
            flags = gui_edit(gui, NK_EDIT_FIELD | NK_EDIT_SIG_ENTER, d->conclusion, GUI_FIELD_MAX);
            gui_vspace(gui, 4);

            /* Rule or Revision */
            if (d->plan.is_revision) {
                revision_explanation(gui);
            } else {
                clause_editor(gui, h * 0.30f);
            }

            /* Validation and Buttons */
            validate(gui, &rule);
            nk_layout_row_dynamic(ctx, gui->row, 1);
            gui_label_colored(gui, d->check, d->valid ? gui->pal.success : gui->pal.error);
            nk_layout_row_template_begin(ctx, gui->row + 4);
            nk_layout_row_template_push_variable(ctx, 10);
            nk_layout_row_template_push_static(ctx, gui_text_width(gui, "Cancel") + 36);
            nk_layout_row_template_push_static(ctx, gui_text_width(gui, "Apply Correction") + 36);
            nk_layout_row_template_end(ctx);
            nk_spacing(ctx, 1);
            if (nk_button_label(ctx, "Cancel")) {
                gui_close_dialog(gui);
            } else if ((gui_button(gui, "Apply Correction", d->valid) || ((flags & NK_EDIT_COMMITED) && d->valid))) {
                app_correct(app, d->conclusion, d->plan.is_revision ? NULL : &rule);
                gui_close_dialog(gui);
            }
        }
    }
    gui_dialog_end(gui);
    clauselist_free(&rule);
}
