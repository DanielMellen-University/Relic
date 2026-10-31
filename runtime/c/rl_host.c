/* rl_host.c: the host_* primitives in plain C (files, folders, clocks,
 * float math, process). Same names, arity and messages as the C++ host
 * (src/builtins/builtins.cpp). Paths are UTF-8; on Windows they are
 * converted to UTF-16 for the wide file APIs. */
#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif
#include "relic.h"
#include "rl_internal.h"

#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <direct.h>
#include <fcntl.h>
#include <io.h>
#include <sys/stat.h>
#include <sys/types.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#define S(v) ((rl_str *)(v).u.o)
#define L(v) ((rl_list *)(v).u.o)
#define BI(name) static int name(rl_ctx *cx, rl_fun *self, int argc, const rl_value *a, rl_value *out)
#define UNUSED (void)self; (void)argc

/* ---- platform helpers ---- */

#ifdef _WIN32
static wchar_t *wide(const char *s) {
    int n = MultiByteToWideChar(CP_UTF8, 0, s, -1, NULL, 0);
    wchar_t *w = (wchar_t *)malloc((size_t)(n > 0 ? n : 1) * sizeof(wchar_t));
    if (!w) abort();
    if (n <= 0) w[0] = 0;
    else MultiByteToWideChar(CP_UTF8, 0, s, -1, w, n);
    return w;
}
static char *narrow(const wchar_t *w) {
    int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, NULL, 0, NULL, NULL);
    char *s = (char *)malloc((size_t)(n > 0 ? n : 1));
    if (!s) abort();
    if (n <= 0) s[0] = 0;
    else WideCharToMultiByte(CP_UTF8, 0, w, -1, s, n, NULL, NULL);
    return s;
}
static FILE *open_file(const char *path, const char *mode) {
    wchar_t *p = wide(path), *m = wide(mode);
    FILE *f = _wfopen(p, m);
    free(p); free(m);
    return f;
}
static int stat_path(const char *path, int *is_dir, long long *size) {
    struct _stat64 st;
    wchar_t *p = wide(path);
    int r = _wstat64(p, &st);
    free(p);
    if (r != 0) return 0;
    if (is_dir) *is_dir = (st.st_mode & _S_IFDIR) != 0;
    if (size) *size = (long long)st.st_size;
    return 1;
}
static int make_one_dir(const char *path) {
    wchar_t *p = wide(path);
    int r = _wmkdir(p);
    free(p);
    return r == 0 || errno == EEXIST;
}
#else
static FILE *open_file(const char *path, const char *mode) { return fopen(path, mode); }
static int stat_path(const char *path, int *is_dir, long long *size) {
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    if (is_dir) *is_dir = S_ISDIR(st.st_mode) ? 1 : 0;
    if (size) *size = (long long)st.st_size;
    return 1;
}
static int make_one_dir(const char *path) { return mkdir(path, 0777) == 0 || errno == EEXIST; }
#endif

/* ---- files ---- */

BI(h_read_file) {
    FILE *f;
    char *data = NULL;
    size_t n = 0, cap = 0, got;
    UNUSED;
    if (rl_arg_str(cx, a[0], "path")) return RL_ERR;
    f = open_file(S(a[0])->data, "rb");
    if (f) {   /* a folder opens fine on Linux and reads as nothing */
        int is_dir = 0;
        if (stat_path(S(a[0])->data, &is_dir, NULL) && is_dir) { fclose(f); f = NULL; }
    }
    if (!f) return rl_fail(cx, "cannot open %s", S(a[0])->data);
    for (;;) {
        if (n == cap) {
            cap = cap ? cap * 2 : 4096;
            data = (char *)realloc(data, cap);
            if (!data) abort();
        }
        got = fread(data + n, 1, cap - n, f);
        n += got;
        if (got == 0) break;
    }
    fclose(f);
    *out = rl_str_new(data, n);
    free(data);
    return RL_OK;
}

