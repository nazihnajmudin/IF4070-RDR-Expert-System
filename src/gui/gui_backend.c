/**
 * @file gui_backend.c
 * @brief Backend GLFW dan OpenGL 1.x untuk Nuklear.
 *
 * Event input dari callback GLFW dimasukkan ke antrean lalu diteruskan ke
 * Nuklear secara bertahap. Setiap frame paling banyak memproses satu event
 * tombol mouse dan tidak memproses tekan dan lepas tombol keyboard yang sama
 * sekaligus, sehingga klik dan ketikan cepat tidak hilang walaupun aplikasi
 * hanya menggambar ulang saat ada event.
 */

#include "gui.h"

#include <stdlib.h>
#include <string.h>

/* Data font yang di-embed saat build (tools/embed.c) */
extern const unsigned char font_google_sans[];
extern const unsigned long font_google_sans_size;
extern const unsigned char font_google_sans_bold[];
extern const unsigned long font_google_sans_bold_size;
extern const unsigned char font_tree[];
extern const unsigned long font_tree_size;

/* Rentang glyph: Latin, tanda baca umum, panah, operator matematika, box drawing, bentuk, dingbat */
static const nk_rune FONT_RANGES[] = {
    0x0020, 0x00FF, 0x0100, 0x017F, 0x2000, 0x206F, 0x2190, 0x21FF,
    0x2200, 0x22FF, 0x2500, 0x257F, 0x25A0, 0x25FF, 0x2700, 0x27BF, 0
};

/**
 * @brief Format vertex untuk konversi perintah gambar Nuklear.
 */
typedef struct {
    float position[2];
    float uv[2];
    nk_byte col[4];
} GuiVertex;

/** Pemetaan tombol GLFW ke tombol Nuklear saat ditekan, dipakai lagi saat dilepas. */
static int key_map_on_press[GLFW_KEY_LAST + 1];

/* ===================================================================== */
/* Event Queue                                                           */
/* ===================================================================== */

void gui_backend_push_event(Gui *gui, int type, int code, int down, double x, double y)
{
    int next = (gui->ev_tail + 1) % GUI_EVENT_QUEUE;
    GuiEvent *e;

    if (next == gui->ev_head) {
        return;  /* antrean penuh, event dibuang */
    }
    e = &gui->events[gui->ev_tail];
    e->type = type;
    e->code = code;
    e->down = down;
    e->x = x;
    e->y = y;
    gui->ev_tail = next;
}

static Gui *gui_of(GLFWwindow *w)
{
    return (Gui *)glfwGetWindowUserPointer(w);
}

/* ===================================================================== */
/* GLFW Callbacks                                                        */
/* ===================================================================== */

static void on_char(GLFWwindow *w, unsigned int codepoint)
{
    Gui *gui = gui_of(w);

    if (gui->alt_down || gui->ctrl_down) {
        return;  /* kombinasi Alt dan Ctrl bukan input teks */
    }
    gui_backend_push_event(gui, GUI_EV_CHAR, (int)codepoint, 1, 0, 0);
}

static void on_scroll(GLFWwindow *w, double xoff, double yoff)
{
    gui_backend_push_event(gui_of(w), GUI_EV_SCROLL, 0, 0, xoff, yoff);
}

static void on_mouse(GLFWwindow *w, int button, int action, int mods)
{
    Gui *gui = gui_of(w);
    double x;
    double y;
    int nk_button;

    (void)mods;
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        nk_button = NK_BUTTON_LEFT;
    } else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
        nk_button = NK_BUTTON_RIGHT;
    } else if (button == GLFW_MOUSE_BUTTON_MIDDLE) {
        nk_button = NK_BUTTON_MIDDLE;
    } else {
        return;
    }
    glfwGetCursorPos(w, &x, &y);
    gui_backend_push_event(gui, GUI_EV_MOUSE, nk_button, action == GLFW_PRESS, x, y);
}

static void on_close(GLFWwindow *w)
{
    Gui *gui = gui_of(w);

    glfwSetWindowShouldClose(w, GLFW_FALSE);
    gui_action_exit(gui);
}

