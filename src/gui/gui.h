/**
 * @file gui.h
 * @brief State bersama dan antarmuka antar modul GUI.
 *
 * GUI dibangun dengan Nuklear (immediate mode) di atas GLFW dan OpenGL 1.x.
 * Seluruh state tampilan disimpan dalam satu struct Gui, sedangkan state
 * project disimpan oleh controller App. Setiap frame, modul-modul GUI
 * menggambar ulang tampilan dari kedua state tersebut dan meneruskan aksi
 * user ke fungsi app_*.
 *
 * Pembagian modul:
 *   gui_backend.c        window, input, font, dan render OpenGL
 *   gui_theme.c          tema warna terang dan gelap
 *   gui_widgets.c        widget bantu (tab, tombol, mnemonic, teks berwarna)
 *   gui_textview.c       tampilan teks read-only yang dapat diseleksi dan disalin
 *   gui_menu.c           menubar, popup menu, shortcut, dan navigasi keyboard
 *   gui_query_panel.c    panel kueri (tab Clause dan Query) serta bar Q
 *   gui_result_panel.c   panel hasil (tab Result dan Tree)
 *   gui_actions.c        aksi File (New, Open, Save) dan alur unsaved changes
 *   gui_dialogs.c        dialog sederhana (New, konfirmasi, Help, About, laporan)
 *   gui_file_dialog.c    file browser
 *   gui_correct_dialog.c dialog koreksi (Edit last query)
 *   gui_manual_dialog.c  dialog edit manual tree
 *   gui_prefs_dialog.c   dialog Edit Preference dari tabel deskriptor
 *   gui_testhook.c       skrip aksi dan screenshot (hanya build test)
 */

#ifndef RDR_GUI_H
#define RDR_GUI_H

#include "nk.h"

#include <GLFW/glfw3.h>

#include "rdr/app.h"
#include "rdr/kb_csv.h"
#include "rdr/platform.h"
#include "rdr/rdr_print.h"

/** Kapasitas field teks satu baris. */
#define GUI_FIELD_MAX 512
/** Kapasitas field teks panjang (rule atau kueri). */
#define GUI_LONG_FIELD_MAX 2048
/** Kapasitas kotak teks kueri bebas. */
#define GUI_TEXT_MAX 16384
/** Kapasitas path. */
#define GUI_PATH_MAX 1024
/** Kapasitas antrean event input. */
#define GUI_EVENT_QUEUE 512

/* ===================================================================== */
/* Shared Types                                                          */
/* ===================================================================== */

/**
 * @brief Warna semantik di luar tabel warna Nuklear.
 */
typedef struct {
    struct nk_color text;           /**< Warna teks utama. */
    struct nk_color muted;          /**< Teks sekunder. */
    struct nk_color accent;         /**< Warna aksen (judul, tab aktif). */
    struct nk_color primary;        /**< Latar tombol utama (Start). */
    struct nk_color fired;          /**< Node yang menyala. */
    struct nk_color failed;         /**< Node yang dievaluasi tetapi false. */
    struct nk_color conclusion;     /**< Teks kesimpulan akhir. */
    struct nk_color conclusion_bg;  /**< Latar baris kesimpulan akhir. */
    struct nk_color error;          /**< Pesan error dan klausa tidak valid. */
    struct nk_color warning;        /**< Pesan peringatan. */
    struct nk_color success;        /**< Pesan sukses. */
    struct nk_color overlay;        /**< Lapisan peredup saat dialog terbuka. */
    struct nk_color panel;          /**< Latar panel konten. */
    struct nk_color tab_active;     /**< Latar tab aktif. */
    struct nk_color tab_idle;       /**< Latar tab tidak aktif. */
    struct nk_color status_bg;      /**< Latar status bar. */
} GuiPalette;

/**
 * @brief Identitas dialog yang sedang terbuka.
 */
