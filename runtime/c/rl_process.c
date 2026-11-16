/* rl_process.c: host_program and host_run.
 * A program and its arguments are a list, not a shell line. stdout and stderr
 * come back as bytes, NUL included. The messages match src/builtins/process.cpp. */
#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif
#include "relic.h"
#include "rl_internal.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <fcntl.h>
#include <poll.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
extern char **environ;
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
#endif

#define S(v) ((rl_str *)(v).u.o)
#define L(v) ((rl_list *)(v).u.o)
#define M(v) ((rl_map *)(v).u.o)

typedef struct { char *data; size_t n, cap; } pbuf;

static void pbuf_add(pbuf *b, const char *p, size_t n) {
    if (b->n + n + 1 > b->cap) {
        size_t c = b->cap ? b->cap : 4096;
        while (b->n + n + 1 > c) c *= 2;
        b->data = (char *)realloc(b->data, c);
        if (!b->data) abort();
        b->cap = c;
    }
    if (n && p) memcpy(b->data + b->n, p, n);
    b->n += n;
    if (b->data) b->data[b->n] = 0;
}

static void pbuf_free(pbuf *b) { free(b->data); b->data = NULL; b->n = b->cap = 0; }

static int has_nul(const rl_str *s) { return memchr(s->data, 0, s->len) != NULL; }

#ifdef _WIN32
static wchar_t *to_wide(const char *s, size_t n, int *ok) {
    int w = n ? MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s, (int)n, NULL, 0) : 0;
    wchar_t *out;
    *ok = 1;
    if (n && w <= 0) { *ok = 0; return NULL; }
    out = (wchar_t *)malloc(((size_t)w + 1) * sizeof(wchar_t));
    if (!out) abort();
    if (w > 0) MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s, (int)n, out, w);
    out[w] = 0;
    return out;
}

static char *from_wide(const wchar_t *w) {
    int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, NULL, 0, NULL, NULL);
    char *s = (char *)malloc(n > 0 ? (size_t)n : 1);
    if (!s) abort();
    if (n <= 0) s[0] = 0;
    else WideCharToMultiByte(CP_UTF8, 0, w, -1, s, n, NULL, NULL);
    return s;
}

static void quote_arg(pbuf *b, const char *s, size_t n) {
    size_t i, bs;
    int need = n == 0;
    for (i = 0; i < n && !need; ++i) {
        char c = s[i];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '"') need = 1;
    }
    if (!need) { pbuf_add(b, s, n); return; }
    pbuf_add(b, "\"", 1);
    bs = 0;
    for (i = 0; i < n; ++i) {
        if (s[i] == '\\') { ++bs; continue; }
        if (s[i] == '"') {
            size_t k;
            for (k = 0; k < bs * 2 + 1; ++k) pbuf_add(b, "\\", 1);
            pbuf_add(b, "\"", 1);
            bs = 0;
            continue;
        }
        while (bs) { pbuf_add(b, "\\", 1); --bs; }
        pbuf_add(b, &s[i], 1);
    }
    while (bs) { pbuf_add(b, "\\\\", 2); --bs; }
    pbuf_add(b, "\"", 1);
}

static int key_eq_w(const wchar_t *entry, const wchar_t *key, size_t klen) {
    size_t i;
    if (entry[0] == L'=') return 0;
    for (i = 0; i < klen; ++i) {
        wchar_t a = entry[i], b = key[i];
        if (!a) return 0;
        if (a >= L'A' && a <= L'Z') a = (wchar_t)(a + 32);
        if (b >= L'A' && b <= L'Z') b = (wchar_t)(b + 32);
        if (a != b) return 0;
    }
    return entry[klen] == L'=';
}
#endif

static int fail_run(rl_ctx *cx, const char *name) {
    if (name && name[0]) return rl_fail(cx, "cannot run %s", name);
    return rl_fail(cx, "cannot run");
}

