/**
 * @file platform.c
 * @brief Implementasi abstraksi sistem file untuk UNIX dan Windows.
 */

#if !defined(_WIN32)
#define _POSIX_C_SOURCE 200809L
#endif

#include "rdr/platform.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#include <wchar.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

/* ===================================================================== */
/* Windows String Conversion                                             */
/* ===================================================================== */

#if defined(_WIN32)
static wchar_t *utf8_to_wide(const char *s)
{
    int n = MultiByteToWideChar(CP_UTF8, 0, s, -1, NULL, 0);
    wchar_t *w;

    if (n <= 0) {
        return NULL;
    }
    w = xmalloc((size_t)n * sizeof(wchar_t));
    MultiByteToWideChar(CP_UTF8, 0, s, -1, w, n);
    return w;
}

static char *wide_to_utf8(const wchar_t *w)
{
    int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, NULL, 0, NULL, NULL);
    char *s;

    if (n <= 0) {
        return str_dup("");
    }
    s = xmalloc((size_t)n);
    WideCharToMultiByte(CP_UTF8, 0, w, -1, s, n, NULL, NULL);
    return s;
}
#endif

/* ===================================================================== */
/* File Access                                                           */
/* ===================================================================== */

FILE *platform_fopen(const char *path, const char *mode)
{
#if defined(_WIN32)
    wchar_t *wpath = utf8_to_wide(path);
    wchar_t *wmode = utf8_to_wide(mode);
    FILE *f = NULL;

    if (wpath != NULL && wmode != NULL) {
        f = _wfopen(wpath, wmode);
    }
    free(wpath);
    free(wmode);
    return f;
#else
    return fopen(path, mode);
#endif
}

int platform_read_file(const char *path, char **out, size_t *len, char *err, size_t errlen)
{
    FILE *f;
    StrBuf sb;
    char chunk[8192];
    size_t n;

    *out = NULL;
    f = platform_fopen(path, "rb");
    if (f == NULL) {
        str_set_error(err, errlen, "cannot open '%s': %s", path, strerror(errno));
        return 0;
    }
    strbuf_init(&sb);
    strbuf_append(&sb, "");
    while ((n = fread(chunk, 1, sizeof(chunk), f)) > 0) {
        if (sb.len + n > PLATFORM_MAX_FILE_SIZE) {
            str_set_error(err, errlen, "'%s' is larger than %lu MiB", path,
                          PLATFORM_MAX_FILE_SIZE / (1024UL * 1024UL));
            fclose(f);
            strbuf_free(&sb);
            return 0;
        }
        strbuf_append_n(&sb, chunk, n);
    }
    if (ferror(f)) {
        str_set_error(err, errlen, "error while reading '%s'", path);
        fclose(f);
        strbuf_free(&sb);
        return 0;
    }
    fclose(f);
    if (len != NULL) {
        *len = sb.len;
    }
    *out = strbuf_steal(&sb);
    return 1;
}