/* Memetakan tombol GLFW ke shortcut aplikasi. */
static GuiShortcut shortcut_of(Gui *gui, int key)
{
    if (gui->alt_down && key >= GLFW_KEY_A && key <= GLFW_KEY_Z) {
        gui->alt_key = 'a' + (key - GLFW_KEY_A);
        return GUI_SC_ALT;
    }
    if (gui->ctrl_down && !gui->edit_active) {
        if (key == GLFW_KEY_Z) {
            return gui->shift_down ? GUI_SC_REDO : GUI_SC_UNDO;
        }
        if (key == GLFW_KEY_Y) {
            return GUI_SC_REDO;
        }
    }
    if (gui->ctrl_down) {
        switch (key) {
        case GLFW_KEY_N:
            return GUI_SC_NEW;
        case GLFW_KEY_O:
            return GUI_SC_OPEN;
        case GLFW_KEY_S:
            return gui->shift_down ? GUI_SC_SAVE_AS : GUI_SC_SAVE;
        default:
            break;
        }
    }
    switch (key) {
    case GLFW_KEY_F5:
        return GUI_SC_START;
    case GLFW_KEY_F1:
        return GUI_SC_HELP;
    case GLFW_KEY_ESCAPE:
        return GUI_SC_ESCAPE;
    default:
        return GUI_SC_NONE;
    }
}

/* Memetakan tombol GLFW ke tombol Nuklear, -1 jika tidak dipakai. */
static int nk_key_of(Gui *gui, int key)
{
    if (gui->ctrl_down) {
        switch (key) {
        case GLFW_KEY_C:
            return NK_KEY_COPY;
        case GLFW_KEY_V:
            return NK_KEY_PASTE;
        case GLFW_KEY_X:
            return NK_KEY_CUT;
        case GLFW_KEY_Z:
            return gui->shift_down ? NK_KEY_TEXT_REDO : NK_KEY_TEXT_UNDO;
        case GLFW_KEY_Y:
            return NK_KEY_TEXT_REDO;
        case GLFW_KEY_A:
            return NK_KEY_TEXT_SELECT_ALL;
        case GLFW_KEY_LEFT:
            return NK_KEY_TEXT_WORD_LEFT;
        case GLFW_KEY_RIGHT:
            return NK_KEY_TEXT_WORD_RIGHT;
        case GLFW_KEY_HOME:
            return NK_KEY_TEXT_START;
        case GLFW_KEY_END:
            return NK_KEY_TEXT_END;
        default:
            break;
        }
    }
    switch (key) {
    case GLFW_KEY_DELETE:
        return NK_KEY_DEL;
    case GLFW_KEY_ENTER:
    case GLFW_KEY_KP_ENTER:
        return NK_KEY_ENTER;
    case GLFW_KEY_TAB:
        return NK_KEY_TAB;
    case GLFW_KEY_BACKSPACE:
        return NK_KEY_BACKSPACE;
    case GLFW_KEY_UP:
        return NK_KEY_UP;
    case GLFW_KEY_DOWN:
        return NK_KEY_DOWN;
    case GLFW_KEY_LEFT:
        return NK_KEY_LEFT;
    case GLFW_KEY_RIGHT:
        return NK_KEY_RIGHT;
    case GLFW_KEY_HOME:
        return NK_KEY_TEXT_LINE_START;
    case GLFW_KEY_END:
        return NK_KEY_TEXT_LINE_END;
    case GLFW_KEY_PAGE_UP:
        return NK_KEY_SCROLL_UP;
    case GLFW_KEY_PAGE_DOWN:
        return NK_KEY_SCROLL_DOWN;
    default:
        return -1;
    }
}

static void on_key(GLFWwindow *w, int key, int scancode, int action, int mods)
{
    Gui *gui = gui_of(w);
    int down = action != GLFW_RELEASE;
    int mapped;

    (void)scancode;
    (void)mods;
    if (key < 0 || key > GLFW_KEY_LAST) {
        return;
    }

    /* Modifier Tracking */
    if (key == GLFW_KEY_LEFT_CONTROL || key == GLFW_KEY_RIGHT_CONTROL) {
        gui->ctrl_down = down;
        gui_backend_push_event(gui, GUI_EV_KEY, NK_KEY_CTRL, down, 0, 0);
        return;
    }
    if (key == GLFW_KEY_LEFT_SHIFT || key == GLFW_KEY_RIGHT_SHIFT) {
        gui->shift_down = down;
        gui_backend_push_event(gui, GUI_EV_KEY, NK_KEY_SHIFT, down, 0, 0);
        return;
    }
    if (key == GLFW_KEY_LEFT_ALT || key == GLFW_KEY_RIGHT_ALT) {
        gui->alt_down = down;
        gui->idle_frames = 0;  /* gambar ulang garis mnemonic */
        return;
    }

    /* Application Shortcuts */
    if (action == GLFW_PRESS) {
        GuiShortcut sc = shortcut_of(gui, key);
        if (sc != GUI_SC_NONE) {
            gui->shortcut = sc;
            key_map_on_press[key] = -1;
            return;
        }
    }

    /* Editing Keys */
    if (action == GLFW_RELEASE) {
        mapped = key_map_on_press[key];
        key_map_on_press[key] = -1;
    } else {
        mapped = nk_key_of(gui, key);
        key_map_on_press[key] = mapped;
    }
    if (mapped >= 0) {
        gui_backend_push_event(gui, GUI_EV_KEY, mapped, down, 0, 0);
    }
}