typedef enum {
    GUI_DLG_NONE,        /**< Tidak ada dialog. */
    GUI_DLG_WELCOME,     /**< Dialog awal: buat KB baru atau buka project. */
    GUI_DLG_NEW,         /**< Dialog New knowledge base. */
    GUI_DLG_UNSAVED,     /**< Konfirmasi perubahan belum disimpan. */
    GUI_DLG_FILE,        /**< File browser. */
    GUI_DLG_CORRECT,     /**< Koreksi kesimpulan (Edit last query). */
    GUI_DLG_MANUAL,      /**< Edit manual tree. */
    GUI_DLG_CSV_REPORT,  /**< Laporan impor CSV. */
    GUI_DLG_PREFS,       /**< Edit Preference. */
    GUI_DLG_HELP,        /**< Halaman bantuan. */
    GUI_DLG_ABOUT,       /**< Informasi aplikasi. */
    GUI_DLG_DETAILS      /**< Rincian pesan status. */
} GuiDialogId;

/**
 * @brief Kegunaan file browser.
 */
typedef enum {
    GUI_FILE_OPEN_PROJECT,  /**< Membuka project. */
    GUI_FILE_SAVE_PROJECT,  /**< Menyimpan project (Save As). */
    GUI_FILE_IMPORT_CSV,    /**< Memilih dataset CSV. */
    GUI_FILE_IMPORT_PREFS   /**< Memilih project sumber preferensi. */
} GuiFileMode;

/**
 * @brief Aksi yang tertunda sampai konfirmasi unsaved changes selesai.
 */
typedef enum {
    GUI_PENDING_NONE,  /**< Tidak ada aksi tertunda. */
    GUI_PENDING_NEW,   /**< Membuat KB baru. */
    GUI_PENDING_OPEN,  /**< Membuka project lain. */
    GUI_PENDING_EXIT   /**< Keluar dari aplikasi. */
} GuiPending;

/**
 * @brief Satu event input yang diantrekan dari callback GLFW.
 */
typedef struct {
    int type;     /**< Jenis event (GUI_EV_*). */
    int code;     /**< Kode tombol atau codepoint. */
    int down;     /**< 1 untuk tekan, 0 untuk lepas. */
    double x;     /**< Posisi x atau scroll x. */
    double y;     /**< Posisi y atau scroll y. */
} GuiEvent;

/** Jenis event input. */
enum {
    GUI_EV_CHAR,    /**< Karakter Unicode. */
    GUI_EV_KEY,     /**< Tombol keyboard Nuklear. */
    GUI_EV_MOUSE,   /**< Tombol mouse. */
    GUI_EV_SCROLL   /**< Scroll mouse. */
};

/** Shortcut aplikasi yang ditangkap dari keyboard. */
typedef enum {
    GUI_SC_NONE,     /**< Tidak ada. */
    GUI_SC_UNDO,     /**< Ctrl+Z saat tidak sedang mengedit teks. */
    GUI_SC_REDO,     /**< Ctrl+Y atau Ctrl+Shift+Z saat tidak sedang mengedit teks. */
    GUI_SC_ALT,      /**< Alt dengan huruf (lihat Gui.alt_key). */
    GUI_SC_NEW,      /**< Ctrl+N. */
    GUI_SC_OPEN,     /**< Ctrl+O. */
    GUI_SC_SAVE,     /**< Ctrl+S. */
    GUI_SC_SAVE_AS,  /**< Ctrl+Shift+S. */
    GUI_SC_START,    /**< F5. */
    GUI_SC_HELP,     /**< F1. */
    GUI_SC_ESCAPE    /**< Escape. */
} GuiShortcut;

/** Komponen yang menerima fokus keyboard. */
typedef enum {
    GUI_FOCUS_NONE,    /**< Tidak ada komponen teks yang fokus. */
    GUI_FOCUS_RESULT,  /**< Tampilan teks tab Result. */
    GUI_FOCUS_TREE     /**< Tampilan teks tab Tree. */
} GuiFocus;

/** Pilihan font untuk baris tampilan teks. */
enum {
    GUI_FONT_REGULAR,  /**< Font biasa. */
    GUI_FONT_BOLD,     /**< Font tebal. */
    GUI_FONT_LARGE,    /**< Font tebal besar. */
    GUI_FONT_TREE       /**< Font fallback untuk garis tree. */
};

/* ===================================================================== */
/* Text View Types                                                       */
/* ===================================================================== */

/**
 * @brief Satu baris tampilan teks dengan paling banyak dua segmen warna.
 *
 * Segmen kedua dimulai pada kolom split.
 */