int rl_bi_program(rl_ctx *cx, rl_fun *self, int argc, const rl_value *a, rl_value *out) {
    (void)self; (void)argc; (void)a;
#ifdef _WIN32
    wchar_t w[32768];
    DWORD n = GetModuleFileNameW(NULL, w, 32768);
    char *s;
    if (n == 0 || n >= 32768) return rl_fail(cx, "cannot find this program");
    s = from_wide(w);
    *out = rl_str_new(s, strlen(s));
    free(s);
    return RL_OK;
#elif defined(__APPLE__)
    char small[4096], *buf;
    uint32_t size = (uint32_t)sizeof small;
    if (_NSGetExecutablePath(small, &size) == 0) {
        *out = rl_str_new(small, strlen(small));
        return RL_OK;
    }
    buf = (char *)malloc(size ? size : 1);
    if (!buf) abort();
    if (_NSGetExecutablePath(buf, &size) != 0) { free(buf); return rl_fail(cx, "cannot find this program"); }
    *out = rl_str_new(buf, strlen(buf));
    free(buf);
    return RL_OK;
#else
    size_t cap = 256;
    char *buf = NULL;
    for (;;) {
        ssize_t n;
        buf = (char *)realloc(buf, cap);
        if (!buf) abort();
        n = readlink("/proc/self/exe", buf, cap);
        if (n < 0) { free(buf); return rl_fail(cx, "cannot find this program"); }
        if ((size_t)n < cap) { buf[n] = 0; *out = rl_str_new(buf, (size_t)n); free(buf); return RL_OK; }
        cap *= 2;
    }
#endif
}

/* Checks the four arguments. On success *cwd is "" or the folder, *in_bytes
 * is the stdin bytes, and *env is the map. */
static int check_args(rl_ctx *cx, const rl_value *a, const char **prog, const char **cwd, const char **in_bytes, size_t *in_len, rl_map **env) {
    rl_list *argv;
    rl_map *em;
    size_t i;
    if (a[0].k != RL_LIST) return rl_fail(cx, "argv must be a list, got %s", rl_type_name(a[0]));
    argv = L(a[0]);
    if (argv->n == 0) return rl_fail(cx, "argv must not be empty");
    for (i = 0; i < argv->n; ++i) {
        if (argv->items[i].k != RL_STR)
            return rl_fail(cx, "argv item %d must be a string, got %s", (int)i, rl_type_name(argv->items[i]));
        if (has_nul(S(argv->items[i])))
            return rl_fail(cx, "argv item %d must not contain a NUL byte", (int)i);
    }
    if (S(argv->items[0])->len == 0) return rl_fail(cx, "argv item 0 must not be empty");
    if (rl_arg_str(cx, a[1], "cwd")) return RL_ERR;
    if (has_nul(S(a[1]))) return rl_fail(cx, "cwd must not contain a NUL byte");
    if (rl_arg_str(cx, a[2], "stdin")) return RL_ERR;
    if (a[3].k != RL_MAP) return rl_fail(cx, "env must be a map, got %s", rl_type_name(a[3]));
    em = M(a[3]);
    for (i = 0; i < em->used; ++i) {
        rl_str *k;
        if (!em->keys[i]) continue;
        k = em->keys[i];
        if (k->len == 0) return rl_fail(cx, "env key must not be empty");
        if (has_nul(k)) return rl_fail(cx, "env key must not contain a NUL byte");
        if (memchr(k->data, '=', k->len)) return rl_fail(cx, "env key must not contain '='");
        if (em->vals[i].k != RL_STR)
            return rl_fail(cx, "env value of %s must be a string, got %s", k->data, rl_type_name(em->vals[i]));
        if (has_nul(S(em->vals[i])))
            return rl_fail(cx, "env value of %s must not contain a NUL byte", k->data);
    }
    *prog = S(argv->items[0])->data;
    *cwd = S(a[1])->data;
    *in_bytes = S(a[2])->data;
    *in_len = S(a[2])->len;
    *env = em;
    return RL_OK;
}

