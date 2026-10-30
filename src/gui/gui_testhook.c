/**
 * @file gui_testhook.c
 * @brief Skrip aksi dan screenshot untuk verifikasi visual GUI.
 *
 * Hanya aktif pada build dengan RDR_TESTHOOK=1 (make TESTHOOK=1) dan saat
 * program dijalankan dengan opsi --test-script. Setiap baris skrip adalah
 * satu perintah:
 *
 *   wait <frames>               menunggu sejumlah frame
 *   size <w> <h>                mengubah ukuran window
 *   click <x> <y>               klik kiri pada koordinat window
 *   dclick <x> <y>              double click kiri
 *   press <x> <y>               menekan tombol kiri (untuk drag)
 *   release <x> <y>             melepas tombol kiri
 *   hold-alt <on|off>           menahan atau melepas Alt
 *   alt <huruf>                 menekan Alt dengan huruf
 *   clipboard                   mencetak isi clipboard ke stdout
 *   expect <hal> <nilai>        memeriksa state aplikasi (lihat run_expect)
 *   move <x> <y>                memindahkan kursor
 *   type <text>                 mengetik teks
 *   key <name>                  enter, tab, backspace, delete, up, down, left, right, home, end,
 *                               selectall, copy
 *   shortcut <name>             new, open, save, saveas, start, help, escape
 *   action <name> [arg]         aksi langsung (lihat run_action)
 *   shot <name>                 menyimpan screenshot <shot_dir>/<name>.png
 *   quit                        keluar
 */

#include "gui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* ===================================================================== */
/* PNG Encoder                                                           */
/* ===================================================================== */

static unsigned long crc_table[256];
static int crc_ready = 0;

static unsigned long crc_update(unsigned long crc, const unsigned char *buf, size_t len)
{
    size_t i;

    if (!crc_ready) {
        unsigned long c;
        int n;
        int k;
        for (n = 0; n < 256; n++) {
            c = (unsigned long)n;
            for (k = 0; k < 8; k++) {
                c = (c & 1) ? 0xEDB88320UL ^ (c >> 1) : c >> 1;
            }
            crc_table[n] = c;
        }
        crc_ready = 1;
    }
    for (i = 0; i < len; i++) {
        crc = crc_table[(crc ^ buf[i]) & 0xFF] ^ (crc >> 8);
    }
    return crc;
}

static void put_u32(unsigned char *p, unsigned long v)
{
    p[0] = (unsigned char)(v >> 24);
    p[1] = (unsigned char)(v >> 16);
    p[2] = (unsigned char)(v >> 8);
    p[3] = (unsigned char)v;
}

static void write_chunk(FILE *f, const char *type, const unsigned char *data, size_t len)
{
    unsigned char head[8];
    unsigned char tail[4];
    unsigned long crc;

    put_u32(head, (unsigned long)len);
    memcpy(head + 4, type, 4);
    fwrite(head, 1, 8, f);
    if (len > 0) {
        fwrite(data, 1, len, f);
    }
    crc = crc_update(0xFFFFFFFFUL, (const unsigned char *)type, 4);
    crc = crc_update(crc, data, len) ^ 0xFFFFFFFFUL;
    put_u32(tail, crc);
    fwrite(tail, 1, 4, f);
}