/* ===================================================================== */
/* Clipboard                                                             */
/* ===================================================================== */

static void clipboard_paste(nk_handle user, struct nk_text_edit *edit)
{
    Gui *gui = (Gui *)user.ptr;
    const char *text = glfwGetClipboardString(gui->window);

    if (text != NULL) {
        nk_textedit_paste(edit, text, nk_strlen(text));
    }
}

static void clipboard_copy(nk_handle user, const char *text, int len)
{
    Gui *gui = (Gui *)user.ptr;
    char *copy;

    if (len <= 0) {
        return;
    }
    copy = str_ndup(text, (size_t)len);
    glfwSetClipboardString(gui->window, copy);
    free(copy);
}

/* ===================================================================== */
/* Lifecycle                                                             */
/* ===================================================================== */

int gui_backend_init(Gui *gui, int width, int height)
{
    int i;

    for (i = 0; i <= GLFW_KEY_LAST; i++) {
        key_map_on_press[i] = -1;
    }
    if (!glfwInit()) {
        return 0;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 1);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    gui->window = glfwCreateWindow(width, height, APP_NAME, NULL, NULL);
    if (gui->window == NULL) {
        glfwTerminate();
        return 0;
    }
    glfwSetWindowUserPointer(gui->window, gui);
    glfwMakeContextCurrent(gui->window);
    glfwSwapInterval(1);
    glfwSetWindowSizeLimits(gui->window, 760, 480, GLFW_DONT_CARE, GLFW_DONT_CARE);

    /* Callbacks */
    glfwSetCharCallback(gui->window, on_char);
    glfwSetKeyCallback(gui->window, on_key);
    glfwSetMouseButtonCallback(gui->window, on_mouse);
    glfwSetScrollCallback(gui->window, on_scroll);
    glfwSetWindowCloseCallback(gui->window, on_close);
    gui->cursor_resize = glfwCreateStandardCursor(GLFW_HRESIZE_CURSOR);

    /* Nuklear Context */
    nk_init_default(&gui->ctx, NULL);
    nk_buffer_init_default(&gui->commands);
    gui->ctx.clip.copy = clipboard_copy;
    gui->ctx.clip.paste = clipboard_paste;
    gui->ctx.clip.userdata = nk_handle_ptr(gui);
    gui->font_size = 0;
    gui->theme = -1;
    return 1;
}

void gui_backend_shutdown(Gui *gui)
{
    if (gui->font_texture != 0) {
        GLuint tex = (GLuint)gui->font_texture;
        glDeleteTextures(1, &tex);
        nk_font_atlas_clear(&gui->atlas);
    }
    nk_buffer_free(&gui->commands);
    nk_free(&gui->ctx);
    if (gui->cursor_resize != NULL) {
        glfwDestroyCursor(gui->cursor_resize);
    }
    if (gui->window != NULL) {
        glfwDestroyWindow(gui->window);
    }
    glfwTerminate();
}

/* ===================================================================== */
/* Fonts                                                                 */
/* ===================================================================== */

