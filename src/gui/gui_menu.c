/**
 * @file gui_menu.c
 * @brief Menubar, popup menu, shortcut keyboard, dan navigasi Alt.
 *
 * Menu digambar sendiri (bukan menu bawaan Nuklear) agar dapat dibuka dengan
 * Alt+huruf dan dinavigasi dengan keyboard:
 *   Alt+F, Alt+E, Alt+P, Alt+H   membuka menu File, Edit, Preference, Help
 *   Atas dan Bawah                memilih item
 *   Kiri dan Kanan                berpindah menu
 *   Enter                         menjalankan item
 *   Esc                           menutup menu
 *
 * Alt+huruf lain memindahkan fokus ke komponen utama:
 *   Alt+Q bar Q, Alt+C tab Clause, Alt+Y tab Query, Alt+R tab Result,
 *   Alt+T tab Tree, Alt+S Start, Alt+L Clear, Alt+U Undo, Alt+O Redo
 */

#include "gui.h"

#include <string.h>

/* ===================================================================== */
/* Menu Definition                                                       */
/* ===================================================================== */

/**
 * @brief Aksi item menu.
 */
typedef enum {
    MI_NEW, MI_OPEN, MI_SAVE, MI_SAVE_AS, MI_EXIT,
    MI_UNDO, MI_REDO, MI_CORRECT, MI_MANUAL, MI_CSV,
    MI_PREFS, MI_IMPORT_PREFS,
    MI_HELP, MI_ABOUT
} MenuAction;

/**
 * @brief Satu item menu.
 */
typedef struct {
    const char *label;     /**< Teks item. */
    const char *shortcut;  /**< Teks shortcut, NULL jika tidak ada. */
    MenuAction action;     /**< Aksi yang dijalankan. */
} MenuItem;

static const MenuItem FILE_ITEMS[] = {
    {"New...", "Ctrl+N", MI_NEW},
    {"Open...", "Ctrl+O", MI_OPEN},
    {"Save", "Ctrl+S", MI_SAVE},
    {"Save As...", "Ctrl+Shift+S", MI_SAVE_AS},
    {"Exit", NULL, MI_EXIT},
};
static const MenuItem EDIT_ITEMS[] = {
    {"Undo", "Ctrl+Z", MI_UNDO},
    {"Redo", "Ctrl+Y", MI_REDO},
    {"Edit last query", NULL, MI_CORRECT},
    {"Manual Edit", NULL, MI_MANUAL},
    {"Insert CSV", NULL, MI_CSV},
};
static const MenuItem PREF_ITEMS[] = {
    {"Edit Preference", NULL, MI_PREFS},
    {"Import Preference", NULL, MI_IMPORT_PREFS},
};
static const MenuItem HELP_ITEMS[] = {
    {"Help", "F1", MI_HELP},
    {"About", NULL, MI_ABOUT},
};

/**
 * @brief Satu menu pada menubar.
 */
typedef struct {
    const char *title;      /**< Judul menu. */
    int mnemonic_key;       /**< Huruf Alt untuk membuka menu. */
    const MenuItem *items;  /**< Item menu. */
    int count;              /**< Jumlah item. */
} Menu;

static const Menu MENUS[4] = {
    {"File", 'f', FILE_ITEMS, 5},
    {"Edit", 'e', EDIT_ITEMS, 5},
    {"Preference", 'p', PREF_ITEMS, 2},
    {"Help", 'h', HELP_ITEMS, 2},
};

/* ===================================================================== */
/* Menu Actions                                                          */
/* ===================================================================== */

static int item_enabled(Gui *gui, MenuAction action)
{
    App *app = gui->app;

    switch (action) {
    case MI_UNDO:
        return app_can_undo(app);
    case MI_REDO:
        return app_can_redo(app);
    case MI_CORRECT:
        return rdr_inference_is_current(app->kb, &app->inference);
    default:
        return 1;
    }
}

