/* rl_core.c: objects, reference counts, the cycle collector, strings,
 * lists, maps, scopes, functions, modules, show/repr, equality, errors
 * and calls. See relic.h for the rules. */
#include "relic.h"
#include "rl_internal.h"

#include <math.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* allocation                                                           */

static void *xmalloc(size_t n) {
    void *p = malloc(n ? n : 1);
    if (!p) { fputs("relic: out of memory\n", stderr); abort(); }
    return p;
}
static void *xrealloc(void *p, size_t n) {
    void *q = realloc(p, n ? n : 1);
    if (!q) { fputs("relic: out of memory\n", stderr); abort(); }
    return q;
}

/* ------------------------------------------------------------------ */
/* tracked containers and counts                                        */

static rl_obj *g_head = NULL;
static size_t g_live = 0;
static size_t g_strings = 0;
static size_t g_threshold = 100000;
#define RL_MIN_THRESHOLD 100000

static void track(rl_obj *o, int kind) {
    o->kind = kind;
    o->rc = 1;
    o->gc_aux = 0;
    o->gc_prev = NULL;
    o->gc_next = g_head;
    if (g_head) g_head->gc_prev = o;
    g_head = o;
    ++g_live;
}
static void untrack(rl_obj *o) {
    if (o->gc_prev) o->gc_prev->gc_next = o->gc_next;
    else g_head = o->gc_next;
    if (o->gc_next) o->gc_next->gc_prev = o->gc_prev;
    --g_live;
}

size_t rl_live_objects(void) { return g_live; }
size_t rl_live_strings(void) { return g_strings; }

/* ------------------------------------------------------------------ */
/* values                                                               */

rl_value rl_none(void) { rl_value v; v.k = RL_NONE; v.u.i = 0; return v; }
rl_value rl_unset(void) { rl_value v; v.k = RL_UNSET; v.u.i = 0; return v; }
rl_value rl_bool(int b) { rl_value v; v.k = RL_BOOL; v.u.i = 0; v.u.b = b ? 1 : 0; return v; }
rl_value rl_int(long long i) { rl_value v; v.k = RL_INT; v.u.i = i; return v; }
rl_value rl_float(double f) { rl_value v; v.k = RL_FLOAT; v.u.f = f; return v; }
rl_value rl_obj_value(rl_obj *o) { rl_value v; v.k = o->kind; v.u.o = o; return v; }

static int is_obj(rl_value v) { return v.k >= RL_STR; }

rl_value rl_retain(rl_value v) {
    if (is_obj(v)) ++v.u.o->rc;
    return v;
}

/* Freeing never recurses: a dying container pushes children that also
 * die onto this stack, and the outermost release empties it. A list
 * nested a million deep frees in a loop. */
static rl_obj **g_dying = NULL;
static size_t g_dying_n = 0, g_dying_cap = 0;
static int g_draining = 0;

static void dying_push(rl_obj *o) {
    if (g_dying_n == g_dying_cap) {
        g_dying_cap = g_dying_cap ? g_dying_cap * 2 : 64;
        g_dying = (rl_obj **)xrealloc(g_dying, g_dying_cap * sizeof *g_dying);
    }
    g_dying[g_dying_n++] = o;
}

static void drop_obj(rl_obj *o);   /* rc already 0 */

static void release_obj(rl_obj *o) {
    if (!o || --o->rc > 0) return;
    if (o->kind == RL_STR) { free(o); --g_strings; return; }
    dying_push(o);
    if (g_draining) return;
    g_draining = 1;
    while (g_dying_n) drop_obj(g_dying[--g_dying_n]);
    g_draining = 0;
}

void rl_release(rl_value v) {
    if (is_obj(v)) release_obj(v.u.o);
}

static void release_values(rl_value *vs, size_t n) {
    size_t i;
    for (i = 0; i < n; ++i) rl_release(vs[i]);
}

