/**
 * @file gui_actions.c
 * @brief Aksi menu File, alur unsaved changes, dan pembukaan dialog.
 */

#include "gui.h"

#include <stdlib.h>
#include <string.h>

/* ===================================================================== */
/* Dialog Management                                                     */
/* ===================================================================== */

void gui_open_dialog(Gui *gui, GuiDialogId id)
{
    if (gui->dialog == GUI_DLG_CORRECT && id != GUI_DLG_CORRECT) {
        gui_correct_dialog_reset(gui);
    }
    gui->dialog = id;
    gui->dialog_opened = 1;
    gui->dialog_error[0] = '\0';
}

void gui_close_dialog(Gui *gui)
{
    if (gui->dialog == GUI_DLG_CORRECT) {
        gui_correct_dialog_reset(gui);
    }
    gui->dialog = GUI_DLG_NONE;
    if (gui->needs_setup) {
        gui_open_dialog(gui, GUI_DLG_WELCOME);  /* KB awal belum ditetapkan user */
    }
}

void gui_sync_appearance(Gui *gui)
{
    gui_backend_update_fonts(gui, prefs_get(&gui->app->prefs, PREF_UI_FONT_SIZE));
    gui_theme_apply(gui, prefs_get(&gui->app->prefs, PREF_UI_THEME));
}

void gui_reset_view(Gui *gui)
{
    gui->needs_setup = 0;
    gui->active_row = -1;
    gui->rows_revision = (unsigned long)-1;
    gui->text_revision = (unsigned long)-1;
    gui->text_active = 0;
    gui->tree_valid = 0;
    gui->result_valid = 0;
    gui->reasoning_active = 0;
    gui->reasoning_step = 0;
    gui->reasoning_revision = (unsigned long)-1;
    gui_textsel_reset(&gui->result_sel);
    gui_textsel_reset(&gui->tree_sel);
    gui->manual.selected = -1;
    gui->manual.loaded = -1;
}

void gui_reasoning_start(Gui *gui)
{
    gui->reasoning_step = 0;
    gui->reasoning_revision = gui->app->revision;
    gui->reasoning_next_time = glfwGetTime();
    gui->reasoning_active = app_has_result(gui->app);
    gui->tree_valid = 0;
}

void gui_reasoning_tick(Gui *gui)
{
    double now;

    if (!gui->reasoning_active || gui->reasoning_revision != gui->app->revision ||
        !app_has_result(gui->app)) {
        return;
    }
    now = glfwGetTime();
    if (now < gui->reasoning_next_time) {
        return;
    }
    if (gui->reasoning_step < gui->app->inference.step_count) {
        gui->reasoning_step++;
        gui->reasoning_next_time = now + 1.0;
        gui->tree_valid = 0;
    }
    if (gui->reasoning_step >= gui->app->inference.step_count) {
        gui->reasoning_active = 0;
    }
}

/* ===================================================================== */
/* File Actions                                                          */
/* ===================================================================== */

void gui_action_new(Gui *gui)
{
    if (gui->app->dirty) {
        gui->pending = GUI_PENDING_NEW;
        gui_open_dialog(gui, GUI_DLG_UNSAVED);
        return;
    }
    gui->new_default[0] = '\0';
    gui_open_dialog(gui, GUI_DLG_NEW);
}

void gui_action_open(Gui *gui)
{
    if (gui->app->dirty) {
        gui->pending = GUI_PENDING_OPEN;
        gui_open_dialog(gui, GUI_DLG_UNSAVED);
        return;
    }
    gui_open_file_dialog(gui, GUI_FILE_OPEN_PROJECT);
}

void gui_action_save(Gui *gui)
{
    gui_query_commit_text(gui);
    if (gui->app->file_path == NULL) {
        gui_action_save_as(gui);
        return;
    }
    app_save(gui->app);
}

void gui_action_save_as(Gui *gui)
{
    gui_query_commit_text(gui);
    gui_open_file_dialog(gui, GUI_FILE_SAVE_PROJECT);
}

void gui_action_exit(Gui *gui)
{
    if (gui->app->dirty) {
        gui->pending = GUI_PENDING_EXIT;
        gui_open_dialog(gui, GUI_DLG_UNSAVED);
        return;
    }
    gui->quit = 1;
}