static void run_item(Gui *gui, MenuAction action)
{
    gui->menu_open = -1;
    switch (action) {
    case MI_NEW:
        gui_action_new(gui);
        break;
    case MI_OPEN:
        gui_action_open(gui);
        break;
    case MI_SAVE:
        gui_action_save(gui);
        break;
    case MI_SAVE_AS:
        gui_action_save_as(gui);
        break;
    case MI_EXIT:
        gui_action_exit(gui);
        break;
    case MI_UNDO:
        gui_action_undo(gui);
        break;
    case MI_REDO:
        gui_action_redo(gui);
        break;
    case MI_CORRECT:
        gui_action_correct(gui);
        break;
    case MI_MANUAL:
        gui_action_manual(gui, -1);
        break;
    case MI_CSV:
        gui_open_file_dialog(gui, GUI_FILE_IMPORT_CSV);
        break;
    case MI_PREFS:
        gui_action_prefs(gui);
        break;
    case MI_IMPORT_PREFS:
        gui_open_file_dialog(gui, GUI_FILE_IMPORT_PREFS);
        break;
    case MI_HELP:
        gui_open_dialog(gui, GUI_DLG_HELP);
        break;
    case MI_ABOUT:
        gui_open_dialog(gui, GUI_DLG_ABOUT);
        break;
    }
}

/* Memilih item aktif berikutnya searah step, mulai dari posisi from. */
static int next_enabled(Gui *gui, const Menu *m, int from, int step)
{
    int i;
    int idx = from;

    for (i = 0; i < m->count; i++) {
        idx = (idx + step + m->count) % m->count;
        if (item_enabled(gui, m->items[idx].action)) {
            return idx;
        }
    }
    return -1;
}

static void open_menu(Gui *gui, int index)
{
    gui->unfocus_edits = 1;
    gui->menu_open = index;
    gui->menu_cursor = next_enabled(gui, &MENUS[index], -1, 1);
}

/* ===================================================================== */
/* Menubar                                                               */
/* ===================================================================== */

void gui_menubar(Gui *gui)
{
    struct nk_context *ctx = &gui->ctx;
    const struct nk_input *in = &ctx->input;
    struct nk_command_buffer *canvas = nk_window_get_canvas(ctx);
    int i;

    nk_layout_row_begin(ctx, NK_STATIC, gui->row, 4);
    for (i = 0; i < 4; i++) {
        struct nk_rect b;
        int hovered;
        int is_open = gui->menu_open == i;
        float tw = gui_text_width(gui, MENUS[i].title);

        nk_layout_row_push(ctx, tw + 26.0f);
        nk_widget(&b, ctx);
        gui->menu_headers[i] = b;
        hovered = gui->dialog == GUI_DLG_NONE && nk_input_is_mouse_hovering_rect(in, b);

        /* Header Interaction */
        if (gui->dialog == GUI_DLG_NONE && nk_input_is_mouse_pressed(in, NK_BUTTON_LEFT) && hovered) {
            if (is_open) {
                gui->menu_open = -1;
            } else {
                open_menu(gui, i);
            }
        } else if (gui->menu_open >= 0 && !is_open && hovered &&
                   (in->mouse.delta.x != 0 || in->mouse.delta.y != 0)) {
            open_menu(gui, i);  /* menu terbuka berpindah hanya saat mouse bergerak */
        }
        is_open = gui->menu_open == i;

        /* Header Drawing */
        if (is_open || hovered) {
            nk_fill_rect(canvas, b, 3, ctx->style.button.active.data.color);
        }
        nk_draw_text(canvas, nk_rect(b.x + 13, b.y + (b.h - gui->font->handle.height) / 2, tw + 2,
                                     gui->font->handle.height),
                     MENUS[i].title, (int)strlen(MENUS[i].title), &gui->font->handle, nk_rgba(0, 0, 0, 0),
                     gui->pal.text);
        gui_mnemonic(gui, nk_rect(b.x + 13 - ctx->style.button.padding.x - 4, b.y, b.w, b.h), MENUS[i].title, 0, 0,
                     &gui->font->handle);
    }
    nk_layout_row_end(ctx);
}

/* ===================================================================== */
/* Popup Menu                                                            */
/* ===================================================================== */