typedef struct {
    char *text;              /**< Isi baris UTF-8 (milik baris). */
    int columns;             /**< Jumlah codepoint pada text. */
    int split;               /**< Kolom awal segmen kedua, -1 jika hanya satu segmen. */
    int font1;               /**< Font segmen pertama (GUI_FONT_*). */
    int font2;               /**< Font segmen kedua. */
    struct nk_color color1;  /**< Warna segmen pertama. */
    struct nk_color color2;  /**< Warna segmen kedua. */
    struct nk_color bg;      /**< Warna latar, alpha 0 berarti tanpa latar. */
    int bg_full;             /**< 1 untuk latar selebar baris, 0 untuk latar segmen kedua saja. */
    float height;            /**< Tinggi baris dalam piksel. */
    int node_id;             /**< Node terkait (tab Tree), -1 jika tidak ada. */
} GuiTextLine;

/**
 * @brief Dokumen tampilan teks.
 */
typedef struct {
    GuiTextLine *items;  /**< Array baris. */
    size_t count;        /**< Jumlah baris. */
    size_t cap;          /**< Kapasitas array. */
    float width;         /**< Lebar baris terpanjang dalam piksel. */
} GuiTextDoc;

/**
 * @brief Posisi dan state seleksi pada tampilan teks.
 */
typedef struct {
    int anchor_line;     /**< Baris titik awal seleksi. */
    int anchor_col;      /**< Kolom titik awal seleksi. */
    int cursor_line;     /**< Baris titik akhir seleksi. */
    int cursor_col;      /**< Kolom titik akhir seleksi. */
    int dragging;        /**< 1 selama tombol mouse ditahan untuk menyeleksi. */
    double last_click;   /**< Waktu klik terakhir. */
    int last_click_line; /**< Baris klik terakhir. */
} GuiTextSel;

/**
 * @brief Kejadian yang dilaporkan tampilan teks pada satu frame.
 */
typedef struct {
    int clicked_line;        /**< Baris yang diklik, -1 jika tidak ada. */
    int double_clicked_line; /**< Baris yang di-double click, -1 jika tidak ada. */
} GuiTextEvent;

/* ===================================================================== */
/* Dialog States                                                         */
/* ===================================================================== */

/**
 * @brief State file browser.
 */
typedef struct {
    GuiFileMode mode;           /**< Kegunaan dialog. */
    char dir[GUI_PATH_MAX];     /**< Folder yang sedang ditampilkan. */
    char dir_edit[GUI_PATH_MAX];/**< Isi field folder yang dapat diedit. */
    char name[GUI_PATH_MAX];    /**< Nama file terpilih. */
    PlatformDirList entries;    /**< Isi folder. */
    StrList roots;              /**< Root sistem file (drive pada Windows). */
    int show_all;               /**< 1 untuk menampilkan semua file. */
    int selected;               /**< Indeks entri terpilih, -1 jika tidak ada. */
    double last_click;          /**< Waktu klik terakhir untuk deteksi double click. */
    int last_click_index;       /**< Entri pada klik terakhir. */
    int confirm_overwrite;      /**< 1 jika menunggu konfirmasi menimpa file. */
    char error[512];            /**< Pesan error dialog. */
} GuiFileDialog;

/**
 * @brief State dialog koreksi.
 */
typedef struct {
    AppCorrectionPlan plan;              /**< Rencana koreksi dari App. */
    char conclusion[GUI_FIELD_MAX];      /**< Kesimpulan yang benar. */
    char (*clauses)[GUI_FIELD_MAX];      /**< Teks klausa exception yang dapat diedit. */
    int *enabled;                        /**< Status centang setiap klausa. */
    size_t count;                        /**< Jumlah klausa. */
    char cornerstone[GUI_LONG_FIELD_MAX];/**< Teks cornerstone pembanding. */
    char query[GUI_LONG_FIELD_MAX];      /**< Teks kueri yang dikoreksi. */
    char check[512];                     /**< Hasil validasi rule saat ini. */
    int valid;                           /**< 1 jika koreksi dapat diterapkan. */
} GuiCorrectDialog;

/**
 * @brief State dialog edit manual.
 */
