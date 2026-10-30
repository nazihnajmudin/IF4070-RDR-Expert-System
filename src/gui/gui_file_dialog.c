/**
 * @file gui_file_dialog.c
 * @brief File browser buatan sendiri untuk Open, Save As, Insert CSV, dan Import Preference.
 */

#include "gui.h"

#include <stdlib.h>
#include <string.h>

/* ===================================================================== */
/* Mode Configuration                                                    */
/* ===================================================================== */

static const char *mode_title(GuiFileMode mode)
{
    switch (mode) {
    case GUI_FILE_OPEN_PROJECT:
        return "Open Project";
    case GUI_FILE_SAVE_PROJECT:
        return "Save Project As";
    case GUI_FILE_IMPORT_CSV:
        return "Insert CSV Dataset";
    case GUI_FILE_IMPORT_PREFS:
        return "Import Preference From Project";
    }
    return "File";
}

static const char *mode_button(GuiFileMode mode)
{
    switch (mode) {
    case GUI_FILE_SAVE_PROJECT:
        return "Save";
    case GUI_FILE_IMPORT_CSV:
    case GUI_FILE_IMPORT_PREFS:
        return "Import";
    default:
        return "Open";
    }
}

static const char *mode_ext(GuiFileMode mode)
{
    return mode == GUI_FILE_IMPORT_CSV ? ".csv" : ".json";
}

static int is_absolute(const char *path)
{
    if (path[0] == '/' || path[0] == '\\') {
        return 1;
    }
    return path[0] != '\0' && path[1] == ':';
}

/* ===================================================================== */
/* Folder Loading                                                        */
/* ===================================================================== */

void gui_file_dialog_load(Gui *gui, const char *dir)
{
    GuiFileDialog *fd = &gui->file;
    PlatformDirList list;
    char err[512];

    platform_dirlist_init(&list);
    if (!platform_list_dir(dir, &list, err, sizeof(err))) {
        str_copy(fd->error, sizeof(fd->error), err);
        platform_dirlist_free(&list);
        str_copy(fd->dir_edit, sizeof(fd->dir_edit), fd->dir);
        return;
    }
    platform_dirlist_free(&fd->entries);
    fd->entries = list;
    if (fd->dir != dir) {
        str_copy(fd->dir, sizeof(fd->dir), dir);
    }
    str_copy(fd->dir_edit, sizeof(fd->dir_edit), fd->dir);
    fd->selected = -1;
    fd->error[0] = '\0';
    fd->confirm_overwrite = 0;
    strlist_clear(&fd->roots);
    platform_list_roots(&fd->roots);
}

static void go_to(Gui *gui, const char *dir)
{
    char *copy = str_dup(dir);
    gui_file_dialog_load(gui, copy);
    free(copy);
}

/* ===================================================================== */
/* Confirmation                                                          */
/* ===================================================================== */

static void confirm(Gui *gui)
{
    GuiFileDialog *fd = &gui->file;
    char *path;
    char *name = str_trim_dup(fd->name);

    if (name[0] == '\0') {
        str_copy(fd->error, sizeof(fd->error), "Choose or type a file name first.");
        free(name);
        return;
    }
    path = is_absolute(name) ? str_dup(name) : platform_path_join(fd->dir, name);
    free(name);

    /* Folder Navigation */
    if (platform_is_dir(path)) {
        gui_file_dialog_load(gui, path);
        fd->name[0] = '\0';
        free(path);
        return;
    }

    /* Mode Specific Checks */
    if (fd->mode == GUI_FILE_SAVE_PROJECT) {
        if (!platform_path_has_ext(path, ".json")) {
            char *with_ext = str_printf("%s%s", path, APP_FILE_EXT);
            free(path);
            path = with_ext;
        }
        if (platform_file_exists(path) && !fd->confirm_overwrite) {
            fd->confirm_overwrite = 1;
            str_set_error(fd->error, sizeof(fd->error), "'%s' already exists. Press Save again to replace it.",
                          platform_path_basename(path));
            free(path);
            return;
        }
    } else if (!platform_file_exists(path)) {
        str_set_error(fd->error, sizeof(fd->error), "'%s' does not exist.", platform_path_basename(path));
        free(path);
        return;
    }

    fd->error[0] = '\0';
    gui_file_chosen(gui, path);
    free(path);
}

/* ===================================================================== */
/* Dialog Drawing                                                        */
/* ===================================================================== */