/* Drops every reference o holds and empties it (o itself stays). */
static void clear_refs(rl_obj *o) {
    switch (o->kind) {
        case RL_LIST: {
            rl_list *l = (rl_list *)o;
            size_t n = l->n;
            l->n = 0;
            release_values(l->items, n);
            break;
        }
        case RL_MAP: {
            rl_map *m = (rl_map *)o;
            size_t i, n = m->n;
            m->n = 0;
            if (m->index) memset(m->index, 0, m->index_cap * sizeof *m->index);
            for (i = 0; i < n; ++i) { release_obj(&m->keys[i]->h); rl_release(m->vals[i]); }
            break;
        }
        case RL_SCOPE: {
            rl_scope *s = (rl_scope *)o;
            size_t i;
            rl_scope *p = s->parent;
            s->parent = NULL;
            for (i = 0; i < s->n; ++i) { rl_value v = s->slots[i]; s->slots[i] = rl_none(); rl_release(v); }
            if (p) release_obj(&p->h);
            break;
        }
        case RL_FUN: {
            rl_fun *f = (rl_fun *)o;
            rl_scope *e = f->env;
            f->env = NULL;
            if (e) release_obj(&e->h);
            break;
        }
        case RL_MODULE: {
            rl_module *m = (rl_module *)o;
            rl_map *n = m->names;
            rl_scope *s = m->scope;
            m->names = NULL;
            m->scope = NULL;
            if (n) release_obj(&n->h);
            if (s) release_obj(&s->h);
            break;
        }
        default: break;
    }
}

static void drop_obj(rl_obj *o) {
    clear_refs(o);
    untrack(o);
    switch (o->kind) {
        case RL_LIST: free(((rl_list *)o)->items); break;
        case RL_MAP: {
            rl_map *m = (rl_map *)o;
            free(m->keys); free(m->vals); free(m->index);
            break;
        }
        case RL_SCOPE: free(((rl_scope *)o)->slots); break;
        case RL_FUN: release_obj(&((rl_fun *)o)->name->h); break;
        case RL_MODULE: release_obj(&((rl_module *)o)->name->h); break;
        default: break;
    }
    free(o);
}

const char *rl_type_name(rl_value v) {
    switch (v.k) {
        case RL_NONE: return "none";
        case RL_BOOL: return "bool";
        case RL_INT: return "int";
        case RL_FLOAT: return "float";
        case RL_STR: return "string";
        case RL_LIST: return "list";
        case RL_MAP: return "map";
        case RL_MODULE: return "module";
        default: return "function";
    }
}

/* ------------------------------------------------------------------ */
/* cycle collector: the host's algorithm (src/interp/gc.cpp).           */
/* Count the references each container gets from other containers; one */
/* with more references than that is held from outside (a root). Mark   */
/* everything reachable from roots; clear the rest.                     */

typedef struct { rl_obj **p; size_t n, cap; } objvec;
static void ov_push(objvec *v, rl_obj *o) {
    if (v->n == v->cap) {
        v->cap = v->cap ? v->cap * 2 : 64;
        v->p = (rl_obj **)xrealloc(v->p, v->cap * sizeof *v->p);
    }
    v->p[v->n++] = o;
}

static void kid(objvec *out, rl_value v) {
    if (v.k >= RL_LIST) ov_push(out, v.u.o);
}
static void children(rl_obj *o, objvec *out) {
    size_t i;
    switch (o->kind) {
        case RL_LIST: { rl_list *l = (rl_list *)o; for (i = 0; i < l->n; ++i) kid(out, l->items[i]); break; }
        case RL_MAP: { rl_map *m = (rl_map *)o; for (i = 0; i < m->n; ++i) kid(out, m->vals[i]); break; }
        case RL_SCOPE: {
            rl_scope *s = (rl_scope *)o;
            for (i = 0; i < s->n; ++i) kid(out, s->slots[i]);
            if (s->parent) ov_push(out, &s->parent->h);
            break;
        }
        case RL_FUN: if (((rl_fun *)o)->env) ov_push(out, &((rl_fun *)o)->env->h); break;
        case RL_MODULE:
            if (((rl_module *)o)->names) ov_push(out, &((rl_module *)o)->names->h);
            if (((rl_module *)o)->scope) ov_push(out, &((rl_module *)o)->scope->h);
            break;
        default: break;
    }
}