#ifdef _WIN32
static int dir_ok(const char *cwd) {
    int ok = 1;
    wchar_t *w = to_wide(cwd, strlen(cwd), &ok);
    DWORD a;
    if (!ok || !w) { free(w); return 0; }
    a = GetFileAttributesW(w);
    free(w);
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

static wchar_t *env_block(rl_map *extra) {
    wchar_t *src = GetEnvironmentStringsW();
    size_t n = 0, cap = 8, i, total = 1;
    wchar_t **ents;
    wchar_t *block, *p;
    if (!src) return NULL;
    ents = (wchar_t **)malloc(cap * sizeof *ents);
    if (!ents) abort();
    for (p = src; *p; ) {
        size_t len = wcslen(p);
        wchar_t *copy = (wchar_t *)malloc((len + 1) * sizeof(wchar_t));
        if (!copy) abort();
        memcpy(copy, p, (len + 1) * sizeof(wchar_t));
        if (n == cap) {
            cap *= 2;
            ents = (wchar_t **)realloc(ents, cap * sizeof *ents);
            if (!ents) abort();
        }
        ents[n++] = copy;
        total += len + 1;
        p += len + 1;
    }
    FreeEnvironmentStringsW(src);
    for (i = 0; i < extra->used; ++i) {
        int conv = 1, found = 0;
        wchar_t *kw, *vw, *row;
        size_t klen, vlen, j;
        if (!extra->keys[i]) continue;
        kw = to_wide(extra->keys[i]->data, extra->keys[i]->len, &conv);
        vw = conv ? to_wide(S(extra->vals[i])->data, S(extra->vals[i])->len, &conv) : NULL;
        if (!conv || !kw || !vw) {
            free(kw); free(vw);
            for (j = 0; j < n; ++j) free(ents[j]);
            free(ents);
            return NULL;
        }
        klen = wcslen(kw);
        vlen = wcslen(vw);
        row = (wchar_t *)malloc((klen + 1 + vlen + 1) * sizeof(wchar_t));
        if (!row) abort();
        memcpy(row, kw, klen * sizeof(wchar_t));
        row[klen] = L'=';
        memcpy(row + klen + 1, vw, (vlen + 1) * sizeof(wchar_t));
        free(kw); free(vw);
        for (j = 0; j < n; ++j) {
            if (key_eq_w(ents[j], row, klen)) { free(ents[j]); ents[j] = row; found = 1; break; }
        }
        if (!found) {
            if (n == cap) {
                cap *= 2;
                ents = (wchar_t **)realloc(ents, cap * sizeof *ents);
                if (!ents) abort();
            }
            ents[n++] = row;
            total += klen + 1 + vlen + 1;
        } else {
            /* replaced: the new row's length may differ; recompute below */
        }
    }
    total = 1;
    for (i = 0; i < n; ++i) total += wcslen(ents[i]) + 1;
    block = (wchar_t *)malloc(total * sizeof(wchar_t));
    if (!block) abort();
    p = block;
    for (i = 0; i < n; ++i) {
        size_t len = wcslen(ents[i]);
        memcpy(p, ents[i], (len + 1) * sizeof(wchar_t));
        p += len + 1;
        free(ents[i]);
    }
    *p = 0;
    free(ents);
    return block;
}

static HANDLE stdin_handle(const char *bytes, size_t n, int *err) {
    SECURITY_ATTRIBUTES sa;
    *err = 0;
    memset(&sa, 0, sizeof sa);
    sa.nLength = sizeof sa;
    sa.bInheritHandle = TRUE;
    if (n == 0) {
        HANDLE rd = NULL, wr = NULL;
        if (!CreatePipe(&rd, &wr, &sa, 0)) { *err = 1; return NULL; }
        SetHandleInformation(wr, HANDLE_FLAG_INHERIT, 0);
        CloseHandle(wr);
        return rd;
    } else {
        wchar_t dir[MAX_PATH], path[MAX_PATH];
        HANDLE h;
        size_t off = 0;
        if (!GetTempPathW(MAX_PATH, dir) || !GetTempFileNameW(dir, L"rlr", 0, path)) { *err = 1; return NULL; }
        h = CreateFileW(path, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_DELETE, &sa, OPEN_EXISTING,
                        FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, NULL);
        if (h == INVALID_HANDLE_VALUE) { DeleteFileW(path); *err = 1; return NULL; }
        while (off < n) {
            DWORD wrote = 0;
            DWORD chunk = (DWORD)((n - off) > 1 << 20 ? 1 << 20 : (n - off));
            if (!WriteFile(h, bytes + off, chunk, &wrote, NULL) || wrote == 0) {
                CloseHandle(h);
                *err = 1;
                return NULL;
            }
            off += wrote;
        }
        if (SetFilePointer(h, 0, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER) { CloseHandle(h); *err = 1; return NULL; }
        return h;
    }
}

static int drain_pipe(HANDLE h, pbuf *b, int *open) {
    DWORD avail = 0, got = 0;
    int any = 0;
    if (!*open) return 0;
    if (!PeekNamedPipe(h, NULL, 0, NULL, &avail, NULL)) { *open = 0; return 0; }
    while (avail) {
        char tmp[4096];
        DWORD chunk = avail > sizeof tmp ? (DWORD)sizeof tmp : avail;
        if (!ReadFile(h, tmp, chunk, &got, NULL) || got == 0) { *open = 0; break; }
        pbuf_add(b, tmp, got);
        any = 1;
        avail -= got;
    }
    return any;
}

static int spawn_win(rl_ctx *cx, rl_list *argv, const char *cwd, const char *in_bytes, size_t in_len, rl_map *env, rl_value *out) {
    SECURITY_ATTRIBUTES sa;
    HANDLE in_h = NULL, out_r = NULL, out_w = NULL, err_r = NULL, err_w = NULL;
    wchar_t *app = NULL, *cline = NULL, *cdir = NULL, *block = NULL;
    pbuf cmd = {0, 0, 0}, obuf = {0, 0, 0}, ebuf = {0, 0, 0};
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    size_t i;
    int has_sep, conv = 1, in_err = 0, out_open = 1, err_open = 1;
    const char *prog = S(argv->items[0])->data;
    DWORD code = 1;
    memset(&sa, 0, sizeof sa);
    sa.nLength = sizeof sa;
    sa.bInheritHandle = TRUE;
    memset(&pi, 0, sizeof pi);
    for (i = 0; i < argv->n; ++i) {
        if (i) pbuf_add(&cmd, " ", 1);
        quote_arg(&cmd, S(argv->items[i])->data, S(argv->items[i])->len);
    }
    if (cmd.n > 32766) { pbuf_free(&cmd); return fail_run(cx, prog); }
    cline = to_wide(cmd.data ? cmd.data : "", cmd.n, &conv);
    pbuf_free(&cmd);
    if (!conv || !cline) { free(cline); return fail_run(cx, prog); }
    has_sep = strchr(prog, '/') || strchr(prog, '\\');
    if (has_sep) {
        app = to_wide(prog, strlen(prog), &conv);
        if (!conv || !app) { free(app); free(cline); return fail_run(cx, prog); }
    }
    if (cwd[0]) {
        cdir = to_wide(cwd, strlen(cwd), &conv);
        if (!conv || !cdir) { free(app); free(cline); free(cdir); return fail_run(cx, prog); }
    }
    if (env->count) {
        block = env_block(env);
        if (!block) { free(app); free(cline); free(cdir); return fail_run(cx, prog); }
    }
    in_h = stdin_handle(in_bytes, in_len, &in_err);
    if (in_err || !in_h) goto fail;
    if (!CreatePipe(&out_r, &out_w, &sa, 0) || !CreatePipe(&err_r, &err_w, &sa, 0)) goto fail;
    SetHandleInformation(out_r, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(err_r, HANDLE_FLAG_INHERIT, 0);
    memset(&si, 0, sizeof si);
    si.cb = sizeof si;
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = in_h;
    si.hStdOutput = out_w;
    si.hStdError = err_w;
    if (!CreateProcessW(app, cline, NULL, NULL, TRUE, CREATE_UNICODE_ENVIRONMENT, block, cdir, &si, &pi)) goto fail;
    CloseHandle(in_h); in_h = NULL;
    CloseHandle(out_w); out_w = NULL;
    CloseHandle(err_w); err_w = NULL;
    for (;;) {
        int got = 0;
        if (out_open) got |= drain_pipe(out_r, &obuf, &out_open);
        if (err_open) got |= drain_pipe(err_r, &ebuf, &err_open);
        if (!out_open && !err_open) break;
        if (WaitForSingleObject(pi.hProcess, got ? 0 : 15) == WAIT_OBJECT_0 && !got) {
            int more = 0;
            if (out_open) more |= drain_pipe(out_r, &obuf, &out_open);
            if (err_open) more |= drain_pipe(err_r, &ebuf, &err_open);
            if (!more) break;
        }
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    if (!GetExitCodeProcess(pi.hProcess, &code)) code = 1;
    {
        rl_value m = rl_map_new(), k, vs, os, es;
        long long signed_code = (long long)(int)code;
        vs = rl_int(signed_code);
        os = rl_str_new(obuf.data ? obuf.data : "", obuf.n);
        es = rl_str_new(ebuf.data ? ebuf.data : "", ebuf.n);
        k = rl_str_c("code"); rl_map_set(M(m), S(k), vs); rl_release(k);
        k = rl_str_c("out"); rl_map_set(M(m), S(k), os); rl_release(k); rl_release(os);
        k = rl_str_c("err"); rl_map_set(M(m), S(k), es); rl_release(k); rl_release(es);
        *out = m;
    }
    CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
    if (out_r) CloseHandle(out_r);
    if (err_r) CloseHandle(err_r);
    free(app); free(cline); free(cdir); free(block);
    pbuf_free(&obuf); pbuf_free(&ebuf);
    return RL_OK;
fail:
    if (in_h) CloseHandle(in_h);
    if (out_r) CloseHandle(out_r);
    if (out_w) CloseHandle(out_w);
    if (err_r) CloseHandle(err_r);
    if (err_w) CloseHandle(err_w);
    if (pi.hProcess) { WaitForSingleObject(pi.hProcess, INFINITE); CloseHandle(pi.hProcess); CloseHandle(pi.hThread); }
    free(app); free(cline); free(cdir); free(block);
    pbuf_free(&obuf); pbuf_free(&ebuf);
    return fail_run(cx, prog);
}
#else
static int dir_ok(const char *cwd) {
    struct stat st;
    return stat(cwd, &st) == 0 && S_ISDIR(st.st_mode);
}

static int stdin_fd(const char *bytes, size_t n) {
    if (n == 0) {
        int p[2];
        if (pipe(p) != 0) return -1;
        close(p[1]);
        return p[0];
    }
    {
        char path[4096];
        const char *tmp = getenv("TMPDIR");
        int fd, nw;
        size_t off = 0;
        if (!tmp || !tmp[0]) tmp = "/tmp";
        nw = snprintf(path, sizeof path, "%s/relic-run-%ld-XXXXXX", tmp, (long)getpid());
        if (nw < 0 || nw >= (int)sizeof path) return -1;
        fd = mkstemp(path);
        if (fd < 0) return -1;
        while (off < n) {
            ssize_t w = write(fd, bytes + off, n - off);
            if (w < 0) { if (errno == EINTR) continue; close(fd); unlink(path); return -1; }
            if (w == 0) { close(fd); unlink(path); return -1; }
            off += (size_t)w;
        }
        if (lseek(fd, 0, SEEK_SET) < 0) { close(fd); unlink(path); return -1; }
        unlink(path);
        return fd;
    }
}

/* PATH search, then exec. Only returns if every exec fails. */
static void exec_argv(char **argv, char **envp) {
    const char *path = "/usr/bin:/bin";
    char **e;
    if (strchr(argv[0], '/')) { execve(argv[0], argv, envp); return; }
    for (e = envp; *e; ++e)
        if (strncmp(*e, "PATH=", 5) == 0) { path = *e + 5; break; }
    for (;;) {
        const char *colon = strchr(path, ':');
        size_t len = colon ? (size_t)(colon - path) : strlen(path);
        char full[4096];
        int nw = len == 0 ? snprintf(full, sizeof full, "%s", argv[0])
                          : snprintf(full, sizeof full, "%.*s/%s", (int)len, path, argv[0]);
        if (nw > 0 && nw < (int)sizeof full) {
            execve(full, argv, envp);
            if (errno != ENOENT && errno != ENOTDIR && errno != EACCES) return;
        }
        if (!colon) return;
        path = colon + 1;
        if (!*path) { execve(argv[0], argv, envp); return; }
    }
}

static char **merge_env(rl_map *extra, char ***owned, size_t *nowned) {
    size_t nenv = 0, i, j, used, no = 0;
    char **envp, **own;
    for (; environ[nenv]; ++nenv) {}
    envp = (char **)calloc(nenv + extra->count + 1, sizeof(char *));
    own = (char **)calloc(extra->count ? extra->count : 1, sizeof(char *));
    if (!envp || !own) abort();
    for (i = 0; i < nenv; ++i) envp[i] = environ[i];
    used = nenv;
    for (i = 0; i < extra->used; ++i) {
        rl_str *k, *v;
        char *s;
        size_t len;
        int found = 0;
        if (!extra->keys[i]) continue;
        k = extra->keys[i];
        v = S(extra->vals[i]);
        len = k->len + 1 + v->len;
        s = (char *)malloc(len + 1);
        if (!s) abort();
        memcpy(s, k->data, k->len);
        s[k->len] = '=';
        memcpy(s + k->len + 1, v->data, v->len);
        s[len] = 0;
        own[no++] = s;
        for (j = 0; j < used; ++j) {
            const char *eq = strchr(envp[j], '=');
            size_t kl = eq ? (size_t)(eq - envp[j]) : strlen(envp[j]);
            if (kl == k->len && memcmp(envp[j], k->data, k->len) == 0) { envp[j] = s; found = 1; break; }
        }
        if (!found) envp[used++] = s;
    }
    envp[used] = NULL;
    *owned = own;
    *nowned = no;
    return envp;
}

/* Reads both pipes until they close. Always closes both fds. */
static int pump(int out_fd, int err_fd, pbuf *out, pbuf *err) {
    int fl, rc = 0;
    fl = fcntl(out_fd, F_GETFL, 0);
    if (fl >= 0) fcntl(out_fd, F_SETFL, fl | O_NONBLOCK);
    fl = fcntl(err_fd, F_GETFL, 0);
    if (fl >= 0) fcntl(err_fd, F_SETFL, fl | O_NONBLOCK);
    for (;;) {
        struct pollfd pf[2];
        int np = 0, oi = -1, ei = -1, r;
        if (out_fd >= 0) { oi = np; pf[np].fd = out_fd; pf[np].events = POLLIN; ++np; }
        if (err_fd >= 0) { ei = np; pf[np].fd = err_fd; pf[np].events = POLLIN; ++np; }
        if (!np) break;
        r = poll(pf, (nfds_t)np, -1);
        if (r < 0) {
            if (errno == EINTR) continue;
            rc = -1;
            break;
        }
        if (oi >= 0 && (pf[oi].revents & (POLLIN | POLLHUP | POLLERR))) {
            char tmp[4096];
            ssize_t n = read(out_fd, tmp, sizeof tmp);
            if (n > 0) pbuf_add(out, tmp, (size_t)n);
            else if (!(n < 0 && (errno == EINTR || errno == EAGAIN))) { close(out_fd); out_fd = -1; }
        }
        if (ei >= 0 && (pf[ei].revents & (POLLIN | POLLHUP | POLLERR))) {
            char tmp[4096];
            ssize_t n = read(err_fd, tmp, sizeof tmp);
            if (n > 0) pbuf_add(err, tmp, (size_t)n);
            else if (!(n < 0 && (errno == EINTR || errno == EAGAIN))) { close(err_fd); err_fd = -1; }
        }
    }
    if (out_fd >= 0) close(out_fd);
    if (err_fd >= 0) close(err_fd);
    return rc;
}

static int spawn_posix(rl_ctx *cx, rl_list *argv, const char *cwd, const char *in_bytes, size_t in_len, rl_map *env, rl_value *out) {
    int op[2] = {-1, -1}, ep[2] = {-1, -1}, errp[2] = {-1, -1}, in_fd = -1;
    char **av = NULL, **envp = NULL, **owned = NULL;
    size_t i, nowned = 0;
    pid_t pid;
    pbuf obuf = {0, 0, 0}, ebuf = {0, 0, 0};
    const char *prog = S(argv->items[0])->data;
    int status = 0, code = 1, pumped;
    char why = 0;
    if (pipe(op) != 0 || pipe(ep) != 0 || pipe(errp) != 0) goto fail;
    fcntl(errp[1], F_SETFD, FD_CLOEXEC);
    in_fd = stdin_fd(in_bytes, in_len);
    if (in_fd < 0) goto fail;
    av = (char **)malloc((argv->n + 1) * sizeof(char *));
    if (!av) abort();
    for (i = 0; i < argv->n; ++i) av[i] = S(argv->items[i])->data;
    av[argv->n] = NULL;
    if (env->count) envp = merge_env(env, &owned, &nowned);
    else envp = environ;
    pid = fork();
    if (pid < 0) goto fail;
    if (pid == 0) {
        close(errp[0]); close(op[0]); close(ep[0]);
        if (in_fd != 0) { dup2(in_fd, 0); if (in_fd > 2) close(in_fd); }
        if (op[1] != 1) { dup2(op[1], 1); if (op[1] > 2) close(op[1]); }
        if (ep[1] != 2) { dup2(ep[1], 2); if (ep[1] > 2) close(ep[1]); }
        if (cwd[0] && chdir(cwd) != 0) { char c = 'c'; if (write(errp[1], &c, 1) < 0) {} _exit(127); }
        exec_argv(av, envp);
        { char c = 'e'; if (write(errp[1], &c, 1) < 0) {} _exit(127); }
    }
    close(errp[1]); errp[1] = -1;
    close(op[1]); op[1] = -1;
    close(ep[1]); ep[1] = -1;
    close(in_fd); in_fd = -1;
    pumped = pump(op[0], ep[0], &obuf, &ebuf);
    op[0] = ep[0] = -1; /* pump closes them */
    while (waitpid(pid, &status, 0) < 0) { if (errno != EINTR) break; }
    if (pumped != 0) {
        close(errp[0]);
        free(av);
        if (owned) { for (i = 0; i < nowned; ++i) free(owned[i]); free(owned); }
        if (envp != environ) free(envp);
        pbuf_free(&obuf); pbuf_free(&ebuf);
        return fail_run(cx, prog);
    }
    if (read(errp[0], &why, 1) == 1) {
        close(errp[0]);
        free(av);
        if (owned) { for (i = 0; i < nowned; ++i) free(owned[i]); free(owned); }
        if (envp != environ) free(envp);
        pbuf_free(&obuf); pbuf_free(&ebuf);
        if (why == 'c') return rl_fail(cx, "cannot use folder %s", cwd);
        return fail_run(cx, prog);
    }
    close(errp[0]); errp[0] = -1;
    if (WIFEXITED(status)) code = WEXITSTATUS(status);
    else if (WIFSIGNALED(status)) code = 128 + WTERMSIG(status);
    {
        rl_value m = rl_map_new(), k, vs, os, es;
        vs = rl_int(code);
        os = rl_str_new(obuf.data ? obuf.data : "", obuf.n);
        es = rl_str_new(ebuf.data ? ebuf.data : "", ebuf.n);
        k = rl_str_c("code"); rl_map_set(M(m), S(k), vs); rl_release(k);
        k = rl_str_c("out"); rl_map_set(M(m), S(k), os); rl_release(k); rl_release(os);
        k = rl_str_c("err"); rl_map_set(M(m), S(k), es); rl_release(k); rl_release(es);
        *out = m;
    }
    free(av);
    if (owned) { for (i = 0; i < nowned; ++i) free(owned[i]); free(owned); }
    if (envp != environ) free(envp);
    pbuf_free(&obuf); pbuf_free(&ebuf);
    return RL_OK;
fail:
    if (op[0] >= 0) close(op[0]);
    if (op[1] >= 0) close(op[1]);
    if (ep[0] >= 0) close(ep[0]);
    if (ep[1] >= 0) close(ep[1]);
    if (errp[0] >= 0) close(errp[0]);
    if (errp[1] >= 0) close(errp[1]);
    if (in_fd >= 0) close(in_fd);
    free(av);
    if (owned) { for (i = 0; i < nowned; ++i) free(owned[i]); free(owned); }
    if (envp && envp != environ) free(envp);
    pbuf_free(&obuf); pbuf_free(&ebuf);
    return fail_run(cx, prog);
}
#endif

int rl_bi_run(rl_ctx *cx, rl_fun *self, int argc, const rl_value *a, rl_value *out) {
    const char *prog = "", *cwd = "", *in_bytes = "";
    size_t in_len = 0;
    rl_map *env = NULL;
    (void)self; (void)argc;
    if (check_args(cx, a, &prog, &cwd, &in_bytes, &in_len, &env)) return RL_ERR;
    if (cwd[0] && !dir_ok(cwd)) return rl_fail(cx, "cannot use folder %s", cwd);
#ifdef _WIN32
    return spawn_win(cx, L(a[0]), cwd, in_bytes, in_len, env, out);
#else
    return spawn_posix(cx, L(a[0]), cwd, in_bytes, in_len, env, out);
#endif
}
