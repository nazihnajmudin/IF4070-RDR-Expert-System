/**
 * @file main_gui.c
 * @brief Program utama GUI rdr-expert.
 *
 * Penggunaan:
 *   rdr-expert [project.rdr.json]
 *
 * Build test (make TESTHOOK=1) menerima opsi tambahan:
 *   --test-script <file>   menjalankan skrip aksi (lihat gui_testhook.c)
 *   --shot-dir <folder>    folder tujuan screenshot
 */

#if defined(_WIN32)
#include <windows.h>
#include <shellapi.h>
#endif

#include "gui.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ===================================================================== */
/* Main Window Parts                                                     */
/* ===================================================================== */

/* Tombol kolom tengah pada posisi tertentu, dengan gaya utama jika primary. */
static int column_button(Gui *gui, struct nk_rect r, const char *label, int mnemonic, int enabled, int primary)
{
    struct nk_context *ctx = &gui->ctx;
    struct nk_style_button saved = ctx->style.button;
    struct nk_rect bounds;
    int clicked;

    nk_layout_space_push(ctx, r);
    bounds = nk_widget_bounds(ctx);
    if (primary) {
        struct nk_color p = gui->pal.primary;
        ctx->style.button.normal = nk_style_item_color(p);
        ctx->style.button.hover = nk_style_item_color(nk_rgb(p.r * 8 / 10, p.g * 8 / 10, p.b * 8 / 10));
        ctx->style.button.active = ctx->style.button.hover;
        ctx->style.button.border_color = p;
        ctx->style.button.text_normal = nk_rgb(255, 255, 255);
        ctx->style.button.text_hover = nk_rgb(255, 255, 255);
        ctx->style.button.text_active = nk_rgb(255, 255, 255);
        nk_style_push_font(ctx, &gui->font_bold->handle);
    }
    clicked = gui_button(gui, label, enabled);
    if (primary) {
        nk_style_pop_font(ctx);
        ctx->style.button = saved;
    }
    gui_mnemonic(gui, bounds, label, mnemonic, 1, primary ? &gui->font_bold->handle : &gui->font->handle);
    return clicked;
}

/* Tombol Start, Clear, Undo, dan Redo yang dipusatkan terhadap kotak panel. */
static void action_column(Gui *gui, float x, float w, float body_y, float body_h)
{
    float bh = gui->row + 10;
    float pair_gap = 6;
    float group_gap = 30;
    float total = 4 * bh + 2 * pair_gap + group_gap;
    float y = (float)(int)(body_y + (body_h - total) / 2.0f);

    if (column_button(gui, nk_rect(x, y, w, bh), "Start", 0, 1, 1)) {
        gui_query_start(gui);
    }
    y += bh + pair_gap;
    if (column_button(gui, nk_rect(x, y, w, bh), "Clear", 3, 1, 0)) {
        gui->qbar[0] = '\0';
        gui->active_row = -1;
        app_clear(gui->app);
    }
    y += bh + group_gap;
    if (column_button(gui, nk_rect(x, y, w, bh), "Undo", 0, app_can_undo(gui->app), 0)) {
        gui_action_undo(gui);
    }
    y += bh + pair_gap;
    if (column_button(gui, nk_rect(x, y, w, bh), "Redo", 3, app_can_redo(gui->app), 0)) {
        gui_action_redo(gui);
    }
}

/* Menggeser batas kedua panel. Lebar kolom tengah dan jaraknya tetap. */
static void splitter(Gui *gui, struct nk_rect space, float left_w, float right_x, float gap, float body_y,
                     float body_h, float avail)
{
    const struct nk_input *in = &gui->ctx.input;
    struct nk_rect left_zone = nk_rect(space.x + left_w - 3, space.y + body_y, gap + 3, body_h);
    struct nk_rect right_zone = nk_rect(space.x + right_x - gap, space.y + body_y, gap + 3, body_h);
    int interactive = gui->dialog == GUI_DLG_NONE && gui->menu_open < 0;
    int hover = interactive && (nk_input_is_mouse_hovering_rect(in, left_zone) ||
                                nk_input_is_mouse_hovering_rect(in, right_zone));
    int want_cursor;

    if (hover && nk_input_is_mouse_pressed(in, NK_BUTTON_LEFT)) {
        gui->split_drag = 1;
        gui->split_x0 = in->mouse.pos.x;
        gui->split_w0 = left_w;
    }
    if (gui->split_drag) {
        if (!nk_input_is_mouse_down(in, NK_BUTTON_LEFT)) {
            gui->split_drag = 0;
        } else {
            double ratio = (gui->split_w0 + (in->mouse.pos.x - gui->split_x0)) / avail;
            ratio = ratio < SESSION_MIN_SPLIT ? SESSION_MIN_SPLIT : ratio > SESSION_MAX_SPLIT ? SESSION_MAX_SPLIT : ratio;
            gui->app->split_ratio = ratio;
        }
    }

    /* Cursor Shape */
    want_cursor = hover || gui->split_drag;
    if (want_cursor != gui->cursor_is_resize) {
        glfwSetCursor(gui->window, want_cursor ? gui->cursor_resize : NULL);
        gui->cursor_is_resize = want_cursor;
    }
}

