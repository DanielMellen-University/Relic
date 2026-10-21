/* rl_ops.c: operators, indexing, fields, ranges, conditions and number
 * parsing. Messages match the host (src/interp/interp.cpp, op_*). */
#include "relic.h"
#include "rl_internal.h"

#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define S(v) ((rl_str *)(v).u.o)
#define L(v) ((rl_list *)(v).u.o)
#define M(v) ((rl_map *)(v).u.o)

static int is_num(rl_value v) { return v.k == RL_INT || v.k == RL_FLOAT; }
static double to_num(rl_value v) { return v.k == RL_INT ? (double)v.u.i : v.u.f; }

/* int overflow is an error, as on the host. */
static int add_ovf(long long x, long long y, long long *r) {
    if ((y > 0 && x > LLONG_MAX - y) || (y < 0 && x < LLONG_MIN - y)) return 1;
    *r = x + y;
    return 0;
}
static int sub_ovf(long long x, long long y, long long *r) {
    if ((y < 0 && x > LLONG_MAX + y) || (y > 0 && x < LLONG_MIN + y)) return 1;
    *r = x - y;
    return 0;
}
static int mul_ovf(long long x, long long y, long long *r) {
    if (x == 0 || y == 0) { *r = 0; return 0; }
    if (x == -1) { if (y == LLONG_MIN) return 1; *r = -y; return 0; }
    if (y == -1) { if (x == LLONG_MIN) return 1; *r = -x; return 0; }
    if (x > 0 ? (y > 0 ? x > LLONG_MAX / y : y < LLONG_MIN / x)
              : (y > 0 ? x < LLONG_MIN / y : x < LLONG_MAX / y)) return 1;
    *r = x * y;
    return 0;
}

static int math(rl_ctx *cx, char o, rl_value a, rl_value b, rl_value *out) {
    *out = rl_none();
    if (a.k == RL_INT && b.k == RL_INT) {
        long long x = a.u.i, y = b.u.i, r = 0;
        switch (o) {
            case '+': if (add_ovf(x, y, &r)) return rl_fail(cx, "int overflow"); *out = rl_int(r); return RL_OK;
            case '-': if (sub_ovf(x, y, &r)) return rl_fail(cx, "int overflow"); *out = rl_int(r); return RL_OK;
            case '*': if (mul_ovf(x, y, &r)) return rl_fail(cx, "int overflow"); *out = rl_int(r); return RL_OK;
            case '/':
                if (y == 0) return rl_fail(cx, "division by zero");
                *out = rl_float((double)x / (double)y);
                return RL_OK;
            default:
                if (y == 0) return rl_fail(cx, "division by zero");
                if (y == -1) { *out = rl_int(0); return RL_OK; }
                r = x % y;
                if (r != 0 && ((r < 0) != (y < 0))) r += y;  /* takes the sign of the divisor */
                *out = rl_int(r);
                return RL_OK;
        }
    } else {
        double x = to_num(a), y = to_num(b), r;
        switch (o) {
            case '+': *out = rl_float(x + y); return RL_OK;
            case '-': *out = rl_float(x - y); return RL_OK;
            case '*': *out = rl_float(x * y); return RL_OK;
            default: break;
        }
        if (y == 0) return rl_fail(cx, "division by zero");
        if (o == '/') { *out = rl_float(x / y); return RL_OK; }
        r = fmod(x, y);
        if (r != 0 && ((r < 0) != (y < 0))) r += y;
        *out = rl_float(r);
        return RL_OK;
    }
}

static int mismatch(rl_ctx *cx, const char *verb, rl_value a, rl_value b) {
    return rl_fail(cx, "cannot %s %s and %s", verb, rl_a_type(a), rl_a_type(b));
}

int rl_add(rl_ctx *cx, rl_value a, rl_value b, rl_value *out) {
    *out = rl_none();
    if (is_num(a) && is_num(b)) return math(cx, '+', a, b, out);
    if (a.k == RL_STR && b.k == RL_STR) {
        {
            size_t n = S(a)->len + S(b)->len;
            char *p = (char *)malloc(n ? n : 1);
            if (!p) abort();
            memcpy(p, S(a)->data, S(a)->len);
            memcpy(p + S(a)->len, S(b)->data, S(b)->len);
            *out = rl_str_new(p, n);
            free(p);
        }
        return RL_OK;
    }
    if (a.k == RL_LIST && b.k == RL_LIST) {
        size_t i, na = L(a)->n, nb = L(b)->n;
        rl_value r = rl_list_new(na + nb);
        for (i = 0; i < na; ++i) rl_list_push(L(r), L(a)->items[i]);
        for (i = 0; i < nb; ++i) rl_list_push(L(r), L(b)->items[i]);
        *out = r;
        return RL_OK;
    }
    return mismatch(cx, "add", a, b);
}

