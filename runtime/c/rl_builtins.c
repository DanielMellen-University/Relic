/* rl_builtins.c: the language built-ins (print, len, str, num, ...).
 * Behavior and messages follow src/builtins/builtins.cpp. A built-in sets
 * its message without the "name(): " prefix; rl_call adds it. */
#include "relic.h"
#include "rl_internal.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define S(v) ((rl_str *)(v).u.o)
#define L(v) ((rl_list *)(v).u.o)
#define M(v) ((rl_map *)(v).u.o)
#define BI(name) static int name(rl_ctx *cx, rl_fun *self, int argc, const rl_value *a, rl_value *out)
#define UNUSED (void)self; (void)argc

int rl_arg_num(rl_ctx *cx, rl_value v, const char *what, double *d) {
    *d = 0;
    if (v.k == RL_INT) { *d = (double)v.u.i; return RL_OK; }
    if (v.k == RL_FLOAT) { *d = v.u.f; return RL_OK; }
    return rl_fail(cx, "%s must be a number, got %s", what, rl_type_name(v));
}
int rl_arg_whole(rl_ctx *cx, rl_value v, const char *what, long long *r) {
    double d = 0;
    *r = 0;
    if (v.k == RL_INT) { *r = v.u.i; return RL_OK; }
    if (rl_arg_num(cx, v, what, &d)) return RL_ERR;
    if (d != floor(d)) return rl_fail(cx, "%s must be a whole number", what);
    *r = (long long)d;
    return RL_OK;
}
int rl_arg_str(rl_ctx *cx, rl_value v, const char *what) {
    if (v.k != RL_STR) return rl_fail(cx, "%s must be a string, got %s", what, rl_type_name(v));
    return RL_OK;
}
static int list_arg(rl_ctx *cx, rl_value v) {
    if (v.k != RL_LIST) return rl_fail(cx, "first argument must be a list, got %s", rl_type_name(v));
    return RL_OK;
}

static void write_all(FILE *to, int argc, const rl_value *a) {
    int i;
    for (i = 0; i < argc; ++i) {
        rl_value s = rl_show(a[i]);
        if (i) fputc(' ', to);
        fwrite(S(s)->data, 1, S(s)->len, to);
        rl_release(s);
    }
    fputc('\n', to);
}

BI(b_print) { UNUSED; write_all(cx->out, argc, a); *out = rl_none(); return RL_OK; }
BI(b_eprint) { UNUSED; fflush(cx->out); write_all(stderr, argc, a); *out = rl_none(); return RL_OK; }

BI(b_len) {
    UNUSED;
    if (a[0].k == RL_STR) *out = rl_int((long long)rl_str_chars(S(a[0])));
    else if (a[0].k == RL_LIST) *out = rl_int((long long)L(a[0])->n);
    else if (a[0].k == RL_MAP) *out = rl_int((long long)M(a[0])->n);
    else return rl_fail(cx, "cannot get the length of %s", rl_a_type(a[0]));
    return RL_OK;
}

BI(b_append) {
    UNUSED;
    if (list_arg(cx, a[0])) return RL_ERR;
    rl_list_push(L(a[0]), a[1]);
    *out = rl_none();
    return RL_OK;
}

BI(b_str) { UNUSED; (void)cx; *out = rl_show(a[0]); return RL_OK; }

BI(b_keys) {
    size_t i;
    UNUSED;
    if (a[0].k != RL_MAP) return rl_fail(cx, "argument must be a map, got %s", rl_type_name(a[0]));
    *out = rl_list_new(M(a[0])->n);
    for (i = 0; i < M(a[0])->n; ++i) rl_list_push(L(*out), rl_obj_value(&M(a[0])->keys[i]->h));
    return RL_OK;
}

/* fail(message): the message is raised exactly as written. */
BI(b_fail) {
    UNUSED;
    *out = rl_none();
    if (a[0].k != RL_STR) return rl_fail(cx, "fail(): message must be a string, got %s", rl_type_name(a[0]));
    return rl_fail_str(cx, S(a[0]));
}

BI(b_gc) { UNUSED; (void)cx; (void)a; *out = rl_int((long long)rl_gc_collect()); return RL_OK; }
BI(b_type) { UNUSED; (void)cx; *out = rl_str_c(rl_type_name(a[0])); return RL_OK; }

static size_t clampi(long long i, size_t n) { return i < 0 ? 0 : ((unsigned long long)i > n ? n : (size_t)i); }
static size_t offset_of(const rl_str *s, size_t index) {
    size_t i = 0, n = 0;
    while (i < s->len && n < index) { i += rl_utf8_char_size(s->data, s->len, i); ++n; }
    return i;
}