void gui_backend_update_fonts(Gui *gui, int size)
{
    struct nk_font_config cfg;
    struct nk_font_config cfg_large;
    const void *image;
    int w;
    int h;
    GLuint tex;

    if (gui->font_size == size) {
        return;
    }

    /* Previous Atlas Cleanup */
    if (gui->font_texture != 0) {
        tex = (GLuint)gui->font_texture;
        glDeleteTextures(1, &tex);
        nk_font_atlas_clear(&gui->atlas);
    }

    /* Atlas Baking */
    nk_font_atlas_init_default(&gui->atlas);
    nk_font_atlas_begin(&gui->atlas);
    cfg = nk_font_config((float)size);
    cfg.range = FONT_RANGES;
    cfg.oversample_h = 2;
    cfg.oversample_v = 1;
    cfg_large = cfg;
    cfg_large.size = (float)size * 1.35f;
    gui->font = nk_font_atlas_add_from_memory(&gui->atlas, (void *)font_google_sans, font_google_sans_size,
                                              (float)size, &cfg);
    gui->font_bold = nk_font_atlas_add_from_memory(&gui->atlas, (void *)font_google_sans_bold,
                                                   font_google_sans_bold_size, (float)size, &cfg);
    gui->font_large = nk_font_atlas_add_from_memory(&gui->atlas, (void *)font_google_sans_bold,
                                                    font_google_sans_bold_size, cfg_large.size, &cfg_large);
    gui->font_tree = nk_font_atlas_add_from_memory(&gui->atlas, (void *)font_tree, font_tree_size,
                                                   (float)size, &cfg);
    image = nk_font_atlas_bake(&gui->atlas, &w, &h, NK_FONT_ATLAS_RGBA32);

    /* Texture Upload */
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);
    gui->font_texture = tex;
    nk_font_atlas_end(&gui->atlas, nk_handle_id((int)tex), &gui->tex_null);
    nk_style_set_font(&gui->ctx, &gui->font->handle);

    gui->font_size = size;
    gui->row = (float)size + 12.0f;
}

/* ===================================================================== */
/* Input                                                                 */
/* ===================================================================== */

int gui_backend_input(Gui *gui, int wait)
{
    struct nk_context *ctx = &gui->ctx;
    char pressed[NK_KEY_MAX];
    int mouse_done = 0;
    int chars = 0;
    int had_input;

    /* Event Collection */
    if (wait) {
        glfwWaitEventsTimeout(0.5);
    } else {
        glfwPollEvents();
    }
    if (gui->alt_down && glfwGetKey(gui->window, GLFW_KEY_LEFT_ALT) != GLFW_PRESS &&
        glfwGetKey(gui->window, GLFW_KEY_RIGHT_ALT) != GLFW_PRESS && gui->testhook == NULL) {
        gui->alt_down = 0;  /* Alt dilepas saat window tidak fokus */
    }
    glfwGetWindowSize(gui->window, &gui->width, &gui->height);
    glfwGetFramebufferSize(gui->window, &gui->fb_width, &gui->fb_height);
    had_input = gui->ev_head != gui->ev_tail || gui->shortcut != GUI_SC_NONE;

    /* Event Delivery */
    gui->edit_active = gui->edit_active_now;
    gui->edit_active_now = 0;
    nk_input_begin(ctx);
    if (gui->testhook == NULL) {
        glfwGetCursorPos(gui->window, &gui->mouse_x, &gui->mouse_y);
        nk_input_motion(ctx, (int)gui->mouse_x, (int)gui->mouse_y);
    }
    memset(pressed, 0, sizeof(pressed));
    while (gui->ev_head != gui->ev_tail) {
        GuiEvent *e = &gui->events[gui->ev_head];
        if (e->type == GUI_EV_MOUSE) {
            if (mouse_done) {
                break;  /* satu transisi tombol mouse per frame */
            }
            nk_input_motion(ctx, (int)e->x, (int)e->y);
            nk_input_button(ctx, (enum nk_buttons)e->code, (int)e->x, (int)e->y, e->down);
            gui->mouse_x = e->x;
            gui->mouse_y = e->y;
            mouse_done = 1;
        } else if (e->type == GUI_EV_KEY) {
            if (!e->down && pressed[e->code]) {
                break;  /* lepas tombol diproses pada frame berikutnya */
            }
            nk_input_key(ctx, (enum nk_keys)e->code, e->down);
            if (e->down) {
                pressed[e->code] = 1;
            }
        } else if (e->type == GUI_EV_CHAR) {
            if (chars >= NK_INPUT_MAX - 1) {
                break;
            }
            nk_input_unicode(ctx, (nk_rune)e->code);
            chars++;
        } else if (e->type == GUI_EV_SCROLL) {
            nk_input_scroll(ctx, nk_vec2((float)e->x, (float)e->y));
        }
        gui->ev_head = (gui->ev_head + 1) % GUI_EVENT_QUEUE;
    }
    if (mouse_done == 0 && gui->testhook != NULL) {
        nk_input_motion(ctx, (int)gui->mouse_x, (int)gui->mouse_y);
    }
    nk_input_end(ctx);
    return had_input;
}