size_t rl_gc_collect(void) {
    objvec all = {0, 0, 0}, kids = {0, 0, 0}, work = {0, 0, 0};
    rl_obj *o;
    size_t i, j, freed;
    for (o = g_head; o; o = o->gc_next) { o->gc_aux = o->rc; ov_push(&all, o); }
    for (i = 0; i < all.n; ++i) {
        kids.n = 0;
        children(all.p[i], &kids);
        for (j = 0; j < kids.n; ++j) --kids.p[j]->gc_aux;
    }
    /* gc_aux > 0: a root. Mark live as gc_aux = -1. */
    for (i = 0; i < all.n; ++i)
        if (all.p[i]->gc_aux > 0) { all.p[i]->gc_aux = -1; ov_push(&work, all.p[i]); }
    while (work.n) {
        rl_obj *w = work.p[--work.n];
        kids.n = 0;
        children(w, &kids);
        for (j = 0; j < kids.n; ++j)
            if (kids.p[j]->gc_aux != -1) { kids.p[j]->gc_aux = -1; ov_push(&work, kids.p[j]); }
    }
    freed = 0;
    for (i = 0; i < all.n; ++i)
        if (all.p[i]->gc_aux != -1) { ++all.p[i]->rc; all.p[freed++] = all.p[i]; }  /* hold the dead */
    for (i = 0; i < freed; ++i) clear_refs(all.p[i]);
    for (i = 0; i < freed; ++i) release_obj(all.p[i]);   /* the last references go here */
    free(all.p); free(kids.p); free(work.p);
    g_threshold = g_live * 2 > RL_MIN_THRESHOLD ? g_live * 2 : RL_MIN_THRESHOLD;
    return freed;
}

void rl_gc_maybe(void) {
    if (g_live > g_threshold) rl_gc_collect();
}

/* ------------------------------------------------------------------ */
/* strings                                                              */

rl_value rl_str_new(const char *bytes, size_t len) {
    rl_str *s = (rl_str *)xmalloc(sizeof(rl_str) + len);
    s->h.kind = RL_STR;
    s->h.rc = 1;
    s->h.gc_prev = s->h.gc_next = NULL;
    s->h.gc_aux = 0;
    s->len = len;
    s->chars = (size_t)-1;
    if (len) memcpy(s->data, bytes, len);
    s->data[len] = 0;
    ++g_strings;
    return rl_obj_value(&s->h);
}

rl_value rl_str_c(const char *s) { return rl_str_new(s, strlen(s)); }

/* Same rule as the host: a malformed byte counts as one character. */
size_t rl_utf8_char_size(const char *s, size_t len, size_t pos) {
    unsigned char c = (unsigned char)s[pos];
    size_t n = c < 0x80 ? 1 : (c >> 5) == 6 ? 2 : (c >> 4) == 14 ? 3 : (c >> 3) == 30 ? 4 : 1;
    size_t i;
    if (n == 1 || pos + n > len) return 1;
    for (i = 1; i < n; ++i)
        if (((unsigned char)s[pos + i] & 0xC0) != 0x80) return 1;
    return n;
}

size_t rl_str_chars(rl_str *s) {
    if (s->chars == (size_t)-1) {
        size_t i = 0, n = 0;
        while (i < s->len) { i += rl_utf8_char_size(s->data, s->len, i); ++n; }
        s->chars = n;
    }
    return s->chars;
}

/* A growable byte buffer for building strings. */
typedef struct { char *p; size_t n, cap; } buf;
static void b_add(buf *b, const char *s, size_t n) {
    if (n == 0 || n > ((size_t)-1) / 4) return;   /* the bound also keeps gcc's overflow check quiet */
    if (b->n + n + 1 > b->cap) {
        while (b->n + n + 1 > b->cap) b->cap = b->cap ? b->cap * 2 : 64;
        b->p = (char *)xrealloc(b->p, b->cap);
    }
    memcpy(b->p + b->n, s, n);
    b->n += n;
}
static void b_cstr(buf *b, const char *s) { b_add(b, s, strlen(s)); }
static rl_value b_done(buf *b) {
    rl_value v = rl_str_new(b->p ? b->p : "", b->n);
    free(b->p);
    return v;
}

/* Floats always show a decimal point or an exponent, so 3.0 and 3 differ. */
static void float_text(buf *b, double d) {
    char tmp[48];
    if (d != d) { b_cstr(b, "nan"); return; }
    if (d == HUGE_VAL) { b_cstr(b, "inf"); return; }
    if (d == -HUGE_VAL) { b_cstr(b, "-inf"); return; }
    snprintf(tmp, sizeof tmp, "%.15g", d);
    b_cstr(b, tmp);
    if (!strpbrk(tmp, ".en")) b_cstr(b, ".0");
}