BI(b_slice) {
    long long b = 0, e = 0;
    UNUSED;
    if (rl_arg_whole(cx, a[1], "start", &b) || rl_arg_whole(cx, a[2], "end", &e)) return RL_ERR;
    if (a[0].k == RL_STR) {
        rl_str *s = S(a[0]);
        size_t n = rl_str_chars(s), x = clampi(b, n), y = clampi(e, n), bx, by;
        if (y <= x) { *out = rl_str_new("", 0); return RL_OK; }
        bx = offset_of(s, x);
        by = offset_of(s, y);
        *out = rl_str_new(s->data + bx, by - bx);
        return RL_OK;
    }
    if (a[0].k == RL_LIST) {
        rl_list *l = L(a[0]);
        size_t x = clampi(b, l->n), y = clampi(e, l->n), i;
        *out = rl_list_new(y > x ? y - x : 0);
        for (i = x; i < y; ++i) rl_list_push(L(*out), l->items[i]);
        return RL_OK;
    }
    return rl_fail(cx, "cannot slice %s", rl_a_type(a[0]));
}

BI(b_ord) {
    rl_str *s;
    size_t n, i;
    long cp;
    UNUSED;
    if (rl_arg_str(cx, a[0], "argument")) return RL_ERR;
    s = S(a[0]);
    if (s->len == 0 || (n = rl_utf8_char_size(s->data, s->len, 0)) != s->len)
        return rl_fail(cx, "needs a one-character string");
    cp = (unsigned char)s->data[0];
    if (n > 1) {
        cp &= 0x7F >> n;
        for (i = 1; i < n; ++i) cp = (cp << 6) | ((unsigned char)s->data[i] & 0x3F);
    }
    *out = rl_int(cp);
    return RL_OK;
}

BI(b_chr) {
    long long n = 0;
    char b[4];
    size_t len;
    UNUSED;
    if (rl_arg_whole(cx, a[0], "argument", &n)) return RL_ERR;
    if (n < 0 || n > 0x10FFFF || (n >= 0xD800 && n <= 0xDFFF))
        return rl_fail(cx, "code must be 0 to 1114111 and not 55296 to 57343 (surrogates)");
    if (n < 0x80) { b[0] = (char)n; len = 1; }
    else if (n < 0x800) { b[0] = (char)(0xC0 | (n >> 6)); b[1] = (char)(0x80 | (n & 0x3F)); len = 2; }
    else if (n < 0x10000) {
        b[0] = (char)(0xE0 | (n >> 12)); b[1] = (char)(0x80 | ((n >> 6) & 0x3F)); b[2] = (char)(0x80 | (n & 0x3F)); len = 3;
    } else {
        b[0] = (char)(0xF0 | (n >> 18)); b[1] = (char)(0x80 | ((n >> 12) & 0x3F));
        b[2] = (char)(0x80 | ((n >> 6) & 0x3F)); b[3] = (char)(0x80 | (n & 0x3F)); len = 4;
    }
    *out = rl_str_new(b, len);
    return RL_OK;
}

BI(b_num) {
    UNUSED;
    if (a[0].k == RL_INT || a[0].k == RL_FLOAT) { *out = a[0]; return RL_OK; }
    if (rl_arg_str(cx, a[0], "argument")) return RL_ERR;
    return rl_parse_num(cx, S(a[0])->data, S(a[0])->len, out);
}

BI(b_int) {
    rl_value v = a[0];
    double d;
    UNUSED;
    if (v.k == RL_STR && rl_parse_num(cx, S(v)->data, S(v)->len, &v)) return RL_ERR;
    if (v.k == RL_INT) { *out = v; return RL_OK; }
    if (v.k != RL_FLOAT) return rl_fail(cx, "cannot turn %s into an int", rl_a_type(v));
    d = v.u.f < 0 ? ceil(v.u.f) : floor(v.u.f);
    if (d != d || d >= 9.2233720368547758e18 || d < -9.2233720368547758e18)
        return rl_fail(cx, "number is out of the int range");
    *out = rl_int((long long)d);
    return RL_OK;
}

BI(b_float) {
    rl_value v = a[0];
    UNUSED;
    if (v.k == RL_STR && rl_parse_num(cx, S(v)->data, S(v)->len, &v)) return RL_ERR;
    if (v.k == RL_INT) *out = rl_float((double)v.u.i);
    else if (v.k == RL_FLOAT) *out = v;
    else return rl_fail(cx, "cannot turn %s into a float", rl_a_type(v));
    return RL_OK;
}

BI(b_pop) {
    rl_list *l;
    UNUSED;
    if (list_arg(cx, a[0])) return RL_ERR;
    l = L(a[0]);
    if (l->n == 0) return rl_fail(cx, "list is empty");
    *out = l->items[--l->n];   /* the list's reference moves to the caller */
    return RL_OK;
}