typedef struct {
    int selected;                          /**< Id node terpilih. */
    int loaded;                            /**< Id node yang isinya sedang dimuat di form. */
    unsigned long loaded_revision;         /**< Revisi App saat form dimuat. */
    char rule[GUI_LONG_FIELD_MAX];         /**< Field rule. */
    char conclusion[GUI_FIELD_MAX];        /**< Field kesimpulan. */
    char cornerstone[GUI_LONG_FIELD_MAX];  /**< Field cornerstone. */
    int adding;                            /**< 0 tidak, 1 tambah true child, 2 tambah false child. */
    char new_rule[GUI_LONG_FIELD_MAX];     /**< Rule node baru. */
    char new_conclusion[GUI_FIELD_MAX];    /**< Kesimpulan node baru. */
    char new_case[GUI_LONG_FIELD_MAX];     /**< Cornerstone node baru. */
    int confirm_delete;                    /**< 1 jika menunggu konfirmasi hapus. */
    RdrValidation validation;              /**< Hasil validasi terakhir. */
    int has_validation;                    /**< 1 jika validation berisi data. */
    char message[512];                     /**< Pesan hasil aksi terakhir. */
    int message_error;                     /**< 1 jika pesan berupa error. */
} GuiManualDialog;

/* ===================================================================== */
/* GUI State                                                             */
/* ===================================================================== */

/**
 * @brief Seluruh state GUI.
 */