void gui_file_dialog(Gui *gui)
{
    struct nk_context *ctx = &gui->ctx;
    GuiFileDialog *fd = &gui->file;
    float h = (float)gui->height * 0.85f;
    float list_h;
    const char *ext = mode_ext(fd->mode);
    size_t i;
    nk_flags flags;

    if (gui_dialog_begin(gui, mode_title(fd->mode), 820, h)) {
        struct nk_style *s = &ctx->style;
        float title_h = gui->font->handle.height + 2 * s->window.header.padding.y + 2 * s->window.header.label_padding.y;

        /* Folder Bar */
        nk_layout_row_template_begin(ctx, gui->row + 2);
        nk_layout_row_template_push_static(ctx, gui_text_width(gui, "Up") + 30);
        nk_layout_row_template_push_static(ctx, gui_text_width(gui, "Home") + 30);
        if (fd->roots.count > 1) {
            nk_layout_row_template_push_static(ctx, gui_text_width(gui, "C:\\") + 60);
        }
        nk_layout_row_template_push_variable(ctx, 200);
        nk_layout_row_template_end(ctx);
        if (nk_button_label(ctx, "Up")) {
            char *parent = platform_path_parent(fd->dir);
            go_to(gui, parent);
            free(parent);
        }
        if (nk_button_label(ctx, "Home")) {
            char *home = platform_home_dir();
            go_to(gui, home);
            free(home);
        }
        if (fd->roots.count > 1) {
            int cur = -1;
            int picked;
            for (i = 0; i < fd->roots.count; i++) {
                if (strncmp(fd->dir, fd->roots.items[i], 2) == 0) {
                    cur = (int)i;
                }
            }
            picked = nk_combo(ctx, (const char **)fd->roots.items, (int)fd->roots.count, cur < 0 ? 0 : cur,
                              (int)gui->row, nk_vec2(140, 220));
            if (picked != cur && cur >= 0) {
                go_to(gui, fd->roots.items[picked]);
            }
        }
        flags = gui_edit(gui, NK_EDIT_FIELD | NK_EDIT_SIG_ENTER, fd->dir_edit, GUI_PATH_MAX);
        if (flags & NK_EDIT_COMMITED) {
            go_to(gui, fd->dir_edit);
        }

        /* Entry List */
        list_h = h - title_h - 4 * (gui->row + 8) - 2 * s->window.padding.y - 16;
        nk_layout_row_dynamic(ctx, list_h, 1);
        ctx->style.window.fixed_background = nk_style_item_color(gui->pal.panel);
        if (nk_group_begin(ctx, "file_entries", NK_WINDOW_BORDER)) {
            for (i = 0; i < fd->entries.count; i++) {
                const PlatformDirEntry *e = &fd->entries.items[i];
                char *label;
                nk_bool selected = (int)i == fd->selected;
                if (!e->is_dir && !fd->show_all && !platform_path_has_ext(e->name, ext)) {
                    continue;
                }
                label = e->is_dir ? str_printf("[%s]", e->name) : str_dup(e->name);
                nk_layout_row_dynamic(ctx, gui->row - 2, 1);
                if (nk_selectable_label(ctx, label, NK_TEXT_LEFT, &selected)) {
                    double now = glfwGetTime();
                    int is_double = fd->last_click_index == (int)i && now - fd->last_click < 0.45;
                    fd->last_click = now;
                    fd->last_click_index = (int)i;
                    fd->selected = (int)i;
                    fd->confirm_overwrite = 0;
                    if (e->is_dir) {
                        if (is_double) {
                            char *sub = platform_path_join(fd->dir, e->name);
                            free(label);
                            go_to(gui, sub);
                            free(sub);
                            break;
                        }
                    } else {
                        str_copy(fd->name, sizeof(fd->name), e->name);
                        if (is_double) {
                            free(label);
                            confirm(gui);
                            break;
                        }
                    }
                }
                free(label);
            }
            nk_group_end(ctx);
        }
        ctx->style.window.fixed_background = nk_style_item_color(ctx->style.window.background);

        /* File Name */
        nk_layout_row_template_begin(ctx, gui->row + 2);
        nk_layout_row_template_push_static(ctx, gui_text_width(gui, "File name") + 12);
        nk_layout_row_template_push_variable(ctx, 200);
        nk_layout_row_template_end(ctx);
        gui_label_colored(gui, "File name", gui->pal.text);
        flags = gui_edit(gui, NK_EDIT_FIELD | NK_EDIT_SIG_ENTER, fd->name, GUI_PATH_MAX);
        if (flags & NK_EDIT_ACTIVE) {
            fd->confirm_overwrite = fd->confirm_overwrite && !(flags & NK_EDIT_ACTIVATED);
        }
        if (flags & NK_EDIT_COMMITED) {
            confirm(gui);
        }

        /* Status Line */
        nk_layout_row_dynamic(ctx, gui->row, 1);
        if (fd->error[0] != '\0') {
            gui_label_colored(gui, fd->error, fd->confirm_overwrite ? gui->pal.warning : gui->pal.error);
        } else {
            char *hint = str_printf("Showing folders and %s files. Double click to open.",
                                    fd->show_all ? "all" : ext);
            gui_label_colored(gui, hint, gui->pal.muted);
            free(hint);
        }

        /* Buttons */
        nk_layout_row_template_begin(ctx, gui->row + 4);
        nk_layout_row_template_push_static(ctx, gui_text_width(gui, "Show all files") + 40);
        nk_layout_row_template_push_variable(ctx, 10);
        nk_layout_row_template_push_static(ctx, gui_text_width(gui, "Cancel") + 36);
        nk_layout_row_template_push_static(ctx, gui_text_width(gui, mode_button(fd->mode)) + 36);
        nk_layout_row_template_end(ctx);
        {
            nk_bool all = fd->show_all;
            nk_checkbox_label(ctx, "Show all files", &all);
            fd->show_all = all;
        }
        nk_spacing(ctx, 1);
        if (nk_button_label(ctx, "Cancel")) {
            gui->pending = GUI_PENDING_NONE;
            gui_close_dialog(gui);
        }
        if (nk_button_label(ctx, mode_button(fd->mode))) {
            confirm(gui);
        }
    }
    gui_dialog_end(gui);
}