static int is_plain_name(const rl_str *s) {
    size_t i;
    if (s->len == 0) return 0;
    for (i = 0; i < s->len; ++i) {
        unsigned char c = (unsigned char)s->data[i];
        int alpha = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
        if (!(alpha || (i > 0 && c >= '0' && c <= '9'))) return 0;
    }
    return 1;
}

static void quoted(buf *b, const rl_str *s) {
    size_t i;
    b_cstr(b, "\"");
    for (i = 0; i < s->len; ++i) {
        char c = s->data[i];
        if (c == '\n') b_cstr(b, "\\n");
        else if (c == '\t') b_cstr(b, "\\t");
        else if (c == '"') b_cstr(b, "\\\"");
        else if (c == '\\') b_cstr(b, "\\\\");
        else b_add(b, &c, 1);
    }
    b_cstr(b, "\"");
}

#define RL_MAX_SHOW_DEPTH 1000

static void repr_in(buf *b, rl_value v, objvec *path) {
    char tmp[32];
    size_t i;
    switch (v.k) {
        case RL_NONE: b_cstr(b, "none"); return;
        case RL_BOOL: b_cstr(b, v.u.b ? "true" : "false"); return;
        case RL_INT: snprintf(tmp, sizeof tmp, "%lld", v.u.i); b_cstr(b, tmp); return;
        case RL_FLOAT: float_text(b, v.u.f); return;
        case RL_STR: quoted(b, (rl_str *)v.u.o); return;
        case RL_FUN: b_cstr(b, "<fun "); b_add(b, ((rl_fun *)v.u.o)->name->data, ((rl_fun *)v.u.o)->name->len); b_cstr(b, ">"); return;
        case RL_MODULE: b_cstr(b, "<module "); b_add(b, ((rl_module *)v.u.o)->name->data, ((rl_module *)v.u.o)->name->len); b_cstr(b, ">"); return;
        default: break;
    }
    {
        int list = v.k == RL_LIST, seen = 0;
        for (i = 0; i < path->n; ++i) seen = seen || path->p[i] == v.u.o;
        if (seen || path->n >= RL_MAX_SHOW_DEPTH) { b_cstr(b, list ? "[...]" : "{...}"); return; }
        ov_push(path, v.u.o);
        if (list) {
            rl_list *l = (rl_list *)v.u.o;
            b_cstr(b, "[");
            for (i = 0; i < l->n; ++i) { if (i) b_cstr(b, ", "); repr_in(b, l->items[i], path); }
            b_cstr(b, "]");
        } else {
            rl_map *m = (rl_map *)v.u.o;
            b_cstr(b, "{");
            for (i = 0; i < m->n; ++i) {
                if (i) b_cstr(b, ", ");
                if (is_plain_name(m->keys[i])) b_add(b, m->keys[i]->data, m->keys[i]->len);
                else quoted(b, m->keys[i]);
                b_cstr(b, ": ");
                repr_in(b, m->vals[i], path);
            }
            b_cstr(b, "}");
        }
        --path->n;
    }
}

rl_value rl_repr(rl_value v) {
    buf b = {0, 0, 0};
    objvec path = {0, 0, 0};
    repr_in(&b, v, &path);
    free(path.p);
    return b_done(&b);
}

rl_value rl_show(rl_value v) {
    if (v.k == RL_STR) return rl_retain(v);
    return rl_repr(v);
}

/* ------------------------------------------------------------------ */
/* lists                                                                */

rl_value rl_list_new(size_t cap) {
    rl_list *l = (rl_list *)xmalloc(sizeof *l);
    track(&l->h, RL_LIST);
    l->n = 0;
    l->cap = cap;
    l->items = cap ? (rl_value *)xmalloc(cap * sizeof *l->items) : NULL;
    return rl_obj_value(&l->h);
}

void rl_list_push(rl_list *l, rl_value v) {
    if (l->n == l->cap) {
        l->cap = l->cap ? l->cap * 2 : 4;
        l->items = (rl_value *)xrealloc(l->items, l->cap * sizeof *l->items);
    }
    l->items[l->n++] = rl_retain(v);
}

/* ------------------------------------------------------------------ */
/* maps: entries in insertion order plus a hash index                   */

static size_t hash_bytes(const char *s, size_t n) {
    size_t h = (size_t)2166136261u, i;
    for (i = 0; i < n; ++i) { h ^= (unsigned char)s[i]; h *= (size_t)16777619u; }
    return h;
}