typedef struct Gui {
    /* Platform */
    GLFWwindow *window;                 /**< Window GLFW. */
    struct nk_context ctx;              /**< Konteks Nuklear. */
    struct nk_buffer commands;          /**< Buffer perintah gambar Nuklear. */
    App *app;                           /**< Controller project. */
    int width;                          /**< Lebar window (koordinat logis). */
    int height;                         /**< Tinggi window (koordinat logis). */
    int fb_width;                       /**< Lebar framebuffer (piksel). */
    int fb_height;                      /**< Tinggi framebuffer (piksel). */

    /* Fonts and Theme */
    struct nk_font_atlas atlas;         /**< Atlas font. */
    unsigned int font_texture;          /**< Texture OpenGL atlas font. */
    struct nk_draw_null_texture tex_null; /**< Texture kosong untuk bentuk polos. */
    struct nk_font *font;               /**< Font biasa. */
    struct nk_font *font_bold;          /**< Font tebal. */
    struct nk_font *font_large;         /**< Font tebal besar untuk kesimpulan. */
    struct nk_font *font_tree;          /**< Font fallback untuk karakter garis tree. */
    int font_size;                      /**< Ukuran font yang sedang terpasang. */
    int theme;                          /**< Tema yang sedang terpasang. */
    GuiPalette pal;                     /**< Warna semantik aktif. */
    float row;                          /**< Tinggi baris standar. */

    /* Input */
    GuiEvent events[GUI_EVENT_QUEUE];   /**< Antrean event. */
    int ev_head;                        /**< Indeks baca antrean. */
    int ev_tail;                        /**< Indeks tulis antrean. */
    GuiShortcut shortcut;               /**< Shortcut yang menunggu diproses. */
    int alt_key;                        /**< Huruf kecil yang ditekan bersama Alt. */
    int ctrl_down;                      /**< 1 jika Ctrl sedang ditekan. */
    int shift_down;                     /**< 1 jika Shift sedang ditekan. */
    int alt_down;                       /**< 1 jika Alt sedang ditekan. */
    int edit_active;                    /**< 1 jika ada field teks aktif pada frame sebelumnya. */
    int edit_active_now;                /**< Akumulasi field teks aktif pada frame berjalan. */
    GLFWcursor *cursor_resize;          /**< Kursor geser horizontal untuk splitter. */
    int cursor_is_resize;               /**< 1 jika kursor geser sedang terpasang. */
    double mouse_x;                     /**< Posisi mouse terakhir. */
    double mouse_y;                     /**< Posisi mouse terakhir. */
    int idle_frames;                    /**< Jumlah frame berturut-turut tanpa input. */

    /* Application Flow */
    int quit;                           /**< 1 jika aplikasi harus berhenti. */
    int menu_open;                      /**< Indeks menu yang terbuka, -1 jika tidak ada. */
    int menu_cursor;                    /**< Item menu yang disorot, -1 jika tidak ada. */
    struct nk_rect menu_headers[4];     /**< Posisi header menu pada frame terakhir. */
    GuiFocus text_focus;                /**< Tampilan teks yang fokus. */
    int focus_clause;                   /**< 1 untuk memfokuskan klausa pertama, 2 untuk menunda satu frame. */
    int focus_text;                     /**< 1 untuk memfokuskan kotak teks kueri, 2 untuk menunda satu frame. */
    int unfocus_edits;                  /**< 1 untuk menonaktifkan field teks yang aktif. */
    int split_drag;                     /**< 1 selama splitter digeser. */
    double split_x0;                    /**< Posisi mouse saat splitter mulai digeser. */
    float split_w0;                     /**< Lebar panel kiri saat splitter mulai digeser. */
    int needs_setup;                    /**< 1 sampai user membuat KB baru atau membuka project. */
    int dialog;                         /**< Dialog yang terbuka (GuiDialogId). */
    int dialog_opened;                  /**< 1 pada frame pertama dialog dibuka. */
    GuiPending pending;                 /**< Aksi tertunda setelah konfirmasi. */
    char title[512];                    /**< Judul window terakhir. */
    char dialog_error[256];             /**< Pesan error pada dialog sederhana. */

    /* Query Panel */
    char qbar[GUI_FIELD_MAX];           /**< Isi bar Q. */
    int qbar_focus;                     /**< 1 untuk memfokuskan bar Q pada frame berikutnya. */
    char (*rows)[GUI_FIELD_MAX];        /**< Buffer edit setiap klausa draft. */
    size_t rows_count;                  /**< Jumlah buffer yang terpakai. */
    size_t rows_cap;                    /**< Kapasitas buffer. */
    unsigned long rows_revision;        /**< Revisi App saat buffer disinkronkan. */
    int active_row;                     /**< Baris yang sedang diedit, -1 jika tidak ada. */
    char text[GUI_TEXT_MAX];            /**< Isi kotak teks kueri bebas. */
    unsigned long text_revision;        /**< Revisi App saat text dimuat dari draft. */
    int text_active;                    /**< 1 jika kotak teks sedang diedit. */

    /* Result Panel */
    GuiTextDoc result_doc;              /**< Dokumen tab Result. */
    GuiTextSel result_sel;              /**< Seleksi tab Result. */
    unsigned long result_revision;      /**< Revisi App saat dokumen Result dibuat. */
    float result_wrap;                  /**< Lebar bungkus saat dokumen Result dibuat. */
    Preferences result_prefs;           /**< Preferensi saat dokumen Result dibuat. */
    int result_font_size;               /**< Ukuran font saat dokumen Result dibuat. */
    int result_valid;                   /**< 1 jika dokumen Result sudah pernah dibuat. */
    GuiTextDoc tree_doc;                /**< Dokumen tab Tree. */
    GuiTextSel tree_sel;                /**< Seleksi tab Tree. */
    RdrLines tree;                      /**< Cache baris tree. */
    float tree_width;                   /**< Lebar baris tree terpanjang. */
    unsigned long tree_revision;        /**< Revisi App saat cache dibuat. */
    Preferences tree_prefs;             /**< Preferensi saat cache dibuat. */
    int tree_font_size;                 /**< Ukuran font saat cache dibuat. */
    int tree_valid;                     /**< 1 jika cache sudah pernah dibuat. */
    double tree_click_time;             /**< Waktu klik terakhir pada baris tree. */
    int tree_click_id;                  /**< Node pada klik terakhir. */
    size_t reasoning_step;              /**< Jumlah langkah reasoning yang sudah ditampilkan. */
    double reasoning_next_time;         /**< Waktu langkah reasoning berikutnya. */
    unsigned long reasoning_revision;   /**< Revisi App yang sedang divisualisasikan. */
    int reasoning_active;               /**< 1 selama replay reasoning berjalan. */

    /* Dialogs */
    char new_default[GUI_FIELD_MAX];    /**< Field kesimpulan default pada dialog New. */
    GuiFileDialog file;                 /**< State file browser. */
    GuiCorrectDialog correct;           /**< State dialog koreksi. */
    GuiManualDialog manual;             /**< State dialog edit manual. */
    Preferences prefs_edit;             /**< Salinan preferensi yang sedang diedit. */
    CsvImportReport csv_report;         /**< Laporan impor CSV terakhir. */
    char csv_path[GUI_PATH_MAX];        /**< File CSV terakhir. */

    /* Test Hook */
    void *testhook;                     /**< State test hook (NULL pada build biasa). */
} Gui;