int rl_sub(rl_ctx *cx, rl_value a, rl_value b, rl_value *out) {
    if (!is_num(a) || !is_num(b)) { *out = rl_none(); return mismatch(cx, "subtract", a, b); }
    return math(cx, '-', a, b, out);
}
int rl_mul(rl_ctx *cx, rl_value a, rl_value b, rl_value *out) {
    if (!is_num(a) || !is_num(b)) { *out = rl_none(); return mismatch(cx, "multiply", a, b); }
    return math(cx, '*', a, b, out);
}
int rl_div(rl_ctx *cx, rl_value a, rl_value b, rl_value *out) {
    if (!is_num(a) || !is_num(b)) { *out = rl_none(); return mismatch(cx, "divide", a, b); }
    return math(cx, '/', a, b, out);
}
int rl_mod(rl_ctx *cx, rl_value a, rl_value b, rl_value *out) {
    if (!is_num(a) || !is_num(b)) { *out = rl_none(); return mismatch(cx, "divide", a, b); }
    return math(cx, '%', a, b, out);
}

int rl_neg(rl_ctx *cx, rl_value v, rl_value *out) {
    *out = rl_none();
    if (v.k == RL_INT) {
        long long r;
        if (sub_ovf(0, v.u.i, &r)) return rl_fail(cx, "int overflow");
        *out = rl_int(r);
        return RL_OK;
    }
    if (v.k != RL_FLOAT) return rl_fail(cx, "cannot negate %s", rl_a_type(v));
    *out = rl_float(-v.u.f);
    return RL_OK;
}

int rl_not(rl_ctx *cx, rl_value v, rl_value *out) {
    *out = rl_none();
    if (v.k != RL_BOOL) return rl_fail(cx, "'not' needs true or false, got %s", rl_type_name(v));
    *out = rl_bool(!v.u.b);
    return RL_OK;
}

static int bytes_cmp(const rl_str *x, const rl_str *y) {
    size_t n = x->len < y->len ? x->len : y->len;
    int c = memcmp(x->data, y->data, n);
    if (c) return c < 0 ? -1 : 1;
    return x->len < y->len ? -1 : (x->len > y->len ? 1 : 0);
}

int rl_compare(rl_ctx *cx, const char *op, rl_value a, rl_value b, rl_value *out) {
    int c;
    *out = rl_none();
    if (a.k == RL_INT && b.k == RL_INT) c = a.u.i < b.u.i ? -1 : (a.u.i > b.u.i ? 1 : 0);
    else if (is_num(a) && is_num(b)) {
        double x = to_num(a), y = to_num(b);
        c = x < y ? -1 : (x > y ? 1 : 0);
    } else if (a.k == RL_STR && b.k == RL_STR) c = bytes_cmp(S(a), S(b));
    else return mismatch(cx, "compare", a, b);
    if (op[0] == '<') *out = rl_bool(op[1] == '=' ? c <= 0 : c < 0);
    else *out = rl_bool(op[1] == '=' ? c >= 0 : c > 0);
    return RL_OK;
}

static int contains(const rl_str *hay, const rl_str *needle) {
    size_t i;
    if (needle->len == 0) return 1;
    if (needle->len > hay->len) return 0;
    for (i = 0; i + needle->len <= hay->len; ++i)
        if (memcmp(hay->data + i, needle->data, needle->len) == 0) return 1;
    return 0;
}