BI(h_write_file) {
    FILE *f;
    int app = a[2].k == RL_BOOL && a[2].u.b;
    UNUSED;
    if (rl_arg_str(cx, a[0], "path")) return RL_ERR;
    f = open_file(S(a[0])->data, app ? "ab" : "wb");
    if (!f) return rl_fail(cx, "cannot write %s", S(a[0])->data);
    if (rl_arg_str(cx, a[1], "text")) { fclose(f); return RL_ERR; }
    fwrite(S(a[1])->data, 1, S(a[1])->len, f);
    fclose(f);
    *out = rl_none();
    return RL_OK;
}

BI(h_file_exists) {
    UNUSED;
    if (rl_arg_str(cx, a[0], "path")) return RL_ERR;
    *out = rl_bool(stat_path(S(a[0])->data, NULL, NULL));
    return RL_OK;
}

BI(h_is_dir) {
    int d = 0;
    UNUSED;
    if (rl_arg_str(cx, a[0], "path")) return RL_ERR;
    *out = rl_bool(stat_path(S(a[0])->data, &d, NULL) && d);
    return RL_OK;
}

BI(h_file_size) {
    int d = 0;
    long long size = 0;
    UNUSED;
    if (rl_arg_str(cx, a[0], "path")) return RL_ERR;
    if (!stat_path(S(a[0])->data, &d, &size) || d) return rl_fail(cx, "cannot get the size of %s", S(a[0])->data);
    *out = rl_int(size);
    return RL_OK;
}

static int cmp_names(const void *x, const void *y) {
    /* the list holds rl_value items, each a string */
    const rl_str *a = (const rl_str *)((const rl_value *)x)->u.o, *b = (const rl_str *)((const rl_value *)y)->u.o;
    size_t n = a->len < b->len ? a->len : b->len;
    int c = memcmp(a->data, b->data, n);
    if (c) return c;
    return a->len < b->len ? -1 : (a->len > b->len ? 1 : 0);
}

static void add_name(rl_value list, const char *name) {
    rl_value s;
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) return;
    s = rl_str_c(name);
    rl_list_push(L(list), s);
    rl_release(s);
}