/* ===================================================================== */
/* Backend (gui_backend.c)                                               */
/* ===================================================================== */

/** @brief Membuat window, konteks OpenGL, dan konteks Nuklear. */
int gui_backend_init(Gui *gui, int width, int height);

/** @brief Menutup window dan membebaskan sumber daya backend. */
void gui_backend_shutdown(Gui *gui);

/** @brief Membangun ulang atlas font jika ukuran font berubah. */
void gui_backend_update_fonts(Gui *gui, int size);

/**
 * @brief Menunggu atau mengambil event lalu meneruskannya ke Nuklear.
 * @param wait 1 untuk menunggu event (hemat CPU), 0 untuk langsung kembali.
 * @return 1 jika ada input yang diproses.
 */
int gui_backend_input(Gui *gui, int wait);

/** @brief Merender perintah gambar Nuklear ke window. */
void gui_backend_render(Gui *gui);

/** @brief Menampilkan hasil render ke layar. */
void gui_backend_present(Gui *gui);

/** @brief Menambahkan event ke antrean (dipakai callback dan test hook). */
void gui_backend_push_event(Gui *gui, int type, int code, int down, double x, double y);

/** @brief Menyimpan isi framebuffer sebagai PNG. */
int gui_backend_screenshot(Gui *gui, const char *path);

/* ===================================================================== */
/* Theme and Widgets (gui_theme.c, gui_widgets.c)                        */
/* ===================================================================== */

/** @brief Memasang tema warna (PREF_THEME_LIGHT atau PREF_THEME_DARK). */
void gui_theme_apply(Gui *gui, int theme);

/**
 * @brief Menggambar baris tab.
 * @return Indeks tab baru yang aktif.
 */
int gui_tabs(Gui *gui, const char *const *labels, int count, int active);

/**
 * @brief Menggambar satu tombol tab pada layout space di posisi tertentu.
 * @param mnemonic Indeks huruf mnemonic yang digaris bawahi saat Alt ditekan, -1 jika tidak ada.
 * @return 1 jika diklik.
 */
int gui_tab_at(Gui *gui, struct nk_rect rect, const char *label, int mnemonic, int active);

/** @brief Lebar tombol tab untuk sebuah label. */
float gui_tab_width(Gui *gui, const char *label);

/**
 * @brief Menggaris bawahi huruf mnemonic pada label yang digambar di bounds.
 *
 * Garis hanya tampil saat Alt ditekan dan tidak ada dialog terbuka.
 *
 * @param centered 1 jika label rata tengah, 0 jika rata kiri dengan padding.
 */
void gui_mnemonic(Gui *gui, struct nk_rect bounds, const char *label, int index, int centered,
                  const struct nk_user_font *font);

/**
 * @brief Field edit yang juga mencatat apakah ada field teks aktif.
 * @return Flag hasil nk_edit_string_zero_terminated.
 */
nk_flags gui_edit(Gui *gui, nk_flags flags, char *buffer, int max);

/** @brief Memfokuskan field edit berikutnya dengan kursor di akhir teks. */
void gui_focus_edit_end(Gui *gui, nk_flags flags, const char *text);

/** @brief Mengembalikan font sesuai konstanta GUI_FONT_*. */
const struct nk_user_font *gui_font(Gui *gui, int which);

/**
 * @brief Item menu dengan teks shortcut rata kanan.
 * @return 1 jika diklik.
 */
int gui_menu_item(Gui *gui, const char *label, const char *shortcut, int enabled);

/** @brief Tombol yang dapat dinonaktifkan. Mengembalikan 1 jika diklik saat aktif. */
int gui_button(Gui *gui, const char *label, int enabled);

/** @brief Memulai field edit nonaktif dengan tampilan redup. */
void gui_edit_disabled_begin(Gui *gui);

/** @brief Mengakhiri field edit nonaktif. */
void gui_edit_disabled_end(Gui *gui);

/** @brief Ruang kosong vertikal setinggi height. */
void gui_vspace(Gui *gui, float height);

/** @brief Label satu baris dengan warna tertentu. */
void gui_label_colored(Gui *gui, const char *text, struct nk_color color);

/** @brief Label satu baris dengan font tebal. */
void gui_label_bold(Gui *gui, const char *text, struct nk_color color);