rl_value rl_map_new(void) {
    rl_map *m = (rl_map *)xmalloc(sizeof *m);
    track(&m->h, RL_MAP);
    m->n = m->cap = 0;
    m->keys = NULL;
    m->vals = NULL;
    m->index = NULL;
    m->index_cap = 0;
    return rl_obj_value(&m->h);
}

static void map_reindex(rl_map *m, size_t cap) {
    size_t i;
    free(m->index);
    m->index_cap = cap;
    m->index = (size_t *)xmalloc(cap * sizeof *m->index);
    memset(m->index, 0, cap * sizeof *m->index);
    for (i = 0; i < m->n; ++i) {
        size_t h = hash_bytes(m->keys[i]->data, m->keys[i]->len) & (cap - 1);
        while (m->index[h]) h = (h + 1) & (cap - 1);
        m->index[h] = i + 1;
    }
}

static size_t map_find(const rl_map *m, const char *key, size_t len) {
    size_t h;
    if (!m->index_cap) return (size_t)-1;
    h = hash_bytes(key, len) & (m->index_cap - 1);
    while (m->index[h]) {
        const rl_str *k = m->keys[m->index[h] - 1];
        if (k->len == len && memcmp(k->data, key, len) == 0) return m->index[h] - 1;
        h = (h + 1) & (m->index_cap - 1);
    }
    return (size_t)-1;
}

rl_value *rl_map_get(rl_map *m, const char *key, size_t len) {
    size_t i = map_find(m, key, len);
    return i == (size_t)-1 ? NULL : &m->vals[i];
}

void rl_map_set(rl_map *m, rl_str *key, rl_value v) {
    size_t i = map_find(m, key->data, key->len);
    if (i != (size_t)-1) {
        rl_value old = m->vals[i];
        m->vals[i] = rl_retain(v);
        rl_release(old);
        return;
    }
    if (m->n == m->cap) {
        m->cap = m->cap ? m->cap * 2 : 4;
        m->keys = (rl_str **)xrealloc(m->keys, m->cap * sizeof *m->keys);
        m->vals = (rl_value *)xrealloc(m->vals, m->cap * sizeof *m->vals);
    }
    ++key->h.rc;
    m->keys[m->n] = key;
    m->vals[m->n] = rl_retain(v);
    ++m->n;
    if ((m->n) * 2 > m->index_cap) map_reindex(m, m->index_cap ? m->index_cap * 2 : 8);
    else {
        size_t h = hash_bytes(key->data, key->len) & (m->index_cap - 1);
        while (m->index[h]) h = (h + 1) & (m->index_cap - 1);
        m->index[h] = m->n;
    }
}

/* Removes key, keeping the order of the rest. *out gets the old value. */
int rl_map_remove(rl_map *m, const char *key, size_t len, rl_value *out) {
    size_t i = map_find(m, key, len);
    if (i == (size_t)-1) return 0;
    *out = m->vals[i];
    release_obj(&m->keys[i]->h);
    memmove(m->keys + i, m->keys + i + 1, (m->n - i - 1) * sizeof *m->keys);
    memmove(m->vals + i, m->vals + i + 1, (m->n - i - 1) * sizeof *m->vals);
    --m->n;
    map_reindex(m, m->index_cap);
    return 1;
}

/* ------------------------------------------------------------------ */
/* scopes, functions, modules                                           */

rl_scope *rl_scope_new(rl_scope *parent, size_t n) {
    rl_scope *s = (rl_scope *)xmalloc(sizeof *s);
    size_t i;
    track(&s->h, RL_SCOPE);
    s->parent = parent;
    if (parent) ++parent->h.rc;
    s->n = n;
    s->slots = n ? (rl_value *)xmalloc(n * sizeof *s->slots) : NULL;
    for (i = 0; i < n; ++i) s->slots[i] = rl_unset();
    return s;
}

void rl_scope_release(rl_scope *s) { if (s) release_obj(&s->h); }

rl_value rl_fun_new(const char *name, rl_code code, int arity, rl_scope *env) {
    rl_fun *f = (rl_fun *)xmalloc(sizeof *f);
    rl_value n = rl_str_c(name);
    track(&f->h, RL_FUN);
    f->name = (rl_str *)n.u.o;
    f->code = code;
    f->arity = arity;
    f->builtin = 0;
    f->env = env;
    f->std_module = NULL;
    if (env) ++env->h.rc;
    return rl_obj_value(&f->h);
}

