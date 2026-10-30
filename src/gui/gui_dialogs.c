/**
 * @file gui_dialogs.c
 * @brief Dispatcher dialog dan dialog sederhana (New, Unsaved, laporan CSV, Help, About, Details).
 */

#include "gui.h"

#include <stdlib.h>
#include <string.h>

/* Teks halaman bantuan yang di-embed saat build (assets/help/help_id.txt dan help_en.txt) */
extern const unsigned char help_text_id[];
extern const unsigned char help_text_en[];

static const char *const HELP_LANGUAGES[] = {"Bahasa Indonesia", "English"};

/** Versi aplikasi yang ditampilkan di About. */
#define APP_VERSION "1.0.0"

/* ===================================================================== */
/* Shared Helpers                                                        */
/* ===================================================================== */

/* Baris tombol rata kanan. Mengembalikan indeks tombol yang diklik atau -1. */
static int button_row(Gui *gui, const char *const *labels, int count)
{
    struct nk_context *ctx = &gui->ctx;
    int clicked = -1;
    int i;

    nk_layout_row_template_begin(ctx, gui->row + 4);
    nk_layout_row_template_push_variable(ctx, 10);
    for (i = 0; i < count; i++) {
        nk_layout_row_template_push_static(ctx, gui_text_width(gui, labels[i]) + 36);
    }
    nk_layout_row_template_end(ctx);
    nk_spacing(ctx, 1);
    for (i = 0; i < count; i++) {
        if (nk_button_label(ctx, labels[i])) {
            clicked = i;
        }
    }
    return clicked;
}

/* Tinggi area isi dialog setelah judul dan baris tombol. */
static float body_height(Gui *gui, float dialog_h)
{
    struct nk_style *s = &gui->ctx.style;
    float title_h = gui->font->handle.height + 2 * s->window.header.padding.y + 2 * s->window.header.label_padding.y;
    return dialog_h - title_h - gui->row - 4 - 3 * s->window.spacing.y - 2 * s->window.padding.y - 8;
}

/* ===================================================================== */
/* New and Welcome Dialogs                                               */
/* ===================================================================== */

static void new_dialog(Gui *gui, int welcome)
{
    struct nk_context *ctx = &gui->ctx;
    static const char *const NEW_BUTTONS[] = {"Cancel", "Create"};
    static const char *const WELCOME_BUTTONS[] = {"Open Project...", "Create"};
    const char *title = welcome ? "Welcome to RDR Expert System" : "New Knowledge Base";
    nk_flags flags;
    int clicked;
    int create = 0;

    if (gui_dialog_begin(gui, title, 640, 330)) {
        gui_wrap_colored(gui,
                         "A knowledge base starts with a single root rule 'T >> <default>'. The default "
                         "conclusion is the only domain specific setting, for example 'Sehat' for a "
                         "diagnosis system or '0%' for a tax system.", gui->pal.text);
        gui_vspace(gui, 6);
        nk_layout_row_dynamic(ctx, gui->row, 1);
        gui_label_bold(gui, "Default conclusion", gui->pal.text);
        nk_layout_row_dynamic(ctx, gui->row + 2, 1);
        if (gui->dialog_opened) {
            nk_edit_focus(ctx, NK_EDIT_FIELD);
        }
        flags = gui_edit(gui, NK_EDIT_FIELD | NK_EDIT_SIG_ENTER, gui->new_default, GUI_FIELD_MAX);
        if (flags & NK_EDIT_COMMITED) {
            create = 1;
        }
        {
            char *rule = str_printf("Root rule: T >> %s", str_is_blank(gui->new_default) ? "..." : gui->new_default);
            nk_layout_row_dynamic(ctx, gui->row, 1);
            gui_label_colored(gui, rule, gui->pal.muted);
            free(rule);
        }
        if (welcome) {
            gui_wrap_colored(gui, "You can also open an existing project file (*.rdr.json).", gui->pal.muted);
        }
        clicked = button_row(gui, welcome ? WELCOME_BUTTONS : NEW_BUTTONS, 2);
        if (clicked == 1) {
            create = 1;
        } else if (clicked == 0) {
            if (welcome) {
                gui_open_file_dialog(gui, GUI_FILE_OPEN_PROJECT);
            } else {
                gui_close_dialog(gui);
            }
        }
        if (gui->dialog_error[0] != '\0') {
            nk_layout_row_dynamic(ctx, gui->row, 1);
            gui_label_colored(gui, gui->dialog_error, gui->pal.error);
        }
        if (create) {
            if (app_new(gui->app, gui->new_default)) {
                gui_reset_view(gui);
                gui_close_dialog(gui);
            } else {
                str_copy(gui->dialog_error, sizeof(gui->dialog_error), "The default conclusion cannot be empty.");
            }
        }
    }
    gui_dialog_end(gui);
}