static void status_bar(Gui *gui)
{
    struct nk_context *ctx = &gui->ctx;
    App *app = gui->app;
    struct nk_rect bounds;
    struct nk_color color;
    const char *name = app->file_path ? platform_path_basename(app->file_path) : "Untitled";
    char *file = str_printf("%s%s", name, app->dirty ? " (modified)" : "");
    char *nodes = str_printf("%lu nodes", (unsigned long)app->kb->node_count);
    int has_details = app->details.count > 0;

    nk_layout_row_template_begin(ctx, gui->row);
    nk_layout_row_template_push_static(ctx, gui_text_width(gui, file) + 16);
    nk_layout_row_template_push_static(ctx, gui_text_width(gui, nodes) + 16);
    nk_layout_row_template_push_variable(ctx, 100);
    if (has_details) {
        nk_layout_row_template_push_static(ctx, gui_text_width(gui, "Details") + 28);
    }
    nk_layout_row_template_end(ctx);

    /* Background */
    bounds = nk_widget_bounds(ctx);
    nk_fill_rect(nk_window_get_canvas(ctx), nk_rect(0, bounds.y - 3, (float)gui->width, bounds.h + 8), 0,
                 gui->pal.status_bg);

    /* Fields */
    gui_label_bold(gui, file, app->dirty ? gui->pal.warning : gui->pal.text);
    gui_label_colored(gui, nodes, gui->pal.muted);
    color = app->status_level == APP_STATUS_ERROR ? gui->pal.error
            : app->status_level == APP_STATUS_WARNING ? gui->pal.warning
                                                      : gui->pal.text;
    gui_label_colored(gui, app->status, color);
    if (has_details && nk_button_label(ctx, "Details")) {
        gui_open_dialog(gui, GUI_DLG_DETAILS);
    }
    free(file);
    free(nodes);
}

static void main_window(Gui *gui)
{
    struct nk_context *ctx = &gui->ctx;
    struct nk_style *st = &ctx->style;
    nk_flags flags = NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_BACKGROUND;
    float w = (float)gui->width;
    float h = (float)gui->height;

    if (gui->dialog != GUI_DLG_NONE || gui->menu_open >= 0) {
        flags |= NK_WINDOW_ROM;
    }
    st->window.border = 0;
    if (nk_begin(ctx, "main", nk_rect(0, 0, w, h), flags)) {
        struct nk_command_buffer *canvas = nk_window_get_canvas(ctx);
        float qbar_h = gui->row + 4;
        float area_h = (float)(int)(h - 2 * st->window.padding.y - gui->row - qbar_h - gui->row
                                    - 3 * st->window.spacing.y - 12);
        nk_window_set_bounds(ctx, "main", nk_rect(0, 0, w, h));
        if (gui->unfocus_edits) {
            nk_edit_unfocus(ctx);
            gui->unfocus_edits = 0;
        }

        /* Menubar */
        nk_fill_rect(canvas, nk_rect(0, 0, w, st->window.padding.y + gui->row + st->window.spacing.y / 2), 0,
                     st->window.header.normal.data.color);
        gui_menubar(gui);

        /* Panels */
        nk_layout_space_begin(ctx, NK_STATIC, area_h, INT_MAX);
        {
            struct nk_rect space = nk_layout_space_bounds(ctx);
            float width = (float)(int)space.w;
            float gap = 10;
            float mid_w = (float)(int)(gui_text_width(gui, "Start") + 56);
            float avail = width - mid_w - 2 * gap;
            float left_w = (float)(int)(avail * (float)gui->app->split_ratio);
            float right_x = left_w + gap + mid_w + gap;
            float top = 6;
            float body_y = top + gui->row + 6;
            float body_h = area_h - body_y;

            gui_query_panel(gui, nk_rect(0, top, left_w, area_h - top), body_y);
            action_column(gui, left_w + gap, mid_w, body_y, body_h);
            gui_result_panel(gui, nk_rect(right_x, top, width - right_x, area_h - top), body_y);
            splitter(gui, space, left_w, right_x, gap, body_y, body_h, avail);
        }
        nk_layout_space_end(ctx);

        /* Q Bar */
        nk_layout_space_begin(ctx, NK_STATIC, qbar_h, INT_MAX);
        gui_query_bar(gui, (float)(int)nk_layout_space_bounds(ctx).w);
        nk_layout_space_end(ctx);

        /* Status Bar */
        status_bar(gui);

        /* Dialog Overlay */
        if (gui->dialog != GUI_DLG_NONE) {
            nk_fill_rect(canvas, nk_rect(0, 0, w, h), 0, gui->pal.overlay);
        }
    }
    nk_end(ctx);
    st->window.border = 1.0f;
}

