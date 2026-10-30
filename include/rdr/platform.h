/**
 * @file platform.h
 * @brief Abstraksi sistem file dan path untuk UNIX dan Windows.
 *
 * Seluruh path yang diterima dan dikembalikan modul ini berupa UTF-8. Pada
 * Windows, path dikonversi ke UTF-16 sebelum memanggil API sistem sehingga
 * nama file non-ASCII tetap berfungsi.
 */

#ifndef RDR_PLATFORM_H
#define RDR_PLATFORM_H

#include <stddef.h>
#include <stdio.h>

#include "rdr/strutil.h"

/** Ukuran file maksimum yang dibaca oleh platform_read_file (64 MiB). */
#define PLATFORM_MAX_FILE_SIZE (64UL * 1024UL * 1024UL)

/**
 * @brief Membuka file dengan path UTF-8.
 * @param path Path file.
 * @param mode Mode gaya fopen, misalnya "rb".
 * @return Handle file atau NULL jika gagal.
 */
FILE *platform_fopen(const char *path, const char *mode);

/**
 * @brief Membaca seluruh isi file ke memori.
 *
 * Hasil selalu diakhiri byte nol tambahan yang tidak dihitung dalam len.
 *
 * @param path Path file.
 * @param out Tempat pointer isi file (milik pemanggil).
 * @param len Tempat panjang isi file (boleh NULL).
 * @param err Buffer pesan error (boleh NULL).
 * @param errlen Ukuran buffer pesan error.
 * @return 1 jika berhasil, 0 jika gagal.
 */
int platform_read_file(const char *path, char **out, size_t *len, char *err, size_t errlen);

/**
 * @brief Menulis data ke file secara atomik.
 *
 * Data ditulis ke file sementara di folder yang sama lalu di-rename ke path
 * tujuan, sehingga file lama tidak rusak jika penulisan gagal di tengah jalan.
 *
 * @return 1 jika berhasil, 0 jika gagal.
 */
int platform_write_file(const char *path, const char *data, size_t len, char *err, size_t errlen);

/**
 * @brief Satu entri direktori.
 */
typedef struct {
    char *name;  /**< Nama entri (tanpa path). */
    int is_dir;  /**< 1 jika direktori. */
} PlatformDirEntry;

/**
 * @brief Daftar entri direktori.
 */
typedef struct {
    PlatformDirEntry *items;  /**< Array entri. */
    size_t count;             /**< Jumlah entri. */
    size_t cap;               /**< Kapasitas array. */
} PlatformDirList;

/**
 * @brief Membaca isi direktori.
 *
 * Entri "." dan ".." tidak dimasukkan. Hasil diurutkan: direktori lebih
 * dulu, lalu berdasarkan nama tanpa membedakan huruf besar dan kecil.
 *
 * @param out Daftar tujuan. Isi sebelumnya dibuang.
 * @return 1 jika berhasil, 0 jika gagal.
 */
int platform_list_dir(const char *path, PlatformDirList *out, char *err, size_t errlen);

/** @brief Menginisialisasi daftar entri kosong. */
void platform_dirlist_init(PlatformDirList *list);

/** @brief Membebaskan daftar entri. */
void platform_dirlist_free(PlatformDirList *list);

/**
 * @brief Mengembalikan direktori kerja saat ini.
 * @return String baru milik pemanggil.
 */
char *platform_getcwd(void);

/**
 * @brief Mengembalikan direktori home user, atau direktori kerja jika tidak diketahui.
 * @return String baru milik pemanggil.
 */
char *platform_home_dir(void);

/**
 * @brief Mengisi daftar root sistem file.
 *
 * Pada UNIX hasilnya "/". Pada Windows hasilnya daftar drive, misalnya "C:\".
 */
void platform_list_roots(StrList *out);

/** @brief Memeriksa apakah path adalah direktori yang ada. */
int platform_is_dir(const char *path);

/** @brief Memeriksa apakah path adalah file biasa yang ada. */
int platform_file_exists(const char *path);

/** @brief Mengembalikan karakter pemisah path utama platform. */
char platform_path_separator(void);

/**
 * @brief Menggabungkan direktori dan nama.
 * @return String baru milik pemanggil.
 */
char *platform_path_join(const char *dir, const char *name);

/**
 * @brief Mengembalikan direktori induk dari path.
 *
 * Root (misalnya "/" atau "C:\") mengembalikan dirinya sendiri.
 *
 * @return String baru milik pemanggil.
 */
char *platform_path_parent(const char *path);

/**
 * @brief Mengembalikan bagian nama file dari path.
 * @return Pointer ke dalam path (bukan alokasi baru).
 */
const char *platform_path_basename(const char *path);

/**
 * @brief Memeriksa apakah path diakhiri ekstensi tertentu (tanpa membedakan huruf).
 * @param ext Ekstensi termasuk titik, misalnya ".json".
 */
int platform_path_has_ext(const char *path, const char *ext);

#endif /* RDR_PLATFORM_H */