rl_value rl_fun_std(rl_value f, const char *stem) {
    if (f.k == RL_FUN) ((rl_fun *)f.u.o)->std_module = stem;
    return f;
}

rl_value rl_module_new(const char *name) {
    rl_module *m = (rl_module *)xmalloc(sizeof *m);
    rl_value n = rl_str_c(name), names = rl_map_new();
    track(&m->h, RL_MODULE);
    m->name = (rl_str *)n.u.o;
    m->names = (rl_map *)names.u.o;
    m->scope = NULL;
    m->nnames = 0;
    m->snames = NULL;
    return rl_obj_value(&m->h);
}

rl_value rl_module_scoped(const char *name, rl_scope *scope, size_t n, const char *const *names) {
    rl_module *m = (rl_module *)xmalloc(sizeof *m);
    rl_value nm = rl_str_c(name);
    track(&m->h, RL_MODULE);
    m->name = (rl_str *)nm.u.o;
    m->names = NULL;
    m->scope = scope;
    ++scope->h.rc;
    m->nnames = n;
    m->snames = names;
    return rl_obj_value(&m->h);
}

/* ------------------------------------------------------------------ */
/* equality (host rules: 1 == 1.0, lists and maps by contents)          */

#define RL_MAX_EQUAL_DEPTH 10000

static int equal_at(rl_value a, rl_value b, int depth) {
    size_t i;
    if (depth > RL_MAX_EQUAL_DEPTH) return 0;
    if ((a.k == RL_INT || a.k == RL_FLOAT) && (b.k == RL_INT || b.k == RL_FLOAT)) {
        if (a.k == RL_INT && b.k == RL_INT) return a.u.i == b.u.i;
        return (a.k == RL_INT ? (double)a.u.i : a.u.f) == (b.k == RL_INT ? (double)b.u.i : b.u.f);
    }
    if (a.k != b.k) return 0;
    switch (a.k) {
        case RL_NONE: return 1;
        case RL_BOOL: return a.u.b == b.u.b;
        case RL_STR: {
            rl_str *x = (rl_str *)a.u.o, *y = (rl_str *)b.u.o;
            return x->len == y->len && memcmp(x->data, y->data, x->len) == 0;
        }
        case RL_LIST: {
            rl_list *x = (rl_list *)a.u.o, *y = (rl_list *)b.u.o;
            if (x == y) return 1;
            if (x->n != y->n) return 0;
            for (i = 0; i < x->n; ++i)
                if (!equal_at(x->items[i], y->items[i], depth + 1)) return 0;
            return 1;
        }
        case RL_MAP: {
            rl_map *x = (rl_map *)a.u.o, *y = (rl_map *)b.u.o;
            if (x == y) return 1;
            if (x->n != y->n) return 0;
            for (i = 0; i < x->n; ++i) {
                rl_value *w = rl_map_get(y, x->keys[i]->data, x->keys[i]->len);
                if (!w || !equal_at(x->vals[i], *w, depth + 1)) return 0;
            }
            return 1;
        }
        default: return a.u.o == b.u.o;
    }
}

int rl_equal(rl_value a, rl_value b) { return equal_at(a, b, 0); }

/* ------------------------------------------------------------------ */
/* errors                                                               */

int rl_fail_str(rl_ctx *cx, rl_str *msg) {
    if (cx->err_msg) release_obj(&cx->err_msg->h);
    ++msg->h.rc;
    cx->err_msg = msg;
    cx->has_err = 1;
    cx->err_line = cx->err_col = 0;
    cx->err_arg = 0;
    cx->err_file = NULL;
    cx->err_std = 0;
    return RL_ERR;
}

int rl_fail(rl_ctx *cx, const char *fmt, ...) {
    char small[256];
    char *p = small;
    int n;
    rl_value s;
    va_list ap;
    va_start(ap, fmt);
    n = vsnprintf(small, sizeof small, fmt, ap);
    va_end(ap);
    if (n < 0) n = 0;
    if ((size_t)n >= sizeof small) {
        p = (char *)xmalloc((size_t)n + 1);
        va_start(ap, fmt);
        vsnprintf(p, (size_t)n + 1, fmt, ap);
        va_end(ap);
    }
    s = rl_str_new(p, (size_t)n);
    if (p != small) free(p);
    rl_fail_str(cx, (rl_str *)s.u.o);
    rl_release(s);
    return RL_ERR;
}