/* Names in a folder, sorted by bytes like std::sort on the host. */
BI(h_list_dir) {
    const char *p;
    rl_value l;
    UNUSED;
    if (rl_arg_str(cx, a[0], "path")) return RL_ERR;
    p = S(a[0])->data;
    l = rl_list_new(0);
#ifdef _WIN32
    {
        size_t n = strlen(p);
        char *pat = (char *)malloc(n + 3);
        wchar_t *w;
        WIN32_FIND_DATAW fd;
        HANDLE h;
        if (!pat) abort();
        memcpy(pat, p, n);
        if (n && p[n - 1] != '/' && p[n - 1] != '\\') pat[n++] = '/';
        pat[n++] = '*';
        pat[n] = 0;
        w = wide(pat);
        free(pat);
        h = FindFirstFileW(w, &fd);
        free(w);
        if (h == INVALID_HANDLE_VALUE) { rl_release(l); return rl_fail(cx, "cannot list %s", p); }
        do {
            char *name = narrow(fd.cFileName);
            add_name(l, name);
            free(name);
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
#else
    {
        DIR *d = opendir(p);
        struct dirent *e;
        if (!d) { rl_release(l); return rl_fail(cx, "cannot list %s", p); }
        while ((e = readdir(d)) != NULL) add_name(l, e->d_name);
        closedir(d);
    }
#endif
    if (L(l)->n > 1) qsort(L(l)->items, L(l)->n, sizeof(rl_value), cmp_names);
    *out = l;
    return RL_OK;
}

/* Makes the folder and any missing parents. */
BI(h_make_dir) {
    const char *p;
    char *tmp;
    size_t n, i;
    int d = 0;
    UNUSED;
    if (rl_arg_str(cx, a[0], "path")) return RL_ERR;
    p = S(a[0])->data;
    n = strlen(p);
    tmp = (char *)malloc(n + 1);
    if (!tmp) abort();
    memcpy(tmp, p, n + 1);
    for (i = 1; i < n; ++i)
        if (tmp[i] == '/' || tmp[i] == '\\') {
            char c = tmp[i];
            tmp[i] = 0;
            if (!(i == 2 && tmp[1] == ':')) make_one_dir(tmp);
            tmp[i] = c;
        }
    if (n) make_one_dir(tmp);
    free(tmp);
    if (!stat_path(p, &d, NULL) || !d) return rl_fail(cx, "cannot make folder %s", p);
    *out = rl_none();
    return RL_OK;
}

/* Removes a file or an empty folder. */
BI(h_remove) {
    const char *p;
    int d = 0, ok;
    UNUSED;
    if (rl_arg_str(cx, a[0], "path")) return RL_ERR;
    p = S(a[0])->data;
    if (!stat_path(p, &d, NULL)) return rl_fail(cx, "cannot remove %s", p);
#ifdef _WIN32
    {
        wchar_t *w = wide(p);
        ok = d ? _wrmdir(w) == 0 : _wremove(w) == 0;
        free(w);
    }
#else
    ok = d ? rmdir(p) == 0 : remove(p) == 0;
#endif
    if (!ok) return rl_fail(cx, "cannot remove %s", p);
    *out = rl_none();
    return RL_OK;
}

/* Renames, replacing the target if it exists (std::filesystem::rename). */
BI(h_rename) {
    int ok;
    UNUSED;
    if (rl_arg_str(cx, a[0], "from") || rl_arg_str(cx, a[1], "to")) return RL_ERR;
#ifdef _WIN32
    {
        wchar_t *f = wide(S(a[0])->data), *t = wide(S(a[1])->data);
        ok = MoveFileExW(f, t, MOVEFILE_REPLACE_EXISTING) != 0;
        free(f); free(t);
    }
#else
    ok = rename(S(a[0])->data, S(a[1])->data) == 0;
#endif
    if (!ok) return rl_fail(cx, "cannot rename %s", S(a[0])->data);
    *out = rl_none();
    return RL_OK;
}

BI(h_cwd) {
    UNUSED;
    (void)cx; (void)a;
#ifdef _WIN32
    {
        wchar_t *w = _wgetcwd(NULL, 0);
        char *s, *c;
        if (!w) { *out = rl_str_c(""); return RL_OK; }
        s = narrow(w);
        free(w);
        for (c = s; *c; ++c) if (*c == '\\') *c = '/';
        *out = rl_str_c(s);
        free(s);
    }
#else
    {
        size_t cap = 256;
        char *b = NULL;
        for (;;) {
            b = (char *)realloc(b, cap);
            if (!b) abort();
            if (getcwd(b, cap)) break;
            if (errno != ERANGE) { b[0] = 0; break; }
            cap *= 2;
        }
        *out = rl_str_c(b);
        free(b);
    }
#endif
    return RL_OK;
}

BI(h_env) {
    const char *v;
    UNUSED;
    if (rl_arg_str(cx, a[0], "name")) return RL_ERR;
    v = getenv(S(a[0])->data);
    *out = v ? rl_str_c(v) : rl_none();
    return RL_OK;
}

/* host_read_line(prompt): the next line of standard input without its line end
 * ("\n" or "\r\n"), or none at the end of the input. On a terminal the prompt
 * is shown first (and a newline follows at the end of the input); otherwise the
 * prompt is ignored. Same as the C++ host's. */
BI(h_read_line) {
    char *data = NULL;
    size_t n = 0, cap = 0;
    int c, tty, any = 0;
    static int binary = 0;
    UNUSED;
    if (rl_arg_str(cx, a[0], "prompt")) return RL_ERR;
    if (!binary) {
        binary = 1;
#ifdef _WIN32
        _setmode(_fileno(stdin), _O_BINARY);
#endif
    }
#ifdef _WIN32
    tty = _isatty(_fileno(stdin)) != 0;
#else
    tty = isatty(0) != 0;
#endif
    if (tty && S(a[0])->len > 0) {
        fwrite(S(a[0])->data, 1, S(a[0])->len, cx->out);
        fflush(cx->out);
    }
    while ((c = getc(stdin)) != EOF) {
        any = 1;
        if (c == '\n') {
            if (n > 0 && data[n - 1] == '\r') --n;
            *out = rl_str_new(data ? data : "", n);
            free(data);
            return RL_OK;
        }
        if (n == cap) {
            cap = cap ? cap * 2 : 128;
            data = (char *)realloc(data, cap);
            if (!data) abort();
        }
        data[n++] = (char)c;
    }
    if (!any) {
        if (tty && S(a[0])->len > 0) { fputc('\n', cx->out); fflush(cx->out); }
        *out = rl_none();
        return RL_OK;
    }
    *out = rl_str_new(data, n);
    free(data);
    return RL_OK;
}

BI(h_exit) {
    long long code;
    UNUSED;
    if (rl_arg_whole(cx, a[0], "code", &code)) return RL_ERR;
    *out = rl_none();
    fflush(cx->out);
    fflush(stdout);
    exit((int)code);
}

/* ---- clocks ---- */

BI(h_clock) {
    UNUSED;
    (void)cx; (void)a;
#ifdef _WIN32
    {
        LARGE_INTEGER f, c;
        QueryPerformanceFrequency(&f);
        QueryPerformanceCounter(&c);
        *out = rl_float((double)c.QuadPart / (double)f.QuadPart);
    }
#else
    {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        *out = rl_float((double)ts.tv_sec + (double)ts.tv_nsec / 1e9);
    }
#endif
    return RL_OK;
}

BI(h_now) {
    UNUSED;
    (void)cx; (void)a;
#ifdef _WIN32
    {
        FILETIME ft;
        unsigned long long t;
        GetSystemTimeAsFileTime(&ft);
        t = ((unsigned long long)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
        *out = rl_float((double)(t - 116444736000000000ULL) / 1e7);
    }
#else
    {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        *out = rl_float((double)ts.tv_sec + (double)ts.tv_nsec / 1e9);
    }
#endif
    return RL_OK;
}

BI(h_sleep) {
    double ms;
    UNUSED;
    if (rl_arg_num(cx, a[0], "milliseconds", &ms)) return RL_ERR;
    if (ms > 0) {
#ifdef _WIN32
        Sleep((DWORD)ms);
#else
        struct timespec ts;
        long long us = (long long)(ms * 1000);
        ts.tv_sec = (time_t)(us / 1000000);
        ts.tv_nsec = (long)((us % 1000000) * 1000);
        while (nanosleep(&ts, &ts) != 0 && errno == EINTR) {}
#endif
    }
    *out = rl_none();
    return RL_OK;
}

/* ---- float math ---- */

#define MATH1(fname, cfun) \
    BI(fname) { double x; UNUSED; if (rl_arg_num(cx, a[0], "argument", &x)) return RL_ERR; *out = rl_float(cfun(x)); return RL_OK; }
MATH1(h_floor, floor)
MATH1(h_sqrt, sqrt)
MATH1(h_sin, sin)
MATH1(h_cos, cos)
MATH1(h_exp, exp)
MATH1(h_log, log)

BI(h_pow) {
    double x, y;
    UNUSED;
    if (rl_arg_num(cx, a[0], "base", &x) || rl_arg_num(cx, a[1], "power", &y)) return RL_ERR;
    *out = rl_float(pow(x, y));
    return RL_OK;
}

/* ---- identity and reflection ---- */

BI(h_same) {
    UNUSED;
    (void)cx;
    *out = rl_bool(a[0].k == a[1].k && (a[0].k == RL_LIST || a[0].k == RL_MAP) && a[0].u.o == a[1].u.o);
    return RL_OK;
}

/* host_find(text, part): the first `part` in `text` in characters, or -1, as
 * on the host: byte matches by Knuth-Morris-Pratt, kept only where both ends
 * fall between characters as len and slice count them. */
BI(h_find) {
    const rl_str *s, *t;
    size_t n, m, i, j, q, k, *f;
    char *starts = NULL;
    int ascii = 1;
    long long r = -1;
    UNUSED;
    if (rl_arg_str(cx, a[0], "text") || rl_arg_str(cx, a[1], "part")) return RL_ERR;
    s = S(a[0]);
    t = S(a[1]);
    n = s->len;
    m = t->len;
    if (m == 0) { *out = rl_int(0); return RL_OK; }
    if (m > n) { *out = rl_int(-1); return RL_OK; }
    for (i = 0; i < n; ++i)
        if ((unsigned char)s->data[i] >= 0x80) { ascii = 0; break; }
    if (!ascii) {
        starts = (char *)calloc(n + 1, 1);
        if (!starts) abort();
        for (i = 0; i < n; i += rl_utf8_char_size(s->data, n, i)) starts[i] = 1;
        starts[n] = 1;
    }
    f = (size_t *)malloc(m * sizeof *f);
    if (!f) abort();
    f[0] = 0;
    for (q = 1, k = 0; q < m; ++q) {
        while (k > 0 && t->data[q] != t->data[k]) k = f[k - 1];
        if (t->data[q] == t->data[k]) ++k;
        f[q] = k;
    }
    for (i = 0, j = 0; i < n; ++i) {
        while (j > 0 && s->data[i] != t->data[j]) j = f[j - 1];
        if (s->data[i] == t->data[j]) ++j;
        if (j == m) {
            size_t at = i + 1 - m, b;
            if (ascii) { r = (long long)at; break; }
            if (starts[at] && starts[i + 1]) {
                r = 0;
                for (b = 0; b < at; ++b) r += starts[b];
                break;
            }
            j = f[j - 1];
        }
    }
    free(f);
    free(starts);
    *out = rl_int(r);
    return RL_OK;
}

static int sort_strs;
static int sort_less(const rl_value *x, const rl_value *y) {
    if (sort_strs) {
        const rl_str *p = S(*x), *q = S(*y);
        size_t n = p->len < q->len ? p->len : q->len;
        int c = memcmp(p->data, q->data, n);
        return c ? c < 0 : p->len < q->len;
    }
    if (x->k == RL_INT && y->k == RL_INT) return x->u.i < y->u.i;
    return (x->k == RL_INT ? (double)x->u.i : x->u.f) < (y->k == RL_INT ? (double)y->u.i : y->u.f);
}
/* The same top-down merge as std/lists.sort_by: split at n / 2, take from the
 * right half only when its item is less. */
static void sort_range(const rl_value **v, const rl_value **tmp, size_t lo, size_t hi) {
    size_t n = hi - lo, mid, i, j, o;
    if (n <= 1) return;
    mid = lo + n / 2;
    sort_range(v, tmp, lo, mid);
    sort_range(v, tmp, mid, hi);
    i = lo; j = mid; o = lo;
    while (i < mid && j < hi) tmp[o++] = sort_less(v[j], v[i]) ? v[j++] : v[i++];
    while (i < mid) tmp[o++] = v[i++];
    while (j < hi) tmp[o++] = v[j++];
    memcpy(v + lo, tmp + lo, n * sizeof *v);
}
/* host_sort(list): a sorted copy when the list holds only numbers or only
 * strings, else none (std/lists.sort then sorts in Relic). */
BI(h_sort) {
    const rl_list *l;
    const rl_value **v, **tmp;
    size_t i;
    int nums = 1, strs = 1;
    rl_value r;
    UNUSED;
    (void)cx;
    *out = rl_none();
    if (a[0].k != RL_LIST) return RL_OK;
    l = L(a[0]);
    for (i = 0; i < l->n; ++i) {
        int k = l->items[i].k;
        if (k != RL_INT && k != RL_FLOAT) nums = 0;
        if (k != RL_STR) strs = 0;
    }
    if (!nums && !strs) return RL_OK;
    v = (const rl_value **)malloc((l->n ? l->n : 1) * sizeof *v);
    tmp = (const rl_value **)malloc((l->n ? l->n : 1) * sizeof *tmp);
    if (!v || !tmp) abort();
    for (i = 0; i < l->n; ++i) v[i] = &l->items[i];
    sort_strs = strs;
    sort_range(v, tmp, 0, l->n);
    r = rl_list_new(l->n);
    for (i = 0; i < l->n; ++i) rl_list_push(L(r), *v[i]);
    free(v);
    free(tmp);
    *out = r;
    return RL_OK;
}

/* Where `use std/...` looks: $RELIC_STD if set, else the std/ folder this
 * runtime was built from (RL_STD_DIR from CMake). Ends in a slash. */
#ifndef RL_STD_DIR
#define RL_STD_DIR ""
#endif
BI(h_std_root) {
    const char *env = getenv("RELIC_STD");
    const char *r = env && *env ? env : RL_STD_DIR;
    size_t n = strlen(r);
    UNUSED;
    (void)cx; (void)a;
    if (n && r[n - 1] != '/' && r[n - 1] != '\\') {
        char *t = (char *)malloc(n + 2);
        if (!t) abort();
        memcpy(t, r, n);
        t[n] = '/';
        *out = rl_str_new(t, n + 1);
        free(t);
    } else {
        *out = rl_str_new(r, n);
    }
    return RL_OK;
}

BI(h_names);
BI(h_call);

static const rl_builtin_def g_host[] = {
    {"host_read_file", 1, h_read_file}, {"host_write_file", 3, h_write_file},
    {"host_file_exists", 1, h_file_exists}, {"host_clock", 0, h_clock}, {"host_now", 0, h_now},
    {"host_floor", 1, h_floor}, {"host_sqrt", 1, h_sqrt}, {"host_sin", 1, h_sin}, {"host_cos", 1, h_cos},
    {"host_exp", 1, h_exp}, {"host_log", 1, h_log}, {"host_pow", 2, h_pow},
    {"host_list_dir", 1, h_list_dir}, {"host_is_dir", 1, h_is_dir}, {"host_make_dir", 1, h_make_dir},
    {"host_remove", 1, h_remove}, {"host_rename", 2, h_rename}, {"host_file_size", 1, h_file_size},
    {"host_cwd", 0, h_cwd}, {"host_env", 1, h_env}, {"host_read_line", 1, h_read_line}, {"host_exit", 1, h_exit}, {"host_sleep", 1, h_sleep},
    {"host_same", 2, h_same}, {"host_find", 2, h_find}, {"host_sort", 1, h_sort}, {"host_std_root", 0, h_std_root}, {"host_names", 0, h_names}, {"host_call", 2, h_call},
    {NULL, 0, NULL}};

const rl_builtin_def *rl_host_defs(void) { return g_host; }

static int cmp_defs(const void *x, const void *y) {
    return strcmp((*(const rl_builtin_def *const *)x)->name, (*(const rl_builtin_def *const *)y)->name);
}

/* host_names(): [name, arity] for every host_* built-in, sorted. */
BI(h_names) {
    size_t i, n = 0, total = rl_builtin_count();
    const rl_builtin_def **defs = (const rl_builtin_def **)malloc((total ? total : 1) * sizeof *defs);
    UNUSED;
    (void)cx; (void)a;
    if (!defs) abort();
    for (i = 0; i < total; ++i)
        if (strncmp(rl_builtin_at(i)->name, "host_", 5) == 0) defs[n++] = rl_builtin_at(i);
    qsort(defs, n, sizeof *defs, cmp_defs);
    *out = rl_list_new(n);
    for (i = 0; i < n; ++i) {
        rl_value pair = rl_list_new(2), s = rl_str_c(defs[i]->name);
        rl_list_push(L(pair), s);
        rl_list_push(L(pair), rl_int(defs[i]->arity));
        rl_list_push(L(*out), pair);
        rl_release(s);
        rl_release(pair);
    }
    free(defs);
    return RL_OK;
}

/* host_call(name, args): calls a host_* built-in with a list of arguments. */
BI(h_call) {
    size_t i, total = rl_builtin_count();
    const rl_builtin_def *d = NULL;
    UNUSED;
    if (rl_arg_str(cx, a[0], "name")) return RL_ERR;
    for (i = 0; i < total; ++i)
        if (strcmp(rl_builtin_at(i)->name, S(a[0])->data) == 0) d = rl_builtin_at(i);
    if (strncmp(S(a[0])->data, "host_", 5) != 0 || !d) return rl_fail(cx, "no host built-in %s", S(a[0])->data);
    if (a[1].k != RL_LIST) return rl_fail(cx, "first argument must be a list, got %s", rl_type_name(a[1]));
    if (d->arity >= 0 && (size_t)d->arity != L(a[1])->n)
        return rl_fail(cx, "%s() takes %d arguments", d->name, d->arity);
    return d->code(cx, self, (int)L(a[1])->n, L(a[1])->items, out);
}