/** @brief Teks panjang yang dibungkus otomatis dengan warna tertentu. */
void gui_wrap_colored(Gui *gui, const char *text, struct nk_color color);

/**
 * @brief Menghitung tinggi yang dibutuhkan teks terbungkus pada lebar tertentu.
 */
float gui_wrap_height(Gui *gui, const char *text, float width);

/**
 * @brief Menggambar teks dua segmen (prefix dan isi) pada satu baris.
 *
 * Dipakai tab Tree agar garis cabang dan label node memiliki warna berbeda.
 */
void gui_draw_segments(Gui *gui, struct nk_rect bounds, const char *prefix, struct nk_color prefix_color,
                       const char *text, struct nk_color text_color, const struct nk_user_font *text_font,
                       struct nk_color background);

/** @brief Lebar teks dalam piksel dengan font biasa. */
float gui_text_width(Gui *gui, const char *text);

/**
 * @brief Membuka window dialog di tengah layar.
 * @return 1 jika konten dialog perlu digambar.
 */
int gui_dialog_begin(Gui *gui, const char *title, float width, float height);

/** @brief Menutup window dialog yang dibuka gui_dialog_begin. */
void gui_dialog_end(Gui *gui);

/* ===================================================================== */
/* Main Window Parts                                                     */
/* ===================================================================== */

/** @brief Menggambar menubar File, Edit, Preference, Help. */
void gui_menubar(Gui *gui);

/** @brief Menggambar popup menu yang sedang terbuka. */
void gui_menu_popup(Gui *gui);

/** @brief Memproses shortcut keyboard yang tertunda. */
void gui_handle_shortcut(Gui *gui);

/**
 * @brief Menggambar panel kueri di dalam layout space.
 * @param area Area panel relatif terhadap layout space.
 * @param body_y Posisi atas kotak isi relatif terhadap layout space.
 */
void gui_query_panel(Gui *gui, struct nk_rect area, float body_y);

/** @brief Menggambar bar Q selebar width di dalam layout space. */
void gui_query_bar(Gui *gui, float width);

/** @brief Menyalin isi kotak teks kueri bebas ke draft App. */
void gui_query_commit_text(Gui *gui);

/** @brief Menjalankan Start dari GUI (commit teks lalu inferensi). */
void gui_query_start(Gui *gui);

/**
 * @brief Menggambar panel hasil di dalam layout space.
 * @param area Area panel relatif terhadap layout space.
 * @param body_y Posisi atas kotak isi relatif terhadap layout space.
 */
void gui_result_panel(Gui *gui, struct nk_rect area, float body_y);

/* ===================================================================== */
/* Text View (gui_textview.c)                                            */
/* ===================================================================== */

/** @brief Menginisialisasi dokumen kosong. */
void gui_textdoc_init(GuiTextDoc *doc);

/** @brief Menghapus seluruh baris dokumen. */
void gui_textdoc_clear(GuiTextDoc *doc);

/** @brief Membebaskan dokumen. */
void gui_textdoc_free(GuiTextDoc *doc);

/**
 * @brief Menambahkan baris ke dokumen.
 * @param style Contoh baris yang menentukan warna, font, dan tinggi (text diabaikan).
 * @param text Isi baris (disalin).
 * @return Baris yang baru ditambahkan.
 */
GuiTextLine *gui_textdoc_add(Gui *gui, GuiTextDoc *doc, const GuiTextLine *style, const char *text);

/** @brief Mengosongkan seleksi. */
void gui_textsel_reset(GuiTextSel *sel);

/**
 * @brief Menggambar tampilan teks read-only pada widget berikutnya.
 *
 * Mendukung seleksi dengan drag mouse, Shift+klik, Ctrl+A, dan Ctrl+C saat
 * tampilan sedang fokus.
 *
 * @param focus_id Identitas fokus tampilan (GUI_FOCUS_*).
 * @return Kejadian klik pada frame ini.
 */
GuiTextEvent gui_textview(Gui *gui, const char *id, GuiTextDoc *doc, GuiTextSel *sel, GuiFocus focus_id);

/** @brief Memperbarui cache baris tree jika perlu. */
void gui_tree_refresh(Gui *gui);

/** @brief Mengembalikan warna baris tree berdasarkan status inferensi. */
struct nk_color gui_tree_line_color(Gui *gui, const RdrLine *line, int *is_conclusion);