void rl_at(rl_ctx *cx, const char *file, int line, int col) {
    if (!cx->has_err || cx->err_line) return;
    cx->err_file = file;
    cx->err_line = line;
    cx->err_col = col;
}

void rl_at3(rl_ctx *cx, const char *file, int in_std, int l, int c, int l1, int c1, int l2, int c2) {
    if (cx->has_err && !cx->err_line) cx->err_std = in_std;
    if (cx->err_arg == 1) rl_at(cx, file, l1, c1);
    else if (cx->err_arg == 2) rl_at(cx, file, l2, c2);
    else rl_at(cx, file, l, c);
}

void rl_interp(int n, const rl_value *parts, rl_value *out) {
    buf b = {0, 0, 0};
    int i;
    for (i = 0; i < n; ++i) {
        rl_value s = rl_show(parts[i]);
        b_add(&b, ((rl_str *)s.u.o)->data, ((rl_str *)s.u.o)->len);
        rl_release(s);
    }
    *out = b_done(&b);
}

int rl_catch(rl_ctx *cx, rl_value *out) {
    rl_value m, k;
    if (!cx->has_err) { *out = rl_none(); return 0; }
    m = rl_map_new();
    k = rl_str_c("message"); rl_map_set((rl_map *)m.u.o, (rl_str *)k.u.o, rl_obj_value(&cx->err_msg->h)); rl_release(k);
    k = rl_str_c("line"); rl_map_set((rl_map *)m.u.o, (rl_str *)k.u.o, rl_int(cx->err_line)); rl_release(k);
    k = rl_str_c("col"); rl_map_set((rl_map *)m.u.o, (rl_str *)k.u.o, rl_int(cx->err_col)); rl_release(k);
    release_obj(&cx->err_msg->h);
    cx->err_msg = NULL;
    cx->has_err = 0;
    cx->err_line = cx->err_col = 0;
    cx->err_file = NULL;
    *out = m;
    return 1;
}

/* Port of diag::snippet in src/diag/diag.cpp. */
static void snippet(FILE *to, const char *src, int line, int col) {
    const char *p = src, *end;
    size_t n, from = 0, i;
    int k;
    if (!src || line < 1 || col < 1) return;
    for (k = 1; k < line; ++k) {
        p = strchr(p, '\n');
        if (!p) return;
        ++p;
    }
    end = strchr(p, '\n');
    n = end ? (size_t)(end - p) : strlen(p);
    if (n && p[n - 1] == '\r') --n;
    for (i = 0; i < n && (p[i] == ' ' || p[i] == '\t'); ++i) {}
    if (i == n) return;
    if (n > 160) {   /* very long lines are cut to a window around the column */
        size_t to_ = 0;
        from = (size_t)col > 80 ? (size_t)col - 80 : 0;
        while (from > 0 && ((unsigned char)p[from] & 0xC0) == 0x80) --from;
        to_ = from + 160;
        while (to_ < n && ((unsigned char)p[to_] & 0xC0) == 0x80) ++to_;
        if (to_ > n) to_ = n;
        p += from;
        n = to_ - from;
        col -= (int)from;
    }
    fputs("    ", to);
    fwrite(p, 1, n, to);
    fputs("\n    ", to);
    for (i = 0; i + 1 < (size_t)col && i < n; ++i) {   /* the caret lines up by characters */
        unsigned char c = (unsigned char)p[i];
        if ((c & 0xC0) == 0x80) continue;
        fputc(c == '\t' ? '\t' : ' ', to);
    }
    fputs("^\n", to);
}

/* With no source text, the file is read now, as the host does. */
static char *read_whole(const char *path) {
    FILE *f = fopen(path, "rb");
    char *data = NULL;
    size_t n = 0, cap = 0, got;
    if (!f) return NULL;
    for (;;) {
        if (n + 1 >= cap) { cap = cap ? cap * 2 : 4096; data = (char *)xrealloc(data, cap); }
        got = fread(data + n, 1, cap - n - 1, f);
        n += got;
        if (got == 0) break;
    }
    fclose(f);
    data[n] = 0;
    return data;
}