/* Menulis gambar RGB sebagai PNG dengan blok deflate tanpa kompresi. */
static int png_write_rgb(const char *path, const unsigned char *rgb, int w, int h)
{
    size_t row = (size_t)w * 3 + 1;
    size_t raw_len = row * (size_t)h;
    size_t blocks = (raw_len + 65534) / 65535;
    size_t z_len = 2 + raw_len + blocks * 5 + 4;
    unsigned char *z = xmalloc(z_len);
    unsigned char *raw = xmalloc(raw_len);
    unsigned char ihdr[13];
    unsigned long a = 1;
    unsigned long b = 0;
    size_t i;
    size_t pos = 0;
    FILE *f;
    int y;

    /* Scanline Assembly */
    for (y = 0; y < h; y++) {
        raw[row * (size_t)y] = 0;
        memcpy(raw + row * (size_t)y + 1, rgb + (size_t)w * 3 * (size_t)y, (size_t)w * 3);
    }

    /* Zlib Stream */
    z[pos++] = 0x78;
    z[pos++] = 0x01;
    for (i = 0; i < raw_len; i += 65535) {
        size_t n = raw_len - i < 65535 ? raw_len - i : 65535;
        z[pos++] = (unsigned char)(i + n == raw_len ? 1 : 0);
        z[pos++] = (unsigned char)(n & 0xFF);
        z[pos++] = (unsigned char)(n >> 8);
        z[pos++] = (unsigned char)(~n & 0xFF);
        z[pos++] = (unsigned char)((~n >> 8) & 0xFF);
        memcpy(z + pos, raw + i, n);
        pos += n;
    }
    for (i = 0; i < raw_len; i++) {
        a = (a + raw[i]) % 65521;
        b = (b + a) % 65521;
    }
    put_u32(z + pos, (b << 16) | a);
    pos += 4;

    /* File Output */
    f = platform_fopen(path, "wb");
    if (f == NULL) {
        free(z);
        free(raw);
        return 0;
    }
    fwrite("\x89PNG\r\n\x1a\n", 1, 8, f);
    put_u32(ihdr, (unsigned long)w);
    put_u32(ihdr + 4, (unsigned long)h);
    ihdr[8] = 8;
    ihdr[9] = 2;
    ihdr[10] = 0;
    ihdr[11] = 0;
    ihdr[12] = 0;
    write_chunk(f, "IHDR", ihdr, 13);
    write_chunk(f, "IDAT", z, pos);
    write_chunk(f, "IEND", NULL, 0);
    fclose(f);
    free(z);
    free(raw);
    return 1;
}

int gui_backend_screenshot(Gui *gui, const char *path)
{
    int w = gui->fb_width;
    int h = gui->fb_height;
    unsigned char *pixels = xmalloc((size_t)w * (size_t)h * 3);
    unsigned char *flipped = xmalloc((size_t)w * (size_t)h * 3);
    int y;
    int ok;

    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadBuffer(GL_BACK);
    glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, pixels);
    for (y = 0; y < h; y++) {
        memcpy(flipped + (size_t)y * (size_t)w * 3, pixels + (size_t)(h - 1 - y) * (size_t)w * 3, (size_t)w * 3);
    }
    ok = png_write_rgb(path, flipped, w, h);
    free(pixels);
    free(flipped);
    return ok;
}

#if defined(RDR_TESTHOOK)

/* ===================================================================== */
/* Script State                                                          */
/* ===================================================================== */

/**
 * @brief State eksekusi skrip.
 */
typedef struct {
    StrList lines;            /**< Baris skrip. */
    size_t pc;                /**< Baris berikutnya. */
    int wait;                 /**< Sisa frame tunggu. */
    char shot_dir[GUI_PATH_MAX]; /**< Folder screenshot. */
    char shot[GUI_PATH_MAX];  /**< Screenshot yang diminta, kosong jika tidak ada. */
    int checks;               /**< Jumlah perintah expect yang dijalankan. */
    int failures;             /**< Jumlah perintah expect yang gagal. */
} TestHook;

int gui_testhook_init(Gui *gui, const char *script_path, const char *shot_dir)
{
    TestHook *t;
    char *text = NULL;
    char err[256];

    if (script_path == NULL) {
        return 1;
    }
    if (!platform_read_file(script_path, &text, NULL, err, sizeof(err))) {
        fprintf(stderr, "testhook: %s\n", err);
        return 0;
    }
    t = xcalloc(1, sizeof(TestHook));
    strlist_init(&t->lines);
    {
        const char *p = text;
        while (*p != '\0') {
            const char *end = strchr(p, '\n');
            char *line = end ? str_trim_range(p, end) : str_trim_dup(p);
            if (line[0] != '\0' && line[0] != '#') {
                strlist_push_owned(&t->lines, line);
            } else {
                free(line);
            }
            if (end == NULL) {
                break;
            }
            p = end + 1;
        }
    }
    free(text);
    str_copy(t->shot_dir, sizeof(t->shot_dir), shot_dir ? shot_dir : ".");
    gui->testhook = t;
    return 1;
}