int rl_in(rl_ctx *cx, rl_value a, rl_value b, rl_value *out) {
    size_t i;
    *out = rl_none();
    if (b.k == RL_LIST) {
        for (i = 0; i < L(b)->n; ++i)
            if (rl_equal(a, L(b)->items[i])) { *out = rl_bool(1); return RL_OK; }
        *out = rl_bool(0);
        return RL_OK;
    }
    if (b.k == RL_MAP) {
        if (a.k != RL_STR) return rl_fail(cx, "map keys must be strings");
        *out = rl_bool(rl_map_get(M(b), S(a)->data, S(a)->len) != NULL);
        return RL_OK;
    }
    if (b.k == RL_STR) {
        if (a.k != RL_STR) return rl_fail(cx, "can only look for a string inside a string");
        *out = rl_bool(contains(S(b), S(a)));
        return RL_OK;
    }
    return rl_fail(cx, "cannot look inside %s", rl_a_type(b));
}

/* A number as a whole-number index, or an error. */
static int whole(rl_ctx *cx, rl_value v, const char *what, long long *r) {
    *r = 0;
    if (v.k == RL_INT) { *r = v.u.i; return RL_OK; }
    if (v.k != RL_FLOAT) return rl_fail(cx, "%s must be a number, got %s", what, rl_type_name(v));
    if (v.u.f != floor(v.u.f) || fabs(v.u.f) > 9e15) return rl_fail(cx, "%s must be a whole number", what);
    *r = (long long)v.u.f;
    return RL_OK;
}

static int slot(rl_ctx *cx, rl_value idx, size_t size, const char *what, size_t *at) {
    long long i = 0;
    *at = 0;
    if (whole(cx, idx, what, &i)) return RL_ERR;
    if (i < 0 || (unsigned long long)i >= size)
        return rl_fail(cx, "%s %lld is out of range (size %lu)", what, i, (unsigned long)size);
    *at = (size_t)i;
    return RL_OK;
}

/* " (did you mean 'x'?)" for a close key, like op_suggest on the host. */
static size_t edit_distance(const char *a, size_t n, const char *b, size_t m) {
    size_t *row = (size_t *)malloc((m + 1) * sizeof *row), i, j, r;
    if (!row) abort();
    for (j = 0; j <= m; ++j) row[j] = j;
    for (i = 1; i <= n; ++i) {
        size_t prev = row[0];
        row[0] = i;
        for (j = 1; j <= m; ++j) {
            size_t cur = row[j];
            size_t best = prev + (a[i - 1] == b[j - 1] ? 0 : 1);
            if (row[j] + 1 < best) best = row[j] + 1;
            if (row[j - 1] + 1 < best) best = row[j - 1] + 1;
            row[j] = best;
            prev = cur;
        }
    }
    r = row[m];
    free(row);
    return r;
}

static int no_key(rl_ctx *cx, rl_map *m, const char *k, size_t len) {
    size_t i, limit = len >= 5 ? 2 : 1, best_d = limit + 1;
    const rl_str *pick = NULL;
    for (i = 0; i < m->n; ++i) {
        const rl_str *c = m->keys[i];
        size_t d, lo = c->len < len ? len - c->len : c->len - len;
        if (c->len == 0 || (c->len == len && memcmp(c->data, k, len) == 0)) continue;
        if (strncmp(c->data, "host_", 5) == 0 && strncmp(k, "host_", 5) != 0) continue;
        if (lo > limit) continue;
        d = edit_distance(k, len, c->data, c->len);
        if (d >= len || d > limit) continue;
        if (d < best_d || (d == best_d && strcmp(c->data, pick->data) < 0)) { best_d = d; pick = c; }
    }
    if (pick) return rl_fail(cx, "map has no key '%s' (did you mean '%s'?)", k, pick->data);
    return rl_fail(cx, "map has no key '%s'", k);
}

/* op_suggest on the host: the closest name within a small edit distance. */
static const char *suggest(const char *k, size_t len, const char *best, size_t *best_d, const char *c) {
    size_t d, clen = strlen(c), lo = clen < len ? len - clen : clen - len, limit = len >= 5 ? 2 : 1;
    if (clen == 0 || (clen == len && memcmp(c, k, len) == 0)) return best;
    if (strncmp(c, "host_", 5) == 0 && strncmp(k, "host_", 5) != 0) return best;
    if (lo > limit) return best;
    d = edit_distance(k, len, c, clen);
    if (d >= len || d > limit) return best;
    if (d < *best_d || (d == *best_d && strcmp(c, best) < 0)) { *best_d = d; return c; }
    return best;
}