void gui_run_pending(Gui *gui)
{
    GuiPending pending = gui->pending;

    gui->pending = GUI_PENDING_NONE;
    gui_close_dialog(gui);
    switch (pending) {
    case GUI_PENDING_NEW:
        gui->new_default[0] = '\0';
        gui_open_dialog(gui, GUI_DLG_NEW);
        break;
    case GUI_PENDING_OPEN:
        gui_open_file_dialog(gui, GUI_FILE_OPEN_PROJECT);
        break;
    case GUI_PENDING_EXIT:
        gui->quit = 1;
        break;
    case GUI_PENDING_NONE:
        break;
    }
}

/* Menyegarkan tampilan setelah undo atau redo. */
static void after_history(Gui *gui)
{
    gui->active_row = -1;
    gui->unfocus_edits = 1;
    gui->text_revision = (unsigned long)-1;
    gui_textsel_reset(&gui->result_sel);
    gui_textsel_reset(&gui->tree_sel);
    gui_sync_appearance(gui);
}

void gui_action_undo(Gui *gui)
{
    gui_query_commit_text(gui);
    app_undo(gui->app);
    after_history(gui);
}

void gui_action_redo(Gui *gui)
{
    gui_query_commit_text(gui);
    app_redo(gui->app);
    after_history(gui);
}

/* ===================================================================== */
/* Edit and Preference Actions                                           */
/* ===================================================================== */

void gui_action_correct(Gui *gui)
{
    gui_correct_dialog_prepare(gui);
    gui_open_dialog(gui, GUI_DLG_CORRECT);
}

void gui_action_manual(Gui *gui, int node_id)
{
    App *app = gui->app;

    if (node_id < 0) {
        node_id = app_has_result(app) ? app->inference.last_concluded : 0;
    }
    gui->manual.adding = 0;
    gui->manual.confirm_delete = 0;
    gui->manual.message[0] = '\0';
    gui_manual_dialog_load(gui, node_id);
    gui_open_dialog(gui, GUI_DLG_MANUAL);
}

void gui_action_prefs(Gui *gui)
{
    gui->prefs_edit = gui->app->prefs;
    gui_open_dialog(gui, GUI_DLG_PREFS);
}

/* ===================================================================== */
/* File Browser Entry Points                                             */
/* ===================================================================== */

void gui_open_file_dialog(Gui *gui, GuiFileMode mode)
{
    GuiFileDialog *fd = &gui->file;
    App *app = gui->app;

    fd->mode = mode;
    fd->error[0] = '\0';
    fd->confirm_overwrite = 0;
    fd->selected = -1;
    fd->name[0] = '\0';

    /* Initial Folder */
    if (fd->dir[0] == '\0') {
        char *dir = app->file_path ? platform_path_parent(app->file_path) : platform_getcwd();
        str_copy(fd->dir, sizeof(fd->dir), dir);
        free(dir);
    }
    if (mode == GUI_FILE_SAVE_PROJECT) {
        str_copy(fd->name, sizeof(fd->name),
                 app->file_path ? platform_path_basename(app->file_path) : "knowledge" APP_FILE_EXT);
    }
    gui_file_dialog_load(gui, fd->dir);
    gui_open_dialog(gui, GUI_DLG_FILE);
}

void gui_file_chosen(Gui *gui, const char *path)
{
    App *app = gui->app;

    switch (gui->file.mode) {
    case GUI_FILE_OPEN_PROJECT:
        if (app_open(app, path)) {
            gui_reset_view(gui);
            gui_sync_appearance(gui);
            gui_close_dialog(gui);
        } else {
            str_copy(gui->file.error, sizeof(gui->file.error), app->status);
        }
        break;
    case GUI_FILE_SAVE_PROJECT:
        if (app_save_as(app, path)) {
            gui_close_dialog(gui);
            if (gui->pending != GUI_PENDING_NONE) {
                gui_run_pending(gui);
            }
        } else {
            str_copy(gui->file.error, sizeof(gui->file.error), app->status);
        }
        break;
    case GUI_FILE_IMPORT_CSV:
        str_copy(gui->csv_path, sizeof(gui->csv_path), path);
        if (app_import_csv(app, path, &gui->csv_report)) {
            gui_open_dialog(gui, GUI_DLG_CSV_REPORT);
        } else {
            str_copy(gui->file.error, sizeof(gui->file.error), app->status);
        }
        break;
    case GUI_FILE_IMPORT_PREFS:
        if (app_import_prefs(app, path)) {
            gui_sync_appearance(gui);
            gui_close_dialog(gui);
        } else {
            str_copy(gui->file.error, sizeof(gui->file.error), app->status);
        }
        break;
    }
}