int gui_testhook_shutdown(Gui *gui)
{
    TestHook *t = gui->testhook;
    int failures;

    if (t == NULL) {
        return 0;
    }
    failures = t->failures;
    printf("testhook: %d expectations, %d failed\n", t->checks, t->failures);
    strlist_free(&t->lines);
    free(t);
    gui->testhook = NULL;
    return failures;
}

/* ===================================================================== */
/* Script Commands                                                       */
/* ===================================================================== */

static void push_click(Gui *gui, double x, double y)
{
    gui_backend_push_event(gui, GUI_EV_MOUSE, NK_BUTTON_LEFT, 1, x, y);
    gui_backend_push_event(gui, GUI_EV_MOUSE, NK_BUTTON_LEFT, 0, x, y);
}

static void push_text(Gui *gui, const char *s)
{
    const unsigned char *p = (const unsigned char *)s;

    while (*p != '\0') {
        unsigned long cp;
        int extra;
        if (*p < 0x80) {
            cp = *p;
            extra = 0;
        } else if ((*p & 0xE0) == 0xC0) {
            cp = *p & 0x1F;
            extra = 1;
        } else if ((*p & 0xF0) == 0xE0) {
            cp = *p & 0x0F;
            extra = 2;
        } else {
            cp = *p & 0x07;
            extra = 3;
        }
        p++;
        while (extra-- > 0 && *p != '\0') {
            cp = (cp << 6) | (*p++ & 0x3F);
        }
        gui_backend_push_event(gui, GUI_EV_CHAR, (int)cp, 1, 0, 0);
    }
}

static void push_key(Gui *gui, const char *name)
{
    static const struct {
        const char *name;
        int key;
    } KEYS[] = {
        {"enter", NK_KEY_ENTER}, {"tab", NK_KEY_TAB}, {"backspace", NK_KEY_BACKSPACE},
        {"delete", NK_KEY_DEL}, {"up", NK_KEY_UP}, {"down", NK_KEY_DOWN}, {"left", NK_KEY_LEFT},
        {"right", NK_KEY_RIGHT}, {"home", NK_KEY_TEXT_LINE_START}, {"end", NK_KEY_TEXT_LINE_END},
        {"selectall", NK_KEY_TEXT_SELECT_ALL}, {"copy", NK_KEY_COPY},
    };
    size_t i;

    for (i = 0; i < sizeof(KEYS) / sizeof(KEYS[0]); i++) {
        if (strcmp(name, KEYS[i].name) == 0) {
            gui_backend_push_event(gui, GUI_EV_KEY, KEYS[i].key, 1, 0, 0);
            gui_backend_push_event(gui, GUI_EV_KEY, KEYS[i].key, 0, 0, 0);
            return;
        }
    }
    fprintf(stderr, "testhook: unknown key '%s'\n", name);
}

static void run_shortcut(Gui *gui, const char *name)
{
    if (strcmp(name, "new") == 0) {
        gui->shortcut = GUI_SC_NEW;
    } else if (strcmp(name, "open") == 0) {
        gui->shortcut = GUI_SC_OPEN;
    } else if (strcmp(name, "save") == 0) {
        gui->shortcut = GUI_SC_SAVE;
    } else if (strcmp(name, "saveas") == 0) {
        gui->shortcut = GUI_SC_SAVE_AS;
    } else if (strcmp(name, "start") == 0) {
        gui->shortcut = GUI_SC_START;
    } else if (strcmp(name, "help") == 0) {
        gui->shortcut = GUI_SC_HELP;
    } else if (strcmp(name, "escape") == 0) {
        gui->shortcut = GUI_SC_ESCAPE;
    } else if (strcmp(name, "undo") == 0) {
        gui->shortcut = GUI_SC_UNDO;
    } else if (strcmp(name, "redo") == 0) {
        gui->shortcut = GUI_SC_REDO;
    }
}