int rl_fail_undefined(rl_ctx *cx, const char *name, int n, const char *const *names, const rl_value *const *slots) {
    size_t len = strlen(name), best_d = (size_t)-1, i, nb = rl_builtin_count();
    const char *best = NULL;
    int j;
    for (j = 0; j < n; ++j)
        if (!slots[j] || slots[j]->k != RL_UNSET) best = suggest(name, len, best, &best_d, names[j]);
    for (i = 0; i < nb; ++i) best = suggest(name, len, best, &best_d, rl_builtin_at(i)->name);
    if (best) return rl_fail(cx, "'%s' is not defined (did you mean '%s'?)", name, best);
    return rl_fail(cx, "'%s' is not defined", name);
}

/* Byte offset of character `index`. A string with as many characters as
 * bytes is all one-byte characters, so that is direct; the count is cached. */
static size_t utf8_offset(const rl_str *s, size_t index) {
    size_t i = 0, n = 0;
    if (rl_str_chars((rl_str *)s) == s->len) return index < s->len ? index : s->len;
    while (i < s->len && n < index) { i += rl_utf8_char_size(s->data, s->len, i); ++n; }
    return i;
}

/* Index and key errors point at the index (operand 2), as on the host. */
static int at_index(rl_ctx *cx) { cx->err_arg = 2; return RL_ERR; }

int rl_index(rl_ctx *cx, rl_value obj, rl_value idx, rl_value *out) {
    *out = rl_none();
    if (obj.k == RL_LIST) {
        size_t at = 0;
        if (slot(cx, idx, L(obj)->n, "list index", &at)) return at_index(cx);
        *out = rl_retain(L(obj)->items[at]);
        return RL_OK;
    }
    if (obj.k == RL_STR) {
        rl_str *s = S(obj);
        long long i;
        size_t at = 0;
        if (whole(cx, idx, "string index", &i)) return at_index(cx);
        at = i < 0 ? s->len : utf8_offset(s, (size_t)i);
        if (at >= s->len) {
            size_t dummy;
            slot(cx, idx, rl_str_chars(s), "string index", &dummy);
            return at_index(cx);
        }
        *out = rl_str_new(s->data + at, rl_utf8_char_size(s->data, s->len, at));
        return RL_OK;
    }
    if (obj.k == RL_MAP) {
        rl_value *v;
        if (idx.k != RL_STR) { rl_fail(cx, "map keys must be strings"); return at_index(cx); }
        v = rl_map_get(M(obj), S(idx)->data, S(idx)->len);
        if (!v) { no_key(cx, M(obj), S(idx)->data, S(idx)->len); return at_index(cx); }
        *out = rl_retain(*v);
        return RL_OK;
    }
    return rl_fail(cx, "cannot index %s", rl_a_type(obj));
}

int rl_set_index(rl_ctx *cx, rl_value obj, rl_value idx, rl_value v) {
    if (obj.k == RL_LIST) {
        size_t at = 0;
        rl_value old;
        if (slot(cx, idx, L(obj)->n, "list index", &at)) return at_index(cx);
        old = L(obj)->items[at];
        L(obj)->items[at] = rl_retain(v);
        rl_release(old);
        return RL_OK;
    }
    if (obj.k == RL_MAP) {
        if (idx.k != RL_STR) { rl_fail(cx, "map keys must be strings"); return at_index(cx); }
        rl_map_set(M(obj), S(idx), v);
        return RL_OK;
    }
    return rl_fail(cx, "cannot set an index on %s", rl_a_type(obj));
}

int rl_field(rl_ctx *cx, rl_value obj, const char *name, rl_value *out) {
    size_t len = strlen(name);
    rl_value *v;
    *out = rl_none();
    if (obj.k == RL_MODULE) {
        rl_module *m = (rl_module *)obj.u.o;
        if (name[0] == '_') return rl_fail(cx, "'%s' is private to module '%s'", name, m->name->data);
        v = NULL;
        if (m->scope) {
            size_t i;
            for (i = 0; i < m->nnames && i < m->scope->n; ++i)
                if (strcmp(m->snames[i], name) == 0 && m->scope->slots[i].k != RL_UNSET) v = &m->scope->slots[i];
        } else if (m->names) {
            v = rl_map_get(m->names, name, len);
        }
        if (!v) return rl_fail(cx, "module '%s' has no '%s' (yet)", m->name->data, name);
        *out = rl_retain(*v);
        return RL_OK;
    }
    if (obj.k != RL_MAP) return rl_fail(cx, "cannot read a field of %s", rl_a_type(obj));
    v = rl_map_get(M(obj), name, len);
    if (!v) return no_key(cx, M(obj), name, len);
    *out = rl_retain(*v);
    return RL_OK;
}