static void update_title(Gui *gui)
{
    char *title = app_title(gui->app);

    if (strcmp(title, gui->title) != 0) {
        str_copy(gui->title, sizeof(gui->title), title);
        glfwSetWindowTitle(gui->window, title);
    }
    free(title);
}

/* ===================================================================== */
/* Command Line                                                          */
/* ===================================================================== */

/**
 * @brief Argumen program dalam UTF-8.
 */
typedef struct {
    const char *open_path;    /**< Project yang dibuka saat start. */
    const char *script;       /**< Skrip test hook. */
    const char *shot_dir;     /**< Folder screenshot test hook. */
} GuiArgs;

static void parse_args(int argc, char **argv, GuiArgs *args)
{
    int i;

    args->open_path = NULL;
    args->script = NULL;
    args->shot_dir = ".";
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--test-script") == 0 && i + 1 < argc) {
            args->script = argv[++i];
        } else if (strcmp(argv[i], "--shot-dir") == 0 && i + 1 < argc) {
            args->shot_dir = argv[++i];
        } else {
            args->open_path = argv[i];
        }
    }
}

#if defined(_WIN32)
/* Mengambil argumen dalam UTF-8 karena argv Windows memakai code page lokal. */
static char **utf8_argv(int *argc)
{
    LPWSTR *wargv = CommandLineToArgvW(GetCommandLineW(), argc);
    char **argv;
    int i;

    if (wargv == NULL) {
        *argc = 0;
        return NULL;
    }
    argv = xcalloc((size_t)*argc + 1, sizeof(char *));
    for (i = 0; i < *argc; i++) {
        int n = WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, NULL, 0, NULL, NULL);
        argv[i] = xmalloc((size_t)(n > 0 ? n : 1));
        WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, argv[i], n, NULL, NULL);
    }
    LocalFree(wargv);
    return argv;
}
#endif

/* ===================================================================== */
/* Main Program                                                          */
/* ===================================================================== */

int main(int argc, char **argv)
{
    Gui *gui;
    GuiArgs args;
    int exit_code = 0;

#if defined(_WIN32)
    argv = utf8_argv(&argc);
#endif
    parse_args(argc, argv, &args);

    /* State Setup */
    gui = xcalloc(1, sizeof(Gui));
    gui->app = app_create("Unknown");
    gui->active_row = -1;
    gui->tree_click_id = -1;
    gui->manual.selected = -1;
    gui->manual.loaded = -1;
    gui->file.last_click_index = -1;
    gui->menu_open = -1;
    gui->menu_cursor = -1;
    gui_textdoc_init(&gui->result_doc);
    gui_textdoc_init(&gui->tree_doc);
    gui_textsel_reset(&gui->result_sel);
    gui_textsel_reset(&gui->tree_sel);
    gui->result_sel.last_click_line = -1;
    gui->tree_sel.last_click_line = -1;
    strlist_init(&gui->file.roots);
    platform_dirlist_init(&gui->file.entries);
    csv_report_init(&gui->csv_report);
    rdr_lines_init(&gui->tree);

    /* Window Setup */
    if (!gui_backend_init(gui, 1200, 760)) {
        fprintf(stderr, "rdr-expert: cannot create an OpenGL window\n");
        app_destroy(gui->app);
        free(gui);
        return 1;
    }
    gui_sync_appearance(gui);
    gui->needs_setup = 1;
    if (args.open_path != NULL) {
        gui->file.mode = GUI_FILE_OPEN_PROJECT;
        gui_file_chosen(gui, args.open_path);
    }
    if (gui->needs_setup) {
        gui_open_dialog(gui, GUI_DLG_WELCOME);
    }
    if (!gui_testhook_init(gui, args.script, args.shot_dir)) {
        exit_code = 1;
        gui->quit = 1;
    }

    /* Main Loop */
    while (!gui->quit) {
        int scripted = gui_testhook_step(gui);
        int queued = gui->ev_head != gui->ev_tail;
        int wait = !scripted && !queued && gui->idle_frames > 2;

        if (gui_backend_input(gui, wait)) {
            gui->idle_frames = 0;
        } else {
            gui->idle_frames++;
        }
        gui_handle_shortcut(gui);
        main_window(gui);
        gui_menu_popup(gui);
        gui_dialogs(gui);
        gui_backend_render(gui);
        gui_testhook_after_render(gui);
        gui_backend_present(gui);
        update_title(gui);
    }

    /* Cleanup */
    if (gui_testhook_shutdown(gui) > 0) {
        exit_code = 1;
    }
    gui_correct_dialog_reset(gui);
    if (gui->manual.has_validation) {
        rdr_validation_free(&gui->manual.validation);
    }
    rdr_lines_free(&gui->tree);
    gui_textdoc_free(&gui->result_doc);
    gui_textdoc_free(&gui->tree_doc);
    csv_report_free(&gui->csv_report);
    platform_dirlist_free(&gui->file.entries);
    strlist_free(&gui->file.roots);
    free(gui->rows);
    gui_backend_shutdown(gui);
    app_destroy(gui->app);
    free(gui);
    return exit_code;
}