/* Aksi langsung tanpa koordinat, agar skrip tetap stabil saat layout berubah. */
static void run_action(Gui *gui, const char *name, const char *arg)
{
    App *app = gui->app;

    if (strcmp(name, "new") == 0) {
        app_new(app, arg);
        gui_reset_view(gui);
        gui_close_dialog(gui);
    } else if (strcmp(name, "open") == 0) {
        gui->file.mode = GUI_FILE_OPEN_PROJECT;
        gui_file_chosen(gui, arg);
    } else if (strcmp(name, "save") == 0) {
        app_save_as(app, arg);
    } else if (strcmp(name, "add") == 0) {
        app_draft_add(app, arg);
    } else if (strcmp(name, "start") == 0) {
        gui_query_start(gui);
    } else if (strcmp(name, "clear") == 0) {
        app_clear(app);
    } else if (strcmp(name, "tab") == 0) {
        if (strcmp(arg, "clause") == 0 || strcmp(arg, "query") == 0) {
            app->query_tab = strcmp(arg, "query") == 0;
        } else {
            app->result_tab = strcmp(arg, "tree") == 0;
        }
    } else if (strcmp(name, "correct") == 0) {
        gui_action_correct(gui);
        if (arg[0] != '\0') {
            str_copy(gui->correct.conclusion, GUI_FIELD_MAX, arg);
        }
    } else if (strcmp(name, "manual") == 0) {
        gui_action_manual(gui, arg[0] ? atoi(arg) : -1);
    } else if (strcmp(name, "prefs") == 0) {
        gui_action_prefs(gui);
    } else if (strcmp(name, "help") == 0) {
        gui_open_dialog(gui, GUI_DLG_HELP);
    } else if (strcmp(name, "about") == 0) {
        gui_open_dialog(gui, GUI_DLG_ABOUT);
    } else if (strcmp(name, "details") == 0) {
        gui_open_dialog(gui, GUI_DLG_DETAILS);
    } else if (strcmp(name, "file") == 0) {
        gui_open_file_dialog(gui, strcmp(arg, "csv") == 0 ? GUI_FILE_IMPORT_CSV
                                  : strcmp(arg, "save") == 0 ? GUI_FILE_SAVE_PROJECT
                                  : strcmp(arg, "prefs") == 0 ? GUI_FILE_IMPORT_PREFS
                                                              : GUI_FILE_OPEN_PROJECT);
    } else if (strcmp(name, "csv") == 0) {
        gui->file.mode = GUI_FILE_IMPORT_CSV;
        gui_file_chosen(gui, arg);
    } else if (strcmp(name, "import-prefs") == 0) {
        gui->file.mode = GUI_FILE_IMPORT_PREFS;
        gui_file_chosen(gui, arg);
    } else if (strcmp(name, "pref") == 0) {
        char key[128];
        char value[128];
        PrefId id;
        Preferences p = app->prefs;
        if (sscanf(arg, "%127s %127s", key, value) == 2 && prefs_find(key, &id) &&
            prefs_set_from_text(&p, id, value, NULL, 0)) {
            app_set_prefs(app, &p);
            gui_sync_appearance(gui);
        }
    } else if (strcmp(name, "unsaved") == 0) {
        gui->pending = GUI_PENDING_NONE;
        gui_open_dialog(gui, GUI_DLG_UNSAVED);
    } else if (strcmp(name, "close") == 0) {
        gui_close_dialog(gui);
    } else if (strcmp(name, "welcome") == 0) {
        gui_open_dialog(gui, GUI_DLG_WELCOME);
    } else {
        fprintf(stderr, "testhook: unknown action '%s'\n", name);
    }
}

/* ===================================================================== */
/* Expectations                                                          */
/* ===================================================================== */

static const char *dialog_name(int dialog)
{
    static const char *const NAMES[] = {"none", "welcome", "new", "unsaved", "file", "correct", "manual",
                                        "csv", "prefs", "help", "about", "details"};
    return dialog >= 0 && dialog < (int)(sizeof(NAMES) / sizeof(NAMES[0])) ? NAMES[dialog] : "?";
}