/* ===================================================================== */
/* Rendering                                                             */
/* ===================================================================== */

void gui_backend_render(Gui *gui)
{
    static const struct nk_draw_vertex_layout_element layout[] = {
        {NK_VERTEX_POSITION, NK_FORMAT_FLOAT, NK_OFFSETOF(GuiVertex, position)},
        {NK_VERTEX_TEXCOORD, NK_FORMAT_FLOAT, NK_OFFSETOF(GuiVertex, uv)},
        {NK_VERTEX_COLOR, NK_FORMAT_R8G8B8A8, NK_OFFSETOF(GuiVertex, col)},
        {NK_VERTEX_LAYOUT_END}
    };
    struct nk_convert_config config;
    struct nk_buffer vbuf;
    struct nk_buffer ebuf;
    const struct nk_draw_command *cmd;
    const nk_draw_index *offset;
    const void *vertices;
    float sx = gui->width > 0 ? (float)gui->fb_width / (float)gui->width : 1.0f;
    float sy = gui->height > 0 ? (float)gui->fb_height / (float)gui->height : 1.0f;
    struct nk_color bg = gui->ctx.style.window.fixed_background.data.color;

    /* GL State Setup */
    glViewport(0, 0, gui->fb_width, gui->fb_height);
    glClearColor(bg.r / 255.0f, bg.g / 255.0f, bg.b / 255.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glPushAttrib(GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT | GL_TRANSFORM_BIT);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_SCISSOR_TEST);
    glEnable(GL_BLEND);
    glEnable(GL_TEXTURE_2D);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0.0, gui->width, gui->height, 0.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);

    /* Command Conversion */
    memset(&config, 0, sizeof(config));
    config.vertex_layout = layout;
    config.vertex_size = sizeof(GuiVertex);
    config.vertex_alignment = NK_ALIGNOF(GuiVertex);
    config.tex_null = gui->tex_null;
    config.circle_segment_count = 22;
    config.curve_segment_count = 22;
    config.arc_segment_count = 22;
    config.global_alpha = 1.0f;
    config.shape_AA = NK_ANTI_ALIASING_ON;
    config.line_AA = NK_ANTI_ALIASING_ON;
    nk_buffer_init_default(&vbuf);
    nk_buffer_init_default(&ebuf);
    nk_convert(&gui->ctx, &gui->commands, &vbuf, &ebuf, &config);

    /* Draw Calls */
    vertices = nk_buffer_memory_const(&vbuf);
    glVertexPointer(2, GL_FLOAT, sizeof(GuiVertex), (const nk_byte *)vertices + NK_OFFSETOF(GuiVertex, position));
    glTexCoordPointer(2, GL_FLOAT, sizeof(GuiVertex), (const nk_byte *)vertices + NK_OFFSETOF(GuiVertex, uv));
    glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(GuiVertex), (const nk_byte *)vertices + NK_OFFSETOF(GuiVertex, col));
    offset = (const nk_draw_index *)nk_buffer_memory_const(&ebuf);
    nk_draw_foreach(cmd, &gui->ctx, &gui->commands) {
        if (cmd->elem_count == 0) {
            continue;
        }
        glBindTexture(GL_TEXTURE_2D, (GLuint)cmd->texture.id);
        glScissor((GLint)(cmd->clip_rect.x * sx),
                  (GLint)((gui->height - (cmd->clip_rect.y + cmd->clip_rect.h)) * sy),
                  (GLint)(cmd->clip_rect.w * sx), (GLint)(cmd->clip_rect.h * sy));
        glDrawElements(GL_TRIANGLES, (GLsizei)cmd->elem_count, GL_UNSIGNED_INT, offset);
        offset += cmd->elem_count;
    }
    nk_clear(&gui->ctx);
    nk_buffer_clear(&gui->commands);
    nk_buffer_free(&vbuf);
    nk_buffer_free(&ebuf);

    /* GL State Restore */
    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisableClientState(GL_COLOR_ARRAY);
    glBindTexture(GL_TEXTURE_2D, 0);
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glPopAttrib();
}

void gui_backend_present(Gui *gui)
{
    glfwSwapBuffers(gui->window);
}