/* ===================================================================== */
/* Unsaved Changes Dialog                                                */
/* ===================================================================== */

static void unsaved_dialog(Gui *gui)
{
    static const char *const BUTTONS[] = {"Cancel", "Discard", "Save"};
    int clicked;

    if (gui_dialog_begin(gui, "Unsaved Changes", 560, 150)) {
        gui_wrap_colored(gui, "The knowledge base or preferences have changes that are not saved yet. "
                              "Save them before continuing?", gui->pal.text);
        clicked = button_row(gui, BUTTONS, 3);
        if (clicked == 0) {
            gui->pending = GUI_PENDING_NONE;
            gui_close_dialog(gui);
        } else if (clicked == 1) {
            gui_run_pending(gui);
        } else if (clicked == 2) {
            gui_query_commit_text(gui);
            if (gui->app->file_path == NULL) {
                gui_open_file_dialog(gui, GUI_FILE_SAVE_PROJECT);
            } else if (app_save(gui->app)) {
                gui_run_pending(gui);
            } else {
                gui->pending = GUI_PENDING_NONE;
                gui_close_dialog(gui);
            }
        }
    }
    gui_dialog_end(gui);
}

/* ===================================================================== */
/* Message List Dialogs                                                  */
/* ===================================================================== */

static void message_list(Gui *gui, const StrList *messages, float height)
{
    struct nk_context *ctx = &gui->ctx;
    size_t i;

    nk_layout_row_dynamic(ctx, height, 1);
    ctx->style.window.fixed_background = nk_style_item_color(gui->pal.panel);
    if (nk_group_begin(ctx, "messages", NK_WINDOW_BORDER)) {
        if (messages->count == 0) {
            nk_layout_row_dynamic(ctx, gui->row, 1);
            gui_label_colored(gui, "(no messages)", gui->pal.muted);
        }
        for (i = 0; i < messages->count; i++) {
            gui_wrap_colored(gui, messages->items[i], gui->pal.text);
        }
        nk_group_end(ctx);
    }
    ctx->style.window.fixed_background = nk_style_item_color(ctx->style.window.background);
}

static void csv_report_dialog(Gui *gui)
{
    static const char *const BUTTONS[] = {"Close"};
    float h = (float)gui->height * 0.8f;
    char *summary;

    if (gui_dialog_begin(gui, "CSV Import Report", 860, h)) {
        summary = csv_report_summary(&gui->csv_report);
        nk_layout_row_dynamic(&gui->ctx, gui->row, 1);
        gui_label_bold(gui, platform_path_basename(gui->csv_path), gui->pal.accent);
        gui_wrap_colored(gui, summary, gui->csv_report.failed ? gui->pal.warning : gui->pal.success);
        free(summary);
        message_list(gui, &gui->csv_report.messages, body_height(gui, h) - 3 * gui->row - 10);
        if (button_row(gui, BUTTONS, 1) == 0) {
            gui_close_dialog(gui);
        }
    }
    gui_dialog_end(gui);
}

static void details_dialog(Gui *gui)
{
    static const char *const BUTTONS[] = {"Close"};
    float h = (float)gui->height * 0.6f;

    if (gui_dialog_begin(gui, "Message Details", 760, h)) {
        gui_wrap_colored(gui, gui->app->status, gui->pal.text);
        message_list(gui, &gui->app->details, body_height(gui, h) - gui->row * 2);
        if (button_row(gui, BUTTONS, 1) == 0) {
            gui_close_dialog(gui);
        }
    }
    gui_dialog_end(gui);
}

/* ===================================================================== */
/* Help and About Dialogs                                                */
/* ===================================================================== */

/* Menggambar satu baris teks bantuan dengan format sederhana. */
static void help_line(Gui *gui, const char *line)
{
    struct nk_context *ctx = &gui->ctx;

    if (strncmp(line, "## ", 3) == 0) {
        nk_layout_row_dynamic(ctx, gui->row + 2, 1);
        gui_label_bold(gui, line + 3, gui->pal.text);
    } else if (strncmp(line, "# ", 2) == 0) {
        nk_layout_row_dynamic(ctx, gui->row + 6, 1);
        gui_label_bold(gui, line + 2, gui->pal.accent);
    } else if (line[0] == '\0') {
        gui_vspace(gui, 6);
    } else {
        gui_wrap_colored(gui, line, gui->pal.text);
    }
}