/* Membandingkan state aplikasi dengan nilai yang diharapkan. */
static void run_expect(Gui *gui, TestHook *t, const char *what, const char *expected)
{
    App *app = gui->app;
    char actual[1024];
    int ok;

    /* Actual Value */
    if (strcmp(what, "conclusion") == 0) {
        str_copy(actual, sizeof(actual), app_has_result(app) ? app_conclusion(app) : "none");
    } else if (strcmp(what, "nodes") == 0) {
        sprintf(actual, "%lu", (unsigned long)app->kb->node_count);
    } else if (strcmp(what, "draft") == 0) {
        char *text = strlist_join(&app->draft, "; ");
        str_copy(actual, sizeof(actual), text);
        free(text);
    } else if (strcmp(what, "dialog") == 0) {
        str_copy(actual, sizeof(actual), dialog_name(gui->dialog));
    } else if (strcmp(what, "dirty") == 0) {
        sprintf(actual, "%d", app->dirty);
    } else if (strcmp(what, "undo") == 0) {
        sprintf(actual, "%d", app_can_undo(app));
    } else if (strcmp(what, "redo") == 0) {
        sprintf(actual, "%d", app_can_redo(app));
    } else if (strcmp(what, "query_tab") == 0) {
        str_copy(actual, sizeof(actual), app->query_tab == SESSION_QUERY_TAB_TEXT ? "query" : "clause");
    } else if (strcmp(what, "result_tab") == 0) {
        str_copy(actual, sizeof(actual), app->result_tab == SESSION_RESULT_TAB_TREE ? "tree" : "result");
    } else if (strcmp(what, "default") == 0) {
        str_copy(actual, sizeof(actual), app->kb->root->conclusion);
    } else if (strcmp(what, "menu") == 0) {
        sprintf(actual, "%d", gui->menu_open);
    } else if (strcmp(what, "theme") == 0) {
        str_copy(actual, sizeof(actual), gui->theme == PREF_THEME_DARK ? "dark" : "light");
    } else if (strcmp(what, "split") == 0) {
        sprintf(actual, "%.2f", app->split_ratio);
    } else if (strcmp(what, "font") == 0) {
        sprintf(actual, "%d", gui->font_size);
    } else if (strcmp(what, "status") == 0 || strcmp(what, "clipboard") == 0) {
        const char *src = what[0] == 's' ? app->status : glfwGetClipboardString(gui->window);
        str_copy(actual, sizeof(actual), src ? src : "");
        t->checks++;
        if (strstr(actual, expected) == NULL) {  /* status dan clipboard dicek sebagai substring */
            t->failures++;
            fprintf(stderr, "EXPECT FAILED (line %lu): %s contains '%s', actual '%s'\n",
                    (unsigned long)t->pc, what, expected, actual);
        }
        return;
    } else {
        fprintf(stderr, "testhook: unknown expectation '%s'\n", what);
        t->failures++;
        return;
    }

    /* Comparison */
    t->checks++;
    ok = strcmp(actual, expected) == 0;
    if (!ok) {
        t->failures++;
        fprintf(stderr, "EXPECT FAILED (line %lu): %s == '%s', actual '%s'\n",
                (unsigned long)t->pc, what, expected, actual);
    }
}