/* ===================================================================== */
/* Actions (gui_actions.c)                                               */
/* ===================================================================== */

/** @brief Membuka dialog. */
void gui_open_dialog(Gui *gui, GuiDialogId id);

/** @brief Menutup dialog yang sedang terbuka. */
void gui_close_dialog(Gui *gui);

/** @brief Aksi File > New (dengan konfirmasi unsaved). */
void gui_action_new(Gui *gui);

/** @brief Aksi File > Open (dengan konfirmasi unsaved). */
void gui_action_open(Gui *gui);

/** @brief Aksi File > Save. Membuka Save As jika belum ada path. */
void gui_action_save(Gui *gui);

/** @brief Aksi File > Save As. */
void gui_action_save_as(Gui *gui);

/** @brief Aksi keluar aplikasi (dengan konfirmasi unsaved). */
void gui_action_exit(Gui *gui);

/** @brief Menjalankan aksi tertunda setelah konfirmasi unsaved selesai. */
void gui_run_pending(Gui *gui);

/** @brief Aksi Undo dari GUI. */
void gui_action_undo(Gui *gui);

/** @brief Aksi Redo dari GUI. */
void gui_action_redo(Gui *gui);

/** @brief Membuka dialog koreksi untuk hasil terakhir. */
void gui_action_correct(Gui *gui);

/** @brief Membuka dialog edit manual, opsional memilih node tertentu. */
void gui_action_manual(Gui *gui, int node_id);

/** @brief Membuka dialog Edit Preference. */
void gui_action_prefs(Gui *gui);

/** @brief Membuka file browser dengan mode tertentu. */
void gui_open_file_dialog(Gui *gui, GuiFileMode mode);

/** @brief Menerapkan file yang dipilih file browser sesuai modenya. */
void gui_file_chosen(Gui *gui, const char *path);

/** @brief Menyesuaikan font dan tema dengan preferensi App. */
void gui_sync_appearance(Gui *gui);

/** @brief Mengosongkan cache tampilan setelah project diganti. */
void gui_reset_view(Gui *gui);
/** @brief Memulai replay langkah reasoning pada tree. */
void gui_reasoning_start(Gui *gui);
/** @brief Memajukan replay reasoning berdasarkan timer satu detik. */
void gui_reasoning_tick(Gui *gui);

/* ===================================================================== */
/* Dialogs                                                               */
/* ===================================================================== */

/** @brief Menggambar dialog yang sedang terbuka. */
void gui_dialogs(Gui *gui);

/** @brief Menggambar file browser. */
void gui_file_dialog(Gui *gui);

/** @brief Mengisi ulang isi folder file browser. */
void gui_file_dialog_load(Gui *gui, const char *dir);

/** @brief Menyiapkan state dialog koreksi dari hasil terakhir. */
void gui_correct_dialog_prepare(Gui *gui);

/** @brief Membebaskan state dialog koreksi. */
void gui_correct_dialog_reset(Gui *gui);

/** @brief Menggambar dialog koreksi. */
void gui_correct_dialog(Gui *gui);

/** @brief Menggambar dialog edit manual. */
void gui_manual_dialog(Gui *gui);

/** @brief Memuat isi node terpilih ke form edit manual. */
void gui_manual_dialog_load(Gui *gui, int node_id);

/** @brief Menggambar dialog Edit Preference. */
void gui_prefs_dialog(Gui *gui);

/* ===================================================================== */
/* Test Hook (gui_testhook.c)                                            */
/* ===================================================================== */

/**
 * @brief Memuat skrip test hook.
 * @return 1 jika berhasil atau test hook tidak dipakai, 0 jika gagal.
 */
int gui_testhook_init(Gui *gui, const char *script_path, const char *shot_dir);

/**
 * @brief Menjalankan langkah skrip berikutnya sebelum frame digambar.
 * @return 1 jika test hook sedang aktif (render tidak boleh menunggu event).
 */
int gui_testhook_step(Gui *gui);

/** @brief Dipanggil setelah render untuk mengambil screenshot yang diminta. */
void gui_testhook_after_render(Gui *gui);

/**
 * @brief Membebaskan state test hook.
 * @return Jumlah perintah expect yang gagal.
 */
int gui_testhook_shutdown(Gui *gui);

#endif /* RDR_GUI_H */