static void help_dialog(Gui *gui)
{
    struct nk_context *ctx = &gui->ctx;
    static const char *const BUTTONS[] = {"Close"};
    float h = (float)gui->height * 0.88f;

    if (gui_dialog_begin(gui, "Help", 900, h)) {
        int lang = prefs_get(&gui->app->prefs, PREF_UI_HELP_LANGUAGE);
        int picked = gui_tabs(gui, HELP_LANGUAGES, 2, lang);
        if (picked != lang) {
            Preferences p = gui->app->prefs;
            prefs_set(&p, PREF_UI_HELP_LANGUAGE, picked);
            app_set_prefs(gui->app, &p);
            lang = picked;
        }
        nk_layout_row_dynamic(ctx, body_height(gui, h) - gui->row - ctx->style.window.spacing.y, 1);
        ctx->style.window.fixed_background = nk_style_item_color(gui->pal.panel);
        if (nk_group_begin(ctx, "help_body", NK_WINDOW_BORDER)) {
            const char *p = (const char *)(lang == PREF_HELP_EN ? help_text_en : help_text_id);
            while (*p != '\0') {
                const char *end = strchr(p, '\n');
                char *line = end ? str_ndup(p, (size_t)(end - p)) : str_dup(p);
                help_line(gui, line);
                free(line);
                if (end == NULL) {
                    break;
                }
                p = end + 1;
            }
            nk_group_end(ctx);
        }
        ctx->style.window.fixed_background = nk_style_item_color(ctx->style.window.background);
        if (button_row(gui, BUTTONS, 1) == 0) {
            gui_close_dialog(gui);
        }
    }
    gui_dialog_end(gui);
}

static void about_dialog(Gui *gui)
{
    struct nk_context *ctx = &gui->ctx;
    static const char *const BUTTONS[] = {"Close"};

    if (gui_dialog_begin(gui, "About", 600, 340)) {
        nk_layout_row_dynamic(ctx, gui->font_large->handle.height + 8, 1);
        nk_style_push_font(ctx, &gui->font_large->handle);
        nk_label_colored(ctx, APP_NAME, NK_TEXT_LEFT, gui->pal.accent);
        nk_style_pop_font(ctx);
        nk_layout_row_dynamic(ctx, gui->row, 1);
        gui_label_colored(gui, "Version " APP_VERSION, gui->pal.muted);
        gui_wrap_colored(gui, "Binary Single Classification Ripple Down Rules expert system written in C. "
                              "Built for IF4070 Knowledge Representation and Reasoning, ITB.", gui->pal.text);
        gui_wrap_colored(gui, "Uses Nuklear (GUI), GLFW (window and input), and the DejaVu Sans Mono font. "
                              "The RDR engine, JSON and CSV parsers are written from scratch.", gui->pal.muted);
        if (button_row(gui, BUTTONS, 1) == 0) {
            gui_close_dialog(gui);
        }
    }
    gui_dialog_end(gui);
}

/* ===================================================================== */
/* Dispatcher                                                            */
/* ===================================================================== */

void gui_dialogs(Gui *gui)
{
    int opened = gui->dialog;

    switch (gui->dialog) {
    case GUI_DLG_WELCOME:
        new_dialog(gui, 1);
        break;
    case GUI_DLG_NEW:
        new_dialog(gui, 0);
        break;
    case GUI_DLG_UNSAVED:
        unsaved_dialog(gui);
        break;
    case GUI_DLG_FILE:
        gui_file_dialog(gui);
        break;
    case GUI_DLG_CORRECT:
        gui_correct_dialog(gui);
        break;
    case GUI_DLG_MANUAL:
        gui_manual_dialog(gui);
        break;
    case GUI_DLG_CSV_REPORT:
        csv_report_dialog(gui);
        break;
    case GUI_DLG_PREFS:
        gui_prefs_dialog(gui);
        break;
    case GUI_DLG_HELP:
        help_dialog(gui);
        break;
    case GUI_DLG_ABOUT:
        about_dialog(gui);
        break;
    case GUI_DLG_DETAILS:
        details_dialog(gui);
        break;
    default:
        break;
    }
    if (gui->dialog == opened) {
        gui->dialog_opened = 0;
    }
}