int gui_testhook_step(Gui *gui)
{
    TestHook *t = gui->testhook;

    if (t == NULL) {
        return 0;
    }
    if (t->wait > 0) {
        t->wait--;
        return 1;
    }
    while (t->pc < t->lines.count && t->wait == 0) {
        char *line = t->lines.items[t->pc++];
        char cmd[32] = "";
        const char *arg;
        double x = 0;
        double y = 0;
        sscanf(line, "%31s", cmd);
        arg = line + strlen(cmd);
        while (*arg == ' ') {
            arg++;
        }

        if (strcmp(cmd, "wait") == 0) {
            t->wait = atoi(arg);
        } else if (strcmp(cmd, "size") == 0) {
            int w = 0;
            int h = 0;
            sscanf(arg, "%d %d", &w, &h);
            glfwSetWindowSize(gui->window, w, h);
            t->wait = 4;
        } else if (strcmp(cmd, "click") == 0 || strcmp(cmd, "dclick") == 0) {
            sscanf(arg, "%lf %lf", &x, &y);
            gui->mouse_x = x;
            gui->mouse_y = y;
            push_click(gui, x, y);
            if (cmd[0] == 'd') {
                push_click(gui, x, y);
            }
            t->wait = 4;
        } else if (strcmp(cmd, "press") == 0 || strcmp(cmd, "release") == 0) {
            sscanf(arg, "%lf %lf", &x, &y);
            gui->mouse_x = x;
            gui->mouse_y = y;
            gui_backend_push_event(gui, GUI_EV_MOUSE, NK_BUTTON_LEFT, cmd[0] == 'p', x, y);
            t->wait = 3;
        } else if (strcmp(cmd, "hold-alt") == 0) {
            gui->alt_down = strcmp(arg, "on") == 0;
            t->wait = 2;
        } else if (strcmp(cmd, "alt") == 0) {
            gui->alt_key = arg[0];
            gui->shortcut = GUI_SC_ALT;
            t->wait = 3;
        } else if (strcmp(cmd, "clipboard") == 0) {
            const char *clip = glfwGetClipboardString(gui->window);
            printf("clipboard>>>\n%s\n<<<clipboard\n", clip ? clip : "");
        } else if (strcmp(cmd, "move") == 0) {
            sscanf(arg, "%lf %lf", &x, &y);
            gui->mouse_x = x;
            gui->mouse_y = y;
            t->wait = 2;
        } else if (strcmp(cmd, "type") == 0) {
            push_text(gui, arg);
            t->wait = 2 + (int)strlen(arg) / 8;
        } else if (strcmp(cmd, "key") == 0) {
            push_key(gui, arg);
            t->wait = 3;
        } else if (strcmp(cmd, "shortcut") == 0) {
            run_shortcut(gui, arg);
            t->wait = 3;
        } else if (strcmp(cmd, "action") == 0) {
            char name[64] = "";
            const char *rest;
            sscanf(arg, "%63s", name);
            rest = arg + strlen(name);
            while (*rest == ' ') {
                rest++;
            }
            run_action(gui, name, rest);
            t->wait = 3;
        } else if (strcmp(cmd, "shot") == 0) {
            str_set_error(t->shot, sizeof(t->shot), "%s/%s.png", t->shot_dir, arg);
            t->wait = 1;
        } else if (strcmp(cmd, "expect") == 0) {
            char what[64] = "";
            const char *value;
            sscanf(arg, "%63s", what);
            value = arg + strlen(what);
            while (*value == ' ') {
                value++;
            }
            run_expect(gui, t, what, value);
        } else if (strcmp(cmd, "quit") == 0) {
            gui->quit = 1;
            return 1;
        } else {
            fprintf(stderr, "testhook: unknown command '%s'\n", cmd);
        }
    }
    if (t->pc >= t->lines.count && t->wait == 0 && t->shot[0] == '\0') {
        gui->quit = 1;
    }
    return 1;
}

void gui_testhook_after_render(Gui *gui)
{
    TestHook *t = gui->testhook;

    if (t == NULL || t->shot[0] == '\0') {
        return;
    }
    if (!gui_backend_screenshot(gui, t->shot)) {
        fprintf(stderr, "testhook: cannot write '%s'\n", t->shot);
    } else {
        printf("screenshot: %s\n", t->shot);
    }
    t->shot[0] = '\0';
}

#else

/* ===================================================================== */
/* Release Build Stubs                                                   */
/* ===================================================================== */

int gui_testhook_init(Gui *gui, const char *script_path, const char *shot_dir)
{
    (void)gui;
    (void)shot_dir;
    if (script_path != NULL) {
        fprintf(stderr, "this build has no test hook support (rebuild with TESTHOOK=1)\n");
        return 0;
    }
    return 1;
}

int gui_testhook_step(Gui *gui)
{
    (void)gui;
    return 0;
}

void gui_testhook_after_render(Gui *gui)
{
    (void)gui;
}

int gui_testhook_shutdown(Gui *gui)
{
    (void)gui;
    return 0;
}

#endif