BI(b_insert) {
    rl_list *l;
    long long i = 0;
    UNUSED;
    if (list_arg(cx, a[0]) || rl_arg_whole(cx, a[1], "index", &i)) return RL_ERR;
    l = L(a[0]);
    if (i < 0 || (unsigned long long)i > l->n) return rl_fail(cx, "index %lld is out of range", i);
    rl_list_push(l, a[2]);
    memmove(l->items + i + 1, l->items + i, (l->n - 1 - (size_t)i) * sizeof *l->items);
    l->items[i] = a[2];
    *out = rl_none();
    return RL_OK;
}

BI(b_remove) {
    UNUSED;
    if (a[0].k == RL_LIST) {
        rl_list *l = L(a[0]);
        long long i = 0;
        if (rl_arg_whole(cx, a[1], "index", &i)) return RL_ERR;
        if (i < 0 || (unsigned long long)i >= l->n) return rl_fail(cx, "index %lld is out of range", i);
        *out = l->items[i];
        memmove(l->items + i, l->items + i + 1, (l->n - 1 - (size_t)i) * sizeof *l->items);
        --l->n;
        return RL_OK;
    }
    if (a[0].k == RL_MAP) {
        if (a[1].k != RL_STR) return rl_fail(cx, "map keys must be strings");
        if (!rl_map_remove(M(a[0]), S(a[1])->data, S(a[1])->len, out))
            return rl_fail(cx, "map has no key '%s'", S(a[1])->data);
        return RL_OK;
    }
    return rl_fail(cx, "cannot remove from %s", rl_a_type(a[0]));
}

BI(b_args) {
    int i;
    UNUSED;
    (void)a;
    *out = rl_list_new((size_t)cx->argc);
    for (i = 0; i < cx->argc; ++i) {
        rl_value s = rl_str_c(cx->argv[i]);
        rl_list_push(L(*out), s);
        rl_release(s);
    }
    return RL_OK;
}

static const rl_builtin_def g_core[] = {
    {"print", -1, b_print}, {"eprint", -1, b_eprint}, {"len", 1, b_len}, {"append", 2, b_append},
    {"str", 1, b_str}, {"keys", 1, b_keys}, {"fail", 1, b_fail}, {"gc", 0, b_gc}, {"type", 1, b_type},
    {"slice", 3, b_slice}, {"ord", 1, b_ord}, {"chr", 1, b_chr}, {"num", 1, b_num}, {"int", 1, b_int},
    {"float", 1, b_float}, {"pop", 1, b_pop}, {"insert", 3, b_insert}, {"remove", 2, b_remove},
    {"args", 0, b_args}, {"host_args", 0, b_args},
    {NULL, 0, NULL}};

/* Built-ins are made once and shared, so `print == print` holds. */
typedef struct { const rl_builtin_def *def; rl_value fun; } cached;
static cached *g_cache = NULL;
static size_t g_cache_n = 0;

static void fill_cache(void) {
    const rl_builtin_def *tables[3];
    size_t t, i, n = 0, k = 0;
    tables[0] = g_core;
    tables[1] = rl_host_defs();
    tables[2] = rl_gfx_defs();
    for (t = 0; t < 3; ++t)
        for (i = 0; tables[t][i].name; ++i) ++n;
    g_cache = (cached *)malloc(n * sizeof *g_cache);
    if (!g_cache) abort();
    for (t = 0; t < 3; ++t)
        for (i = 0; tables[t][i].name; ++i) {
            g_cache[k].def = &tables[t][i];
            g_cache[k].fun = rl_none();
            ++k;
        }
    g_cache_n = n;
}

int rl_builtin(const char *name, rl_value *out) {
    size_t i;
    if (!g_cache) fill_cache();
    for (i = 0; i < g_cache_n; ++i)
        if (strcmp(g_cache[i].def->name, name) == 0) {
            if (g_cache[i].fun.k == RL_NONE) {
                g_cache[i].fun = rl_fun_new(name, g_cache[i].def->code, g_cache[i].def->arity, NULL);
                ((rl_fun *)g_cache[i].fun.u.o)->builtin = 1;
            }
            *out = rl_retain(g_cache[i].fun);
            return 1;
        }
    *out = rl_none();
    return 0;
}

size_t rl_builtin_count(void) {
    if (!g_cache) fill_cache();
    return g_cache_n;
}
const rl_builtin_def *rl_builtin_at(size_t i) {
    if (!g_cache) fill_cache();
    return i < g_cache_n ? g_cache[i].def : NULL;
}

void rl_builtins_release(void) {
    size_t i;
    for (i = 0; i < g_cache_n; ++i) rl_release(g_cache[i].fun);
    free(g_cache);
    g_cache = NULL;
    g_cache_n = 0;
}