int platform_write_file(const char *path, const char *data, size_t len, char *err, size_t errlen)
{
    char *tmp = str_printf("%s.tmp", path);
    FILE *f;
    int ok;

    /* Temporary File Write */
    f = platform_fopen(tmp, "wb");
    if (f == NULL) {
        str_set_error(err, errlen, "cannot write '%s': %s", path, strerror(errno));
        free(tmp);
        return 0;
    }
    ok = fwrite(data, 1, len, f) == len;
    ok = (fclose(f) == 0) && ok;
    if (!ok) {
        str_set_error(err, errlen, "error while writing '%s'", path);
        remove(tmp);
        free(tmp);
        return 0;
    }

    /* Atomic Replace */
#if defined(_WIN32)
    {
        wchar_t *wtmp = utf8_to_wide(tmp);
        wchar_t *wpath = utf8_to_wide(path);
        ok = wtmp && wpath && MoveFileExW(wtmp, wpath, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
        if (!ok && wtmp != NULL) {
            DeleteFileW(wtmp);
        }
        free(wtmp);
        free(wpath);
    }
#else
    ok = rename(tmp, path) == 0;
    if (!ok) {
        remove(tmp);
    }
#endif
    if (!ok) {
        str_set_error(err, errlen, "cannot replace '%s'", path);
    }
    free(tmp);
    return ok;
}

/* ===================================================================== */
/* Directory Listing                                                     */
/* ===================================================================== */

void platform_dirlist_init(PlatformDirList *list)
{
    list->items = NULL;
    list->count = 0;
    list->cap = 0;
}

void platform_dirlist_free(PlatformDirList *list)
{
    size_t i;

    for (i = 0; i < list->count; i++) {
        free(list->items[i].name);
    }
    free(list->items);
    platform_dirlist_init(list);
}

static void dirlist_push(PlatformDirList *list, char *name, int is_dir)
{
    if (list->count == list->cap) {
        list->cap = list->cap ? list->cap * 2 : 32;
        list->items = xrealloc(list->items, list->cap * sizeof(PlatformDirEntry));
    }
    list->items[list->count].name = name;
    list->items[list->count].is_dir = is_dir;
    list->count++;
}

static int ci_compare(const char *a, const char *b)
{
    for (; *a != '\0' && *b != '\0'; a++, b++) {
        int ca = (*a >= 'A' && *a <= 'Z') ? *a + 32 : (unsigned char)*a;
        int cb = (*b >= 'A' && *b <= 'Z') ? *b + 32 : (unsigned char)*b;
        if (ca != cb) {
            return ca - cb;
        }
    }
    return (unsigned char)*a - (unsigned char)*b;
}

static int entry_compare(const void *pa, const void *pb)
{
    const PlatformDirEntry *a = pa;
    const PlatformDirEntry *b = pb;

    if (a->is_dir != b->is_dir) {
        return b->is_dir - a->is_dir;
    }
    return ci_compare(a->name, b->name);
}

int platform_list_dir(const char *path, PlatformDirList *out, char *err, size_t errlen)
{
    platform_dirlist_free(out);
#if defined(_WIN32)
    {
        char *pattern = platform_path_join(path, "*");
        wchar_t *wpattern = utf8_to_wide(pattern);
        WIN32_FIND_DATAW fd;
        HANDLE h = wpattern ? FindFirstFileW(wpattern, &fd) : INVALID_HANDLE_VALUE;

        free(pattern);
        free(wpattern);
        if (h == INVALID_HANDLE_VALUE) {
            str_set_error(err, errlen, "cannot open folder '%s'", path);
            return 0;
        }
        do {
            if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) {
                continue;
            }
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) {
                continue;
            }
            dirlist_push(out, wide_to_utf8(fd.cFileName),
                         (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0);
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
#else
    {
        DIR *d = opendir(path);
        struct dirent *e;

        if (d == NULL) {
            str_set_error(err, errlen, "cannot open folder '%s': %s", path, strerror(errno));
            return 0;
        }
        while ((e = readdir(d)) != NULL) {
            char *full;
            if (e->d_name[0] == '.') {
                continue;  /* lewati ".", "..", dan file tersembunyi */
            }
            full = platform_path_join(path, e->d_name);
            dirlist_push(out, str_dup(e->d_name), platform_is_dir(full));
            free(full);
        }
        closedir(d);
    }
#endif
    if (out->count > 1) {
        qsort(out->items, out->count, sizeof(PlatformDirEntry), entry_compare);
    }
    return 1;
}

/* ===================================================================== */
/* Well-known Locations                                                  */
/* ===================================================================== */

char *platform_getcwd(void)
{
#if defined(_WIN32)
    wchar_t buf[MAX_PATH * 4];
    DWORD n = GetCurrentDirectoryW((DWORD)(sizeof(buf) / sizeof(buf[0])), buf);

    if (n == 0 || n >= sizeof(buf) / sizeof(buf[0])) {
        return str_dup("C:\\");
    }
    return wide_to_utf8(buf);
#else
    char buf[4096];

    if (getcwd(buf, sizeof(buf)) == NULL) {
        return str_dup("/");
    }
    return str_dup(buf);
#endif
}

char *platform_home_dir(void)
{
#if defined(_WIN32)
    const wchar_t *home = _wgetenv(L"USERPROFILE");

    if (home != NULL && home[0] != L'\0') {
        return wide_to_utf8(home);
    }
#else
    const char *home = getenv("HOME");

    if (home != NULL && home[0] != '\0') {
        return str_dup(home);
    }
#endif
    return platform_getcwd();
}

void platform_list_roots(StrList *out)
{
#if defined(_WIN32)
    DWORD mask = GetLogicalDrives();
    int i;

    for (i = 0; i < 26; i++) {
        if (mask & (1UL << i)) {
            strlist_pushf(out, "%c:\\", 'A' + i);
        }
    }
#else
    strlist_push(out, "/");
#endif
}

int platform_is_dir(const char *path)
{
#if defined(_WIN32)
    wchar_t *w = utf8_to_wide(path);
    DWORD attr = w ? GetFileAttributesW(w) : INVALID_FILE_ATTRIBUTES;

    free(w);
    return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY);
#else
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
#endif
}

int platform_file_exists(const char *path)
{
#if defined(_WIN32)
    wchar_t *w = utf8_to_wide(path);
    DWORD attr = w ? GetFileAttributesW(w) : INVALID_FILE_ATTRIBUTES;

    free(w);
    return attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY);
#else
    struct stat st;
    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
#endif
}

/* ===================================================================== */
/* Path Manipulation                                                     */
/* ===================================================================== */

static int is_sep(char c)
{
#if defined(_WIN32)
    return c == '/' || c == '\\';
#else
    return c == '/';
#endif
}

char platform_path_separator(void)
{
#if defined(_WIN32)
    return '\\';
#else
    return '/';
#endif
}

char *platform_path_join(const char *dir, const char *name)
{
    size_t n = strlen(dir);

    if (n == 0) {
        return str_dup(name);
    }
    if (is_sep(dir[n - 1])) {
        return str_printf("%s%s", dir, name);
    }
    return str_printf("%s%c%s", dir, platform_path_separator(), name);
}

/* Panjang bagian root pada path, misalnya 1 untuk "/" atau 3 untuk "C:\". */
static size_t root_length(const char *path)
{
#if defined(_WIN32)
    if (path[0] != '\0' && path[1] == ':') {
        return is_sep(path[2]) ? 3 : 2;
    }
#endif
    return is_sep(path[0]) ? 1 : 0;
}

char *platform_path_parent(const char *path)
{
    size_t root = root_length(path);
    size_t n = strlen(path);

    while (n > root && is_sep(path[n - 1])) {
        n--;
    }
    while (n > root && !is_sep(path[n - 1])) {
        n--;
    }
    while (n > root && is_sep(path[n - 1])) {
        n--;
    }
    if (n == 0) {
        return str_dup(".");
    }
    return str_ndup(path, n);
}

const char *platform_path_basename(const char *path)
{
    const char *base = path;
    const char *p;

    for (p = path; *p != '\0'; p++) {
        if (is_sep(*p) && p[1] != '\0') {
            base = p + 1;
        }
    }
    return base;
}

int platform_path_has_ext(const char *path, const char *ext)
{
    size_t n = strlen(path);
    size_t e = strlen(ext);

    return n >= e && str_iequals(path + n - e, ext);
}