void rl_report(rl_ctx *cx, FILE *to, const char *source) {
    char *owned = NULL;
    if (!cx->has_err) return;
    fprintf(to, "%s:%d:%d: error: %s\n", cx->err_file ? cx->err_file : "<program>", cx->err_line,
            cx->err_col, cx->err_msg->data);
    if (!source && cx->err_file) source = owned = read_whole(cx->err_file);
    snippet(to, source, cx->err_line, cx->err_col);
    free(owned);
}

/* ------------------------------------------------------------------ */
/* calls                                                                */

static const char *a_type_of(rl_value v) {
    switch (v.k) {
        case RL_INT: return "an int";
        case RL_NONE: return "a none";
        case RL_BOOL: return "a bool";
        case RL_FLOAT: return "a float";
        case RL_STR: return "a string";
        case RL_LIST: return "a list";
        case RL_MAP: return "a map";
        case RL_MODULE: return "a module";
        default: return "a function";
    }
}
const char *rl_a_type(rl_value v) { return a_type_of(v); }

int rl_call(rl_ctx *cx, rl_value fv, int argc, const rl_value *argv, rl_value *out) {
    rl_fun *f;
    int r;
    *out = rl_none();
    if (fv.k != RL_FUN) return rl_fail(cx, "cannot call %s", a_type_of(fv));
    f = (rl_fun *)fv.u.o;
    if (f->arity >= 0 && argc != f->arity)
        return rl_fail(cx, "%s() takes %d argument%s, got %d", f->name->data, f->arity,
                       f->arity == 1 ? "" : "s", argc);
    if (cx->depth >= RL_MAX_DEPTH)
        return rl_fail(cx, "too much recursion (more than %d calls deep)", RL_MAX_DEPTH);
    ++cx->depth;
    ++f->h.rc;   /* the callee stays alive while it runs */
    r = f->code(cx, f, argc, argv, out);
    --cx->depth;
    if (r != RL_OK && f->builtin && cx->has_err && !cx->err_line && strcmp(f->name->data, "fail") != 0) {
        /* Built-in errors read "name(): message", as on the host. */
        rl_value m;
        buf b = {0, 0, 0};
        b_add(&b, f->name->data, f->name->len);
        b_cstr(&b, "(): ");
        b_add(&b, cx->err_msg->data, cx->err_msg->len);
        m = b_done(&b);
        rl_fail_str(cx, (rl_str *)m.u.o);
        rl_release(m);
    }
    release_obj(&f->h);
    if (r != RL_OK) { rl_release(*out); *out = rl_none(); }
    rl_gc_maybe();
    return r;
}

void rl_std_boundary(rl_ctx *cx, rl_value fv) {
    rl_fun *f;
    const char *msg;
    if (!cx->has_err || fv.k != RL_FUN) return;
    f = (rl_fun *)fv.u.o;
    if (!f->std_module || !cx->err_std || !cx->err_line) return;
    msg = cx->err_msg->data;
    if (strncmp(msg, "host_", 5) == 0 && strstr(msg, "(): ")) {
        rl_value m;
        buf b = {0, 0, 0};
        b_cstr(&b, f->std_module);
        b_cstr(&b, ".");
        b_add(&b, f->name->data, f->name->len);
        b_cstr(&b, "(): ");
        b_cstr(&b, strstr(msg, "(): ") + 4);
        m = b_done(&b);
        rl_fail_str(cx, (rl_str *)m.u.o);   /* also clears the position */
        rl_release(m);
        return;
    }
    cx->err_line = cx->err_col = 0;
    cx->err_arg = 0;
    cx->err_file = NULL;
    cx->err_std = 0;
}

void rl_ctx_init(rl_ctx *cx, int argc, char **argv) {
    cx->out = stdout;
    cx->has_err = 0;
    cx->err_msg = NULL;
    cx->err_line = cx->err_col = 0;
    cx->err_arg = 0;
    cx->err_file = NULL;
    cx->err_std = 0;
    cx->depth = 0;
    cx->argc = argc;
    cx->argv = argv;
}

void rl_ctx_free(rl_ctx *cx) {
    if (cx->err_msg) release_obj(&cx->err_msg->h);
    cx->err_msg = NULL;
    cx->has_err = 0;
    rl_builtins_release();
    free(g_dying);
    g_dying = NULL;
    g_dying_n = g_dying_cap = 0;
}