void gui_menu_popup(Gui *gui)
{
    struct nk_context *ctx = &gui->ctx;
    const struct nk_input *in = &ctx->input;
    const Menu *m;
    struct nk_rect header;
    struct nk_rect r;
    float label_w = 0;
    float key_w = 0;
    float item_h = gui->row + 2;
    int i;
    int run = -1;

    if (gui->menu_open < 0 || gui->dialog != GUI_DLG_NONE) {
        gui->menu_open = -1;
        return;
    }
    m = &MENUS[gui->menu_open];
    header = gui->menu_headers[gui->menu_open];
    for (i = 0; i < m->count; i++) {
        float lw = gui_text_width(gui, m->items[i].label);
        float kw = m->items[i].shortcut ? gui_text_width(gui, m->items[i].shortcut) : 0;
        label_w = lw > label_w ? lw : label_w;
        key_w = kw > key_w ? kw : key_w;
    }
    r = nk_rect(header.x, header.y + header.h + 2, label_w + key_w + 70,
                (float)m->count * item_h + 2 * ctx->style.window.padding.y + 4);

    /* Outside Click */
    if (nk_input_is_mouse_pressed(in, NK_BUTTON_LEFT) && !nk_input_is_mouse_hovering_rect(in, r)) {
        int on_header = 0;
        for (i = 0; i < 4; i++) {
            on_header = on_header || nk_input_is_mouse_hovering_rect(in, gui->menu_headers[i]);
        }
        if (!on_header) {
            gui->menu_open = -1;
            return;
        }
    }

    /* Keyboard Navigation */
    if (nk_input_is_key_pressed(in, NK_KEY_DOWN)) {
        gui->menu_cursor = next_enabled(gui, m, gui->menu_cursor, 1);
    } else if (nk_input_is_key_pressed(in, NK_KEY_UP)) {
        gui->menu_cursor = next_enabled(gui, m, gui->menu_cursor < 0 ? 0 : gui->menu_cursor, -1);
    } else if (nk_input_is_key_pressed(in, NK_KEY_RIGHT)) {
        open_menu(gui, (gui->menu_open + 1) % 4);
        return;
    } else if (nk_input_is_key_pressed(in, NK_KEY_LEFT)) {
        open_menu(gui, (gui->menu_open + 3) % 4);
        return;
    } else if (nk_input_is_key_pressed(in, NK_KEY_ENTER) && gui->menu_cursor >= 0) {
        run = gui->menu_cursor;
    }

    /* Items */
    if (nk_begin(ctx, "menu_popup", r, NK_WINDOW_BORDER | NK_WINDOW_NO_SCROLLBAR)) {
        struct nk_command_buffer *canvas = nk_window_get_canvas(ctx);
        struct nk_vec2 saved = ctx->style.window.spacing;
        nk_window_set_bounds(ctx, "menu_popup", r);
        nk_window_set_focus(ctx, "menu_popup");
        ctx->style.window.spacing = nk_vec2(0, 0);
        for (i = 0; i < m->count; i++) {
            const MenuItem *item = &m->items[i];
            int enabled = item_enabled(gui, item->action);
            struct nk_rect b;
            float ty;
            nk_layout_row_dynamic(ctx, item_h, 1);
            nk_widget(&b, ctx);
            if (enabled && nk_input_is_mouse_hovering_rect(in, b) &&
                (in->mouse.delta.x != 0 || in->mouse.delta.y != 0)) {
                gui->menu_cursor = i;
            }
            if (i == gui->menu_cursor) {
                nk_fill_rect(canvas, b, 3, gui->ctx.style.selectable.normal_active.data.color);
            }
            ty = b.y + (b.h - gui->font->handle.height) / 2;
            nk_draw_text(canvas, nk_rect(b.x + 10, ty, label_w + 4, gui->font->handle.height), item->label,
                         (int)strlen(item->label), &gui->font->handle, nk_rgba(0, 0, 0, 0),
                         enabled ? gui->pal.text : gui->pal.muted);
            if (item->shortcut != NULL) {
                float kw = gui_text_width(gui, item->shortcut);
                nk_draw_text(canvas, nk_rect(b.x + b.w - kw - 10, ty, kw + 2, gui->font->handle.height),
                             item->shortcut, (int)strlen(item->shortcut), &gui->font->handle, nk_rgba(0, 0, 0, 0),
                             gui->pal.muted);
            }
            if (enabled && nk_input_mouse_clicked(in, NK_BUTTON_LEFT, b)) {
                run = i;
            }
        }
        ctx->style.window.spacing = saved;
    }
    nk_end(ctx);
    if (run >= 0 && item_enabled(gui, m->items[run].action)) {
        run_item(gui, m->items[run].action);
    }
}

