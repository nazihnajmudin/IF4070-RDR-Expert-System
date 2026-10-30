/**
 * @file gui_prefs_dialog.c
 * @brief Dialog Edit Preference yang dibangkitkan dari tabel deskriptor preferensi.
 *
 * Setiap preferensi digambar sesuai tipenya: checkbox untuk PREF_BOOL,
 * property untuk PREF_INT, dan combo untuk PREF_ENUM. Preferensi baru di
 * prefs.c otomatis muncul di dialog ini tanpa perubahan kode GUI.
 */

#include "gui.h"

#include <string.h>

static void pref_row(Gui *gui, PrefId id)
{
    struct nk_context *ctx = &gui->ctx;
    const PrefDescriptor *d = prefs_descriptor(id);
    int value = prefs_get(&gui->prefs_edit, id);
    struct nk_rect label_bounds;

    nk_layout_row_dynamic(ctx, gui->row + 2, 2);
    label_bounds = nk_widget_bounds(ctx);
    gui_label_colored(gui, d->label, gui->pal.text);
    if (nk_input_is_mouse_hovering_rect(&ctx->input, label_bounds)) {
        nk_tooltip(ctx, d->description);
    }

    /* Value Control */
    switch (d->type) {
    case PREF_BOOL: {
        nk_bool on = value != 0;
        nk_checkbox_label(ctx, on ? "On" : "Off", &on);
        value = on ? 1 : 0;
        break;
    }
    case PREF_INT:
        nk_property_int(ctx, "#", d->min_value, &value, d->max_value, 1, 1.0f);
        break;
    case PREF_ENUM:
        value = nk_combo(ctx, (const char **)d->option_labels, d->option_count, value, (int)gui->row,
                         nk_vec2(nk_widget_width(ctx), (float)d->option_count * (gui->row + 6) + 12));
        break;
    }
    prefs_set(&gui->prefs_edit, id, value);

    /* Description */
    nk_layout_row_dynamic(ctx, gui->row - 4, 1);
    gui_label_colored(gui, d->description, gui->pal.muted);
}

void gui_prefs_dialog(Gui *gui)
{
    struct nk_context *ctx = &gui->ctx;
    float h = (float)gui->height * 0.88f;
    const char *group = NULL;
    int i;

    if (gui_dialog_begin(gui, "Edit Preference", 760, h)) {
        struct nk_style *s = &ctx->style;
        float title_h = gui->font->handle.height + 2 * s->window.header.padding.y + 2 * s->window.header.label_padding.y;
        float body_h = h - title_h - 2 * (gui->row + 4) - 3 * s->window.spacing.y - 2 * s->window.padding.y - 8;

        /* Preference List */
        nk_layout_row_dynamic(ctx, body_h, 1);
        if (nk_group_begin(ctx, "prefs_body", NK_WINDOW_BORDER)) {
            for (i = 0; i < PREF_COUNT; i++) {
                const PrefDescriptor *d = prefs_descriptor((PrefId)i);
                if (group == NULL || strcmp(group, d->group) != 0) {
                    group = d->group;
                    nk_layout_row_dynamic(ctx, gui->row + 4, 1);
                    gui_label_bold(gui, group, gui->pal.accent);
                }
                pref_row(gui, (PrefId)i);
            }
            nk_group_end(ctx);
        }
        nk_layout_row_dynamic(ctx, gui->row, 1);
        gui_label_colored(gui, "Applied preferences are stored in the project file on the next Save.",
                          gui->pal.muted);

        /* Buttons */
        nk_layout_row_template_begin(ctx, gui->row + 4);
        nk_layout_row_template_push_static(ctx, gui_text_width(gui, "Reset to Default") + 36);
        nk_layout_row_template_push_variable(ctx, 10);
        nk_layout_row_template_push_static(ctx, gui_text_width(gui, "Cancel") + 36);
        nk_layout_row_template_push_static(ctx, gui_text_width(gui, "Apply") + 36);
        nk_layout_row_template_end(ctx);
        if (nk_button_label(ctx, "Reset to Default")) {
            prefs_init_defaults(&gui->prefs_edit);
        }
        nk_spacing(ctx, 1);
        if (nk_button_label(ctx, "Cancel")) {
            gui_close_dialog(gui);
        }
        if (nk_button_label(ctx, "Apply")) {
            if (!app_set_prefs(gui->app, &gui->prefs_edit)) {
                app_set_status(gui->app, APP_STATUS_INFO, "Preferences unchanged.");
            }
            gui_sync_appearance(gui);
            gui_close_dialog(gui);
        }
    }
    gui_dialog_end(gui);
}