int rl_set_field(rl_ctx *cx, rl_value obj, const char *name, rl_value v) {
    rl_value k;
    if (obj.k == RL_MODULE)
        return rl_fail(cx, "cannot change a module's names from outside; give it a function that does it");
    if (obj.k != RL_MAP) return rl_fail(cx, "cannot set a field on %s", rl_a_type(obj));
    k = rl_str_c(name);
    rl_map_set(M(obj), S(k), v);
    rl_release(k);
    return RL_OK;
}

int rl_range(rl_ctx *cx, rl_value av, rl_value bv, rl_value *out) {
    long long a = 0, b = 0, i;
    rl_value l;
    *out = rl_none();
    if (whole(cx, av, "range start", &a)) { cx->err_arg = 1; return RL_ERR; }
    if (whole(cx, bv, "range end", &b)) { cx->err_arg = 2; return RL_ERR; }
    if (b - a > 10000000) return rl_fail(cx, "range is too large");
    l = rl_list_new(b > a ? (size_t)(b - a) : 0);
    for (i = a; i < b; ++i) rl_list_push(L(l), rl_int(i));
    *out = l;
    return RL_OK;
}

int rl_cond(rl_ctx *cx, rl_value v, const char *what, int *truth) {
    if (v.k != RL_BOOL) return rl_fail(cx, "%s must be true or false, got %s", what, rl_type_name(v));
    *truth = v.u.b;
    return RL_OK;
}

int rl_loop_items(rl_ctx *cx, rl_value src, rl_value *out) {
    size_t i;
    rl_value l;
    *out = rl_none();
    if (src.k == RL_LIST) {
        l = rl_list_new(L(src)->n);
        for (i = 0; i < L(src)->n; ++i) rl_list_push(L(l), L(src)->items[i]);
    } else if (src.k == RL_STR) {
        rl_str *s = S(src);
        l = rl_list_new(0);
        for (i = 0; i < s->len;) {
            size_t n = rl_utf8_char_size(s->data, s->len, i);
            rl_value c = rl_str_new(s->data + i, n);
            rl_list_push(L(l), c);
            rl_release(c);
            i += n;
        }
    } else if (src.k == RL_MAP) {
        l = rl_list_new(M(src)->n);
        for (i = 0; i < M(src)->n; ++i) rl_list_push(L(l), rl_obj_value(&M(src)->keys[i]->h));
    } else {
        return rl_fail(cx, "cannot loop over %s", rl_a_type(src));
    }
    *out = l;
    return RL_OK;
}

int rl_check_type(rl_ctx *cx, const char *type, rl_value v, const char *what) {
    int ok = strcmp(type, "any") == 0 || (strcmp(type, "number") == 0 ? is_num(v) : strcmp(type, rl_type_name(v)) == 0);
    if (!ok) return rl_fail(cx, "%s %s, got %s", what, type, rl_type_name(v));
    return RL_OK;
}

/* num("...") rules from the host's parse_num: digits give an int when they
 * fit, otherwise the whole text must read as a float (strtod, so hex,
 * inf and nan work); out-of-range floats are not numbers. */
int rl_parse_num(rl_ctx *cx, const char *s, size_t len, rl_value *out) {
    size_t i, j;
    int digits;
    char *end;
    double d;
    *out = rl_none();
    if (len == 0 || s[0] == ' ' || (s[0] >= '\t' && s[0] <= '\r') || strlen(s) != len) goto bad;
    i = (s[0] == '-' || s[0] == '+') ? 1 : 0;
    digits = i < len;
    for (j = i; j < len; ++j)
        if (s[j] < '0' || s[j] > '9') digits = 0;
    if (digits) {
        long long v;
        errno = 0;
        v = strtoll(s, NULL, 10);
        if (errno != ERANGE) { *out = rl_int(v); return RL_OK; }
    }
    errno = 0;
    d = strtod(s, &end);
    if (end == s || (size_t)(end - s) != len || errno == ERANGE) goto bad;
    *out = rl_float(d);
    return RL_OK;
bad:
    return rl_fail(cx, "not a number: \"%s\"", s);
}