/* ===================================================================== */
/* Shortcuts                                                             */
/* ===================================================================== */

static void alt_shortcut(Gui *gui, int key)
{
    App *app = gui->app;
    int i;

    for (i = 0; i < 4; i++) {
        if (MENUS[i].mnemonic_key == key) {
            open_menu(gui, i);
            return;
        }
    }
    gui->menu_open = -1;
    switch (key) {
    case 'q':
        gui->qbar_focus = 1;
        gui->text_focus = GUI_FOCUS_NONE;
        break;
    case 'c':
        gui_query_commit_text(gui);
        gui->focus_clause = app->query_tab == SESSION_QUERY_TAB_CLAUSE ? 1 : 2;  /* fokus ditunda satu frame jika tab berganti */
        app->query_tab = SESSION_QUERY_TAB_CLAUSE;
        gui->text_focus = GUI_FOCUS_NONE;
        break;
    case 'y':
        gui->focus_text = app->query_tab == SESSION_QUERY_TAB_TEXT ? 1 : 2;
        app->query_tab = SESSION_QUERY_TAB_TEXT;
        gui->text_revision = (unsigned long)-1;
        gui->text_focus = GUI_FOCUS_NONE;
        break;
    case 'r':
        app->result_tab = SESSION_RESULT_TAB_RESULT;
        gui->text_focus = GUI_FOCUS_RESULT;
        gui->unfocus_edits = 1;
        break;
    case 't':
        app->result_tab = SESSION_RESULT_TAB_TREE;
        gui->text_focus = GUI_FOCUS_TREE;
        gui->unfocus_edits = 1;
        break;
    case 's':
        gui_query_start(gui);
        break;
    case 'l':
        gui->qbar[0] = '\0';
        gui->active_row = -1;
        app_clear(app);
        break;
    case 'u':
        gui_action_undo(gui);
        break;
    case 'o':
        gui_action_redo(gui);
        break;
    default:
        break;
    }
}

void gui_handle_shortcut(Gui *gui)
{
    GuiShortcut sc = gui->shortcut;

    gui->shortcut = GUI_SC_NONE;
    if (sc == GUI_SC_NONE) {
        return;
    }

    /* Dialog Shortcuts */
    if (gui->dialog != GUI_DLG_NONE) {
        if (sc == GUI_SC_ESCAPE && gui->dialog != GUI_DLG_WELCOME) {
            if (gui->dialog == GUI_DLG_UNSAVED) {
                gui->pending = GUI_PENDING_NONE;
            }
            gui_close_dialog(gui);
        }
        return;
    }

    /* Open Menu Shortcuts */
    if (gui->menu_open >= 0 && sc == GUI_SC_ESCAPE) {
        gui->menu_open = -1;
        return;
    }
    if (sc != GUI_SC_ALT) {
        gui->menu_open = -1;
    }

    /* Main Window Shortcuts */
    switch (sc) {
    case GUI_SC_NEW:
        gui_action_new(gui);
        break;
    case GUI_SC_OPEN:
        gui_action_open(gui);
        break;
    case GUI_SC_SAVE:
        gui_action_save(gui);
        break;
    case GUI_SC_SAVE_AS:
        gui_action_save_as(gui);
        break;
    case GUI_SC_START:
        gui_query_start(gui);
        break;
    case GUI_SC_HELP:
        gui_open_dialog(gui, GUI_DLG_HELP);
        break;
    case GUI_SC_UNDO:
        gui_action_undo(gui);
        break;
    case GUI_SC_REDO:
        gui_action_redo(gui);
        break;
    case GUI_SC_ALT:
        alt_shortcut(gui, gui->alt_key);
        break;
    case GUI_SC_ESCAPE:
        gui->text_focus = GUI_FOCUS_NONE;
        gui_textsel_reset(&gui->result_sel);
        gui_textsel_reset(&gui->tree_sel);
        break;
    default:
        break;
    }
}
