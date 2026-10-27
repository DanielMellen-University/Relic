/* test_runtime.c: unit tests for the C runtime. Expected strings come from
 * the C++ host (relic 1.0.2) so the two stay in step. Ends by checking that
 * every container and string was freed. */
#include "../relic.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gfx_scale.h"   /* src/builtins: the high-DPI math of both graphics modules */

static int g_pass = 0, g_fail = 0;
static rl_ctx cx;

#define L(v) ((rl_list *)(v).u.o)
#define M(v) ((rl_map *)(v).u.o)
#define SD(v) (((rl_str *)(v).u.o)->data)

static void check(int ok, const char *what, int line) {
    if (ok) ++g_pass;
    else { ++g_fail; fprintf(stderr, "FAIL line %d: %s\n", line, what); }
}
#define CHECK(c) check((c) != 0, #c, __LINE__)

/* Shows v (and releases it), compares with want. */
static void show_is(rl_value v, const char *want, int line) {
    rl_value s = rl_show(v);
    int ok = strcmp(SD(s), want) == 0;
    if (!ok) fprintf(stderr, "  got  [%s]\n  want [%s]\n", SD(s), want);
    check(ok, want, line);
    rl_release(s);
    rl_release(v);
}
#define SHOW(v, want) show_is((v), (want), __LINE__)

/* r must be RL_ERR with message want; clears the error. */
static void err_is(int r, const char *want, int line) {
    rl_value e, msg;
    int ok = r == RL_ERR && cx.has_err && strcmp(cx.err_msg->data, want) == 0;
    if (!ok) fprintf(stderr, "  got  [%s]\n  want [%s]\n", cx.has_err ? cx.err_msg->data : "(no error)", want);
    check(ok, want, line);
    if (rl_catch(&cx, &e)) {
        rl_field(&cx, e, "message", &msg);
        rl_release(msg);
        rl_release(e);
    }
}
#define ERR(r, want) err_is((r), (want), __LINE__)

static rl_value S(const char *s) { return rl_str_c(s); }

/* T(): a string the test borrows; all are released by drop_temps(). */
static rl_value g_temps[64];
static int g_ntemps = 0;
static rl_value T(const char *s) { return g_temps[g_ntemps++] = rl_str_c(s); }
static void drop_temps(void) { while (g_ntemps) rl_release(g_temps[--g_ntemps]); }

/* Calls a built-in by name with n borrowed args; *out owned. */
static int call(const char *name, int n, const rl_value *args, rl_value *out) {
    rl_value f;
    int r;
    if (!rl_builtin(name, &f)) { fprintf(stderr, "no built-in %s\n", name); exit(2); }
    r = rl_call(&cx, f, n, args, out);
    rl_release(f);
    return r;
}
static rl_value call1(const char *name, rl_value a) {
    rl_value out;
    if (call(name, 1, &a, &out)) { fprintf(stderr, "unexpected error in %s: %s\n", name, cx.err_msg->data); exit(2); }
    return out;
}
static rl_value calls(const char *name, const char *arg) {
    rl_value a = S(arg), r = call1(name, a);
    rl_release(a);
    return r;
}
static rl_value op(int (*f)(rl_ctx *, rl_value, rl_value, rl_value *), rl_value a, rl_value b) {
    rl_value out;
    if (f(&cx, a, b, &out)) { fprintf(stderr, "unexpected error: %s\n", cx.err_msg->data); exit(2); }
    rl_release(a);
    rl_release(b);
    return out;
}

static void set(rl_value map, const char *k, rl_value v) {
    rl_value key = S(k);
    rl_map_set(M(map), (rl_str *)key.u.o, v);
    rl_release(key);
    rl_release(v);
}
static void push(rl_value list, rl_value v) { rl_list_push(L(list), v); rl_release(v); }

static void test_numbers(void) {
    SHOW(op(rl_add, rl_int(1), rl_int(2)), "3");
    SHOW(op(rl_div, rl_int(7), rl_int(2)), "3.5");
    SHOW(op(rl_mod, rl_int(7), rl_int(-3)), "-2");
    SHOW(op(rl_mod, rl_int(-7), rl_int(3)), "2");
    SHOW(op(rl_mul, rl_float(2.0), rl_int(3)), "6.0");
    SHOW(op(rl_add, rl_float(0.1), rl_float(0.2)), "0.3");
    SHOW(rl_float(1e20), "1e+20");
    SHOW(op(rl_mul, rl_float(1e300), rl_float(1e10)), "inf");
    SHOW(rl_float(3.0), "3.0");
    SHOW(op(rl_div, rl_int(1), rl_int(3)), "0.333333333333333");
    SHOW(rl_float(-0.0), "-0.0");
    SHOW(rl_float(1e17), "1e+17");
    SHOW(rl_float(1.5e-7), "1.5e-07");
    {
        rl_value r;
        ERR(rl_add(&cx, rl_int(9223372036854775807LL), rl_int(1), &r), "int overflow");
        ERR(rl_mul(&cx, rl_int(4611686018427387904LL), rl_int(2), &r), "int overflow");
        ERR(rl_neg(&cx, rl_int(-9223372036854775807LL - 1), &r), "int overflow");
        ERR(rl_div(&cx, rl_int(1), rl_int(0), &r), "division by zero");
        ERR(rl_mod(&cx, rl_float(1), rl_float(0), &r), "division by zero");
        SHOW(op(rl_mod, rl_int(-9223372036854775807LL - 1), rl_int(-1)), "0");
        CHECK(rl_mul(&cx, rl_int(-3037000499LL), rl_int(3037000499LL), &r) == RL_OK && r.u.i == -9223372030926249001LL);
    }
}

static void test_strings(void) {
    rl_value h = S("h\xc3\xa9llo"), r, a[3];
    SHOW(call1("len", h), "5");
    CHECK(rl_index(&cx, h, rl_int(1), &r) == RL_OK);
    SHOW(r, "\xc3\xa9");
    a[0] = h; a[1] = rl_int(1); a[2] = rl_int(3);
    CHECK(call("slice", 3, a, &r) == RL_OK);
    SHOW(r, "\xc3\xa9l");
    /* a whole float past the int range is the nearest int: the end of a slice is clamped */
    a[0] = T("abc"); a[1] = rl_int(0); a[2] = rl_float(1e300);
    CHECK(call("slice", 3, a, &r) == RL_OK);
    SHOW(r, "abc");
    a[1] = rl_float(-1e300); a[2] = rl_int(2);
    CHECK(call("slice", 3, a, &r) == RL_OK);
    SHOW(r, "ab");
    SHOW(calls("ord", "\xc3\xa9"), "233");
    SHOW(call1("chr", rl_int(233)), "\xc3\xa9");
    SHOW(call1("chr", rl_int(128512)), "\xf0\x9f\x98\x80");
    SHOW(call1("str", rl_int(12)), "12");
    ERR(rl_index(&cx, T("abc"), rl_int(5), &r), "string index 5 is out of range (size 3)");
    rl_release(h);
    h = S("\xff" "a");   /* a bad byte counts as one character */
    SHOW(call1("len", h), "2");
    rl_release(h);
    {
        rl_value x = S("ab"), y = S("cd"), z;
        SHOW(op(rl_add, rl_retain(x), rl_retain(y)), "abcd");
        CHECK(rl_compare(&cx, ">", y, x, &z) == RL_OK && z.u.b == 1);
        CHECK(rl_in(&cx, T("ell"), T("hello"), &z) == RL_OK && z.u.b == 1);
        ERR(rl_in(&cx, rl_int(1), T("x"), &z), "can only look for a string inside a string");
        rl_release(x);
        rl_release(y);
    }
}

/* rl_str_offset against a plain walk, on a long string of mixed widths
 * with bad bytes in it (long enough for the table of marks). */
static void test_string_offsets(void) {
    static const char *pieces[] = {"a", "\xc3\xa9", "\xe2\x82\xac", "\xf0\x9f\x98\x80", "\xff", "\xc3"};
    char buf[4096];
    size_t len = 0, i, chars = 0, at = 0, bad = 0;
    rl_value v, r, a[3];
    rl_str *s;
    for (i = 0; len < 2000; ++i) {
        const char *p = pieces[(i * 7 + i / 3) % 6];
        size_t n = strlen(p);
        memcpy(buf + len, p, n);
        len += n;
    }
    buf[len] = 0;
    v = rl_str_new(buf, len);
    s = (rl_str *)v.u.o;
    for (i = 0; i <= rl_str_chars(s) + 2; ++i) {  /* in order */
        size_t want = at;
        if (rl_str_offset(s, i) != want) ++bad;
        if (at < len) { at += rl_utf8_char_size(buf, len, at); ++chars; }
    }
    CHECK(bad == 0);
    CHECK(chars == rl_str_chars(s));
    CHECK(s->marks != NULL);
    for (i = rl_str_chars(s) + 1; i-- > 0;) {  /* backwards, against a fresh walk */
        size_t k, w = 0;
        for (k = 0; k < i && w < len; ++k) w += rl_utf8_char_size(buf, len, w);
        if (rl_str_offset(s, i) != w) ++bad;
    }
    CHECK(bad == 0);
    CHECK(rl_index(&cx, v, rl_int(1), &r) == RL_OK);  /* a, then e acute, euro, 0xff */
    SHOW(r, "\xc3\xa9");
    a[0] = v; a[1] = rl_int(1); a[2] = rl_int(3);
    CHECK(call("slice", 3, a, &r) == RL_OK);
    SHOW(r, "\xc3\xa9\xe2\x82\xac");
    rl_release(v);
}

static void test_num_parse(void) {
    rl_value r;
    SHOW(calls("num", "42"), "42");
    SHOW(calls("num", "-1.5"), "-1.5");
    SHOW(calls("num", "0x1p4"), "16.0");
    SHOW(calls("num", "inf"), "inf");
    SHOW(calls("num", "9223372036854775808"), "9.22337203685478e+18");
    SHOW(call1("int", rl_float(-3.7)), "-3");
    SHOW(calls("int", "12"), "12");
    SHOW(call1("float", rl_int(3)), "3.0");
    {
        rl_value a = S("12x");
        ERR(call("num", 1, &a, &r), "num(): not a number: \"12x\"");
        rl_release(a);
        a = S(" 1");
        ERR(call("num", 1, &a, &r), "num(): not a number: \" 1\"");
        rl_release(a);
        a = S("1e400");
        ERR(call("num", 1, &a, &r), "num(): not a number: \"1e400\"");
        rl_release(a);
        a = rl_float(1e300);
        ERR(call("int", 1, &a, &r), "int(): number is out of the int range");
    }
}

static void test_containers(void) {
    rl_value l = rl_list_new(0), inner = rl_list_new(0), m = rl_map_new(), r, a[3];
    push(l, rl_int(1));
    push(l, S("a"));
    push(inner, rl_float(2.5));
    push(inner, rl_none());
    push(l, rl_retain(inner));
    set(m, "x", rl_int(1));
    set(m, "a b", S("q\"\n"));
    push(l, rl_retain(m));
    push(l, rl_bool(1));
    SHOW(rl_retain(l), "[1, \"a\", [2.5, none], {x: 1, \"a b\": \"q\\\"\\n\"}, true]");
    rl_release(inner);
    rl_release(m);
    rl_release(l);

    /* order is kept after removes; keys come back in order */
    m = rl_map_new();
    set(m, "b", rl_int(1));
    set(m, "a", rl_int(2));
    set(m, "c", rl_int(3));
    a[0] = m; a[1] = S("a");
    CHECK(call("remove", 2, a, &r) == RL_OK && r.k == RL_INT && r.u.i == 2);
    rl_release(a[1]);
    SHOW(rl_retain(m), "{b: 1, c: 3}");
    SHOW(call1("keys", m), "[\"b\", \"c\"]");
    a[1] = S("zz");
    ERR(call("remove", 2, a, &r), "remove(): map has no key 'zz'");
    rl_release(a[1]);
    ERR(rl_field(&cx, m, "cc", &r), "map has no key 'cc' (did you mean 'c'?)");
    rl_release(m);

    m = rl_map_new();
    set(m, "alpha", rl_int(1));
    ERR(rl_field(&cx, m, "alpah", &r), "map has no key 'alpah' (did you mean 'alpha'?)");
    {
        int i;
        char k[16];
        for (i = 0; i < 1000; ++i) { sprintf(k, "k%d", i); set(m, k, rl_int(i)); }
        CHECK(M(m)->n == 1001);
        CHECK(rl_map_get(M(m), "k777", 4)->u.i == 777);
        for (i = 0; i < 1000; i += 2) {
            rl_value key, old;
            sprintf(k, "k%d", i);
            key = S(k);
            CHECK(rl_map_remove(M(m), k, strlen(k), &old));
            rl_release(old);
            rl_release(key);
        }
        CHECK(M(m)->n == 501 && rl_map_get(M(m), "k999", 4) && !rl_map_get(M(m), "k998", 4));
    }
    rl_release(m);

    l = rl_list_new(0);
    push(l, rl_int(1)); push(l, rl_int(2)); push(l, rl_int(3));
    SHOW(call1("pop", l), "3");
    a[0] = l; a[1] = rl_int(0); a[2] = rl_int(9);
    CHECK(call("insert", 3, a, &r) == RL_OK);
    SHOW(rl_retain(l), "[9, 1, 2]");
    a[1] = rl_int(1);
    SHOW((call("remove", 2, a, &r), r), "1");
    SHOW(rl_retain(l), "[9, 2]");
    ERR(rl_index(&cx, l, rl_int(3), &r), "list index 3 is out of range (size 2)");
    ERR(rl_index(&cx, l, rl_float(0.5), &r), "list index must be a whole number");
    a[1] = rl_int(5);
    ERR(call("insert", 3, a, &r), "insert(): index 5 is out of range");
    a[1] = rl_float(1e30);
    ERR(call("insert", 3, a, &r), "insert(): index 9223372036854775807 is out of range");
    a[1] = rl_float(-1e30);
    ERR(call("remove", 2, a, &r), "remove(): index -9223372036854775808 is out of range");
    CHECK(rl_set_index(&cx, l, rl_int(0), T("x")) == RL_OK);
    SHOW(rl_retain(l), "[\"x\", 2]");
    CHECK(rl_set_index(&cx, l, rl_int(0), rl_int(0)) == RL_OK);
    {
        rl_value l2 = rl_list_new(0), eq;
        push(l2, rl_int(0)); push(l2, rl_float(2.0));
        CHECK(rl_equal(l, l2));
        CHECK(rl_in(&cx, rl_float(2.0), l, &eq) == RL_OK && eq.u.b);
        rl_release(l2);
    }
    SHOW(op(rl_add, rl_retain(l), rl_retain(l)), "[0, 2, 0, 2]");
    rl_release(l);

    CHECK(rl_range(&cx, rl_int(2), rl_int(5), &r) == RL_OK);
    SHOW(r, "[2, 3, 4]");
    ERR(rl_range(&cx, rl_int(0), rl_int(20000000), &r), "range is too large");
    CHECK(rl_loop_items(&cx, T("h\xc3\xa9"), &r) == RL_OK);
    SHOW(r, "[\"h\", \"\xc3\xa9\"]");
}

static void test_errors_and_builtins(void) {
    rl_value r, a[1], f;
    ERR(rl_add(&cx, rl_int(1), T("a"), &r), "cannot add an int and a string");
    ERR(rl_sub(&cx, T("a"), rl_none(), &r), "cannot subtract a string and a none");
    ERR(rl_neg(&cx, T("a"), &r), "cannot negate a string");
    ERR(rl_not(&cx, rl_int(1), &r), "'not' needs true or false, got int");
    a[0] = rl_int(5);
    ERR(call("len", 1, a, &r), "len(): cannot get the length of an int");
    a[0] = S("boom");
    ERR(call("fail", 1, a, &r), "boom");
    rl_release(a[0]);
    a[0] = rl_int(55296);
    ERR(call("chr", 1, a, &r), "chr(): code must be 0 to 1114111 and not 55296 to 57343 (surrogates)");
    a[0] = S("x");
    ERR(call("host_floor", 1, a, &r), "host_floor(): argument must be a number, got string");
    rl_release(a[0]);
    a[0] = S("/nonexistent/x");
    ERR(call("host_read_file", 1, a, &r), "host_read_file(): cannot open /nonexistent/x");
    rl_release(a[0]);
    a[0] = S(".");   /* a folder opens on Linux and reads as nothing; it must not read */
    ERR(call("host_read_file", 1, a, &r), "host_read_file(): cannot open .");
    rl_release(a[0]);
    ERR(call("len", 0, a, &r), "len() takes 1 argument, got 0");
    ERR(rl_call(&cx, rl_int(3), 0, NULL, &r), "cannot call an int");
    ERR(rl_cond(&cx, rl_int(1), "'if' condition", (int *)&a[0].u.b), "'if' condition must be true or false, got int");
    ERR(rl_check_type(&cx, "int", T("x"), "parameter 'n' must be"), "parameter 'n' must be int, got string");
    CHECK(rl_check_type(&cx, "number", rl_float(1), "x") == RL_OK);

    /* a positioned error and rl_catch's map */
    rl_fail(&cx, "boom");
    rl_at(&cx, "t.relic", 3, 9);
    rl_at(&cx, "t.relic", 1, 1);   /* the first position wins */
    CHECK(rl_catch(&cx, &r));
    SHOW(r, "{message: \"boom\", line: 3, col: 9}");
    CHECK(!cx.has_err);

    /* the report matches the host's: header, source line, caret */
    {
        FILE *tf = tmpfile();
        char got[256];
        size_t n;
        rl_fail(&cx, "cannot add an int and a string");
        rl_at(&cx, "e.relic", 2, 13);
        rl_report(&cx, tf, "let x = 1\n\tprint(\"\xc3\xa9\", x + \"a\")\r\n");
        rewind(tf);
        n = fread(got, 1, sizeof got - 1, tf);
        got[n] = 0;
        fclose(tf);
        CHECK(strcmp(got, "e.relic:2:13: error: cannot add an int and a string\n"
                          "    \tprint(\"\xc3\xa9\", x + \"a\")\n    \t          ^\n") == 0);
        CHECK(rl_catch(&cx, &r));
        rl_release(r);
    }

    CHECK(rl_builtin("print", &f));
    CHECK(rl_builtin("print", &r) && r.u.o == f.u.o);   /* one shared object */
    SHOW(rl_retain(f), "<fun print>");
    rl_release(f);
    rl_release(r);
    SHOW(call1("type", rl_none()), "none");
    SHOW(call1("type", rl_float(1.5)), "float");
    CHECK(rl_builtin("host_names", &f));
    CHECK(rl_call(&cx, f, 0, NULL, &r) == RL_OK && L(r)->n == 77);
    rl_release(r);
    rl_release(f);
    SHOW((call("host_gfx_available", 0, NULL, &r), r), "false");   /* relic_rt has the stub */
    ERR(call("host_gfx_open", 0, NULL, &r), "host_gfx_open() takes 3 arguments, got 0");
}

static void test_host(void) {
    rl_value r, a[3];
    const char *dir = "rl_test_tmp";
    a[0] = S(dir);
    CHECK(call("host_make_dir", 1, a, &r) == RL_OK);
    SHOW(call1("host_is_dir", a[0]), "true");
    rl_release(a[0]);
    a[0] = S("rl_test_tmp/b.txt"); a[1] = S("hi"); a[2] = rl_bool(0);
    CHECK(call("host_write_file", 3, a, &r) == RL_OK);
    rl_release(a[1]);
    a[1] = S("!"); a[2] = rl_bool(1);
    CHECK(call("host_write_file", 3, a, &r) == RL_OK);
    rl_release(a[1]);
    SHOW(call1("host_read_file", a[0]), "hi!");
    SHOW(call1("host_file_size", a[0]), "3");
    SHOW(call1("host_file_exists", a[0]), "true");
    a[1] = S("rl_test_tmp/a.txt");
    CHECK(call("host_rename", 2, a, &r) == RL_OK);
    rl_release(a[0]);
    a[0] = S(dir);
    SHOW(call1("host_list_dir", a[0]), "[\"a.txt\"]");
    {
        /* several names come back sorted by bytes */
        rl_value w[3];
        const char *more[2] = {"rl_test_tmp/c.txt", "rl_test_tmp/Z.txt"};
        int k;
        for (k = 0; k < 2; ++k) {
            w[0] = S(more[k]); w[1] = S(""); w[2] = rl_bool(0);
            CHECK(call("host_write_file", 3, w, &r) == RL_OK);
            rl_release(w[1]);
            rl_release(w[0]);
        }
        SHOW(call1("host_list_dir", a[0]), "[\"Z.txt\", \"a.txt\", \"c.txt\"]");
        for (k = 0; k < 2; ++k) {
            w[0] = S(more[k]);
            CHECK(call("host_remove", 1, w, &r) == RL_OK);
            rl_release(w[0]);
        }
    }
    CHECK(call("host_remove", 1, &a[1], &r) == RL_OK);
    CHECK(call("host_remove", 1, &a[0], &r) == RL_OK);
    SHOW(call1("host_file_exists", a[0]), "false");
    ERR(call("host_remove", 1, &a[0], &r), "host_remove(): cannot remove rl_test_tmp");
    ERR(call("host_list_dir", 1, &a[0], &r), "host_list_dir(): cannot list rl_test_tmp");
    rl_release(a[0]);
    rl_release(a[1]);
    a[0] = rl_int(2); a[1] = rl_int(10);
    CHECK(call("host_pow", 2, a, &r) == RL_OK);
    SHOW(r, "1024.0");
    SHOW(call1("host_sqrt", rl_int(16)), "4.0");
    SHOW(call1("host_floor", rl_float(2.7)), "2.0");
    {
        rl_value t0, t1;
        CHECK(call("host_clock", 0, NULL, &t0) == RL_OK);
        a[0] = rl_int(5);
        CHECK(call("host_sleep", 1, a, &r) == RL_OK);
        CHECK(call("host_clock", 0, NULL, &t1) == RL_OK && t1.u.f - t0.u.f >= 0.004);
        CHECK(call("host_now", 0, NULL, &t0) == RL_OK && t0.u.f > 1.7e9);
        CHECK(call("host_cwd", 0, NULL, &r) == RL_OK && r.k == RL_STR && strchr(SD(r), '\\') == NULL);
        rl_release(r);
    }
    SHOW(call("args", 0, NULL, &r) == RL_OK ? r : rl_none(), "[\"one\", \"two\"]");
}

/* ---- cycles, deep nesting, leaks ---- */

static void test_cycles(void) {
    size_t before = rl_live_objects(), freed;
    rl_value l = rl_list_new(0), m = rl_map_new(), keep = rl_list_new(0), r;
    push(l, rl_retain(l));                 /* a list that holds itself */
    SHOW(rl_retain(l), "[[...]]");
    set(m, "me", rl_retain(m));            /* a map that holds itself */
    push(keep, rl_retain(keep));           /* a cycle still held from outside */
    rl_release(l);
    rl_release(m);
    CHECK(rl_live_objects() == before + 3);
    freed = rl_gc_collect();
    CHECK(freed == 2);
    CHECK(rl_live_objects() == before + 1);
    SHOW(rl_retain(keep), "[[...]]");
    rl_release(keep);
    CHECK(rl_gc_collect() == 1);

    /* a closure that holds itself through its scope: fun in a slot of its env */
    {
        rl_scope *s = rl_scope_new(NULL, 1);
        rl_value f = rl_fun_new("bump", NULL, 0, s);
        s->slots[0] = rl_retain(f);
        rl_scope_release(s);
        rl_release(f);
        CHECK(rl_live_objects() == before + 2);
        CHECK(rl_gc_collect() == 2);
    }
    CHECK(rl_live_objects() == before);

    /* the call-site collector runs when there are many containers */
    {
        int i;
        for (i = 0; i < 250000; ++i) {
            rl_value x = rl_list_new(0);
            push(x, rl_retain(x));
            rl_release(x);
            if (i % 1000 == 0) rl_gc_maybe();
        }
        CHECK(rl_live_objects() < before + 200001);
        rl_gc_collect();
        CHECK(rl_live_objects() == before);
    }

    /* a list nested a million deep frees without recursing */
    {
        int i;
        rl_value top = rl_list_new(0);
        for (i = 0; i < 1000000; ++i) {
            rl_value next = rl_list_new(1);
            push(next, top);
            top = next;
        }
        rl_release(top);
        CHECK(rl_live_objects() == before);
    }
    (void)r;
}

/* Calls generated code (cgen) uses on top of the C1 ABI. */
static void test_codegen_support(void) {
    rl_value r, l, parts[3], one = rl_int(1), unset = rl_unset();
    const char *names[2] = {"total", "count"};
    const rl_value *slots[2];
    /* rl_at3 picks the operand the failing call named: index errors point at
       the index, range errors at the bad end, others at the node. */
    l = rl_list_new(0);
    CHECK(rl_index(&cx, l, rl_int(4), &r) == RL_ERR);
    rl_at3(&cx, "t.relic", 0, 1, 5, 1, 1, 1, 7);
    CHECK(rl_catch(&cx, &r));
    SHOW(r, "{message: \"list index 4 is out of range (size 0)\", line: 1, col: 7}");
    rl_release(l);
    CHECK(rl_add(&cx, rl_int(1), T("a"), &r) == RL_ERR);
    rl_at3(&cx, "t.relic", 0, 2, 3, 2, 1, 2, 5);
    CHECK(rl_catch(&cx, &r));
    SHOW(r, "{message: \"cannot add an int and a string\", line: 2, col: 3}");
    /* undefined names suggest a close visible name or built-in; an unset
       slot (a `let` not reached yet) is not offered */
    slots[0] = &one;
    slots[1] = &unset;
    ERR(rl_fail_undefined(&cx, "totl", 2, names, slots), "'totl' is not defined (did you mean 'total'?)");
    ERR(rl_fail_undefined(&cx, "coutn", 2, names, slots), "'coutn' is not defined");
    ERR(rl_fail_undefined(&cx, "prnt", 0, NULL, NULL), "'prnt' is not defined (did you mean 'print'?)");
    /* string interpolation joins shown values */
    parts[0] = T("n=");
    parts[1] = rl_int(3);
    parts[2] = T("!");
    rl_interp(3, parts, &r);
    SHOW(r, "n=3!");
    CHECK(unset.k == RL_UNSET);

    /* a compiled module: names are its scope's slots, unset ones not there yet */
    {
        static const char *const nm[2] = {"pi", "later"};
        rl_scope *sc = rl_scope_new(NULL, 2);
        rl_value m = rl_module_scoped("geo", sc, 2, nm), f, list_fn, arg;
        sc->slots[0] = rl_float(3.5);
        CHECK(rl_field(&cx, m, "pi", &r) == RL_OK && r.u.f == 3.5);
        ERR(rl_field(&cx, m, "later", &r), "module 'geo' has no 'later' (yet)");
        ERR(rl_field(&cx, m, "_x", &r), "'_x' is private to module 'geo'");
        SHOW(rl_retain(m), "<module geo>");
        rl_scope_release(sc);
        rl_release(m);

        /* the std boundary: a host_ error inside std/files' list() gets the
           std name, and the position is cleared for the caller to set */
        f = rl_fun_std(rl_fun_new("list", NULL, 1, NULL), "files");
        rl_builtin("host_list_dir", &list_fn);
        arg = S("no_such_dir_rl");
        CHECK(rl_call(&cx, list_fn, 1, &arg, &r) == RL_ERR);
        rl_at3(&cx, "/std/files.relic", 1, 4, 5, 4, 5, 4, 5);
        rl_std_boundary(&cx, f);
        CHECK(cx.err_line == 0);
        rl_at3(&cx, "main.relic", 0, 2, 7, 2, 7, 2, 7);
        CHECK(rl_catch(&cx, &r));
        SHOW(r, "{message: \"files.list(): cannot list no_such_dir_rl\", line: 2, col: 7}");
        /* an error from user code (not in std) is left alone */
        CHECK(rl_fail(&cx, "boom") == RL_ERR);
        rl_at3(&cx, "main.relic", 0, 9, 1, 9, 1, 9, 1);
        rl_std_boundary(&cx, f);
        CHECK(rl_catch(&cx, &r));
        SHOW(r, "{message: \"boom\", line: 9, col: 1}");
        rl_release(arg);
        rl_release(list_fn);
        rl_release(f);
    }
}

/* The high-DPI math (src/builtins/gfx_scale.h). Headless SDL always has a
 * scale of 1, so the other scales are checked here. */
static void test_gfx_scale(void) {
    CHECK(gfx_scale_of(1600, 800) == 2.0);
    CHECK(gfx_scale_of(1500, 1000) == 1.5);
    CHECK(gfx_scale_of(800, 800) == 1.0);
    CHECK(gfx_scale_of(0, 800) == 1.0 && gfx_scale_of(800, 0) == 1.0);
    CHECK(gfx_font_pixels(18, 1.0) == 18);
    CHECK(gfx_font_pixels(18, 2.0) == 36);
    CHECK(gfx_font_pixels(18, 1.5) == 27);
    CHECK(gfx_font_pixels(7, 1.25) == 9);      /* 8.75 rounds to 9 */
    CHECK(gfx_font_pixels(1, 0.25) == 1);      /* never below 1 */
    CHECK(gfx_units_up(10, 1.0) == 10);
    CHECK(gfx_units_up(36, 2.0) == 18);
    CHECK(gfx_units_up(27, 1.5) == 18);
    CHECK(gfx_units_up(25, 1.5) == 17);        /* 16.67 rounds up */
    CHECK(gfx_units_up(0, 2.0) == 0);
}

/* The small public calls the other tests only reach through built-ins:
 * type names, repr against show, fields, range starts and error strings. */
static void test_small_api(void) {
    rl_value r, l = rl_list_new(0), m = rl_map_new();
    CHECK(strcmp(rl_type_name(rl_int(1)), "int") == 0);
    CHECK(strcmp(rl_type_name(rl_float(1)), "float") == 0);
    CHECK(strcmp(rl_type_name(rl_none()), "none") == 0);
    CHECK(strcmp(rl_type_name(rl_bool(0)), "bool") == 0);
    CHECK(strcmp(rl_type_name(l), "list") == 0);
    CHECK(strcmp(rl_type_name(m), "map") == 0);
    CHECK(strcmp(rl_type_name(T("a")), "string") == 0);
    CHECK(strcmp(rl_a_type(rl_int(1)), "an int") == 0);
    CHECK(strcmp(rl_a_type(rl_float(1)), "a float") == 0);
    CHECK(strcmp(rl_a_type(rl_none()), "a none") == 0);
    CHECK(strcmp(rl_a_type(l), "a list") == 0);
    CHECK(strcmp(rl_a_type(T("a")), "a string") == 0);

    /* show prints a string as it is, repr quotes it (as inside a list) */
    SHOW(rl_show(T("a\"b")), "a\"b");
    SHOW(rl_repr(T("a\"b")), "\"a\\\"b\"");
    SHOW(rl_repr(rl_int(5)), "5");

    /* fields on a map; none anywhere else */
    CHECK(rl_set_field(&cx, m, "k", rl_int(1)) == RL_OK);
    CHECK(rl_set_field(&cx, m, "k", rl_int(2)) == RL_OK);   /* replaces */
    SHOW(rl_retain(m), "{k: 2}");
    ERR(rl_set_field(&cx, rl_int(5), "k", rl_int(1)), "cannot set a field on an int");
    ERR(rl_set_field(&cx, l, "k", rl_int(1)), "cannot set a field on a list");
    ERR(rl_set_field(&cx, T("s"), "k", rl_int(1)), "cannot set a field on a string");
    {
        rl_value mod = rl_module_new("m");
        ERR(rl_set_field(&cx, mod, "k", rl_int(1)),
            "cannot change a module's names from outside; give it a function that does it");
        ERR(rl_field(&cx, mod, "k", &r), "module 'm' has no 'k' (yet)");
        rl_release(mod);
    }

    /* a range start is checked on its own (the for loop does it first) */
    CHECK(rl_range_start(&cx, rl_int(3)) == RL_OK);
    CHECK(rl_range_start(&cx, rl_float(3.0)) == RL_OK);
    ERR(rl_range_start(&cx, rl_float(1.5)), "range start must be a whole number");
    ERR(rl_range_start(&cx, T("a")), "range start must be a number, got string");

    /* the number reader, called directly */
    CHECK(rl_parse_num(&cx, "12", 2, &r) == RL_OK && r.k == RL_INT && r.u.i == 12);
    CHECK(rl_parse_num(&cx, "-7", 2, &r) == RL_OK && r.k == RL_INT && r.u.i == -7);
    CHECK(rl_parse_num(&cx, "1.5", 3, &r) == RL_OK && r.k == RL_FLOAT && r.u.f == 1.5);
    ERR(rl_parse_num(&cx, "12abc", 5, &r), "not a number: \"12abc\"");
    ERR(rl_parse_num(&cx, "", 0, &r), "not a number: \"\"");

    /* an error made from a string object */
    {
        rl_value msg = S("made by hand");
        CHECK(rl_fail_str(&cx, (rl_str *)msg.u.o) == RL_ERR);
        rl_release(msg);
        ERR(RL_ERR, "made by hand");
    }

    /* rl_obj_value wraps an object the caller owns */
    {
        rl_value s = rl_str_c("wrapped");
        rl_value w = rl_obj_value((rl_obj *)s.u.o);
        CHECK(w.k == s.k && w.u.o == s.u.o);
        SHOW(rl_retain(w), "wrapped");
        rl_release(w);   /* the one reference, s was not retained for it */
    }
    rl_release(l);
    rl_release(m);
    drop_temps();
}

/* Operator errors and the small helpers generated code leans on. The
 * messages are the host's (checked against `relic` 1.0.25). */
static void test_operator_errors(void) {
    rl_value r, l = rl_list_new(0), m = rl_map_new(), none = rl_none();
    int truth = -1;
    push(l, rl_int(1));

    ERR(rl_not(&cx, rl_int(1), &r), "'not' needs true or false, got int");
    CHECK(rl_not(&cx, rl_bool(0), &r) == RL_OK && r.k == RL_BOOL && r.u.b == 1);
    CHECK(rl_not(&cx, rl_bool(1), &r) == RL_OK && r.k == RL_BOOL && r.u.b == 0);

    ERR(rl_sub(&cx, rl_int(1), T("a"), &r), "cannot subtract an int and a string");
    ERR(rl_sub(&cx, T("a"), rl_int(1), &r), "cannot subtract a string and an int");
    ERR(rl_sub(&cx, rl_int(1), none, &r), "cannot subtract an int and a none");
    ERR(rl_sub(&cx, rl_float(2.5), l, &r), "cannot subtract a float and a list");
    ERR(rl_mul(&cx, rl_int(1), T("a"), &r), "cannot multiply an int and a string");
    ERR(rl_mul(&cx, l, rl_int(2), &r), "cannot multiply a list and an int");
    ERR(rl_neg(&cx, none, &r), "cannot negate a none");
    ERR(rl_neg(&cx, T("a"), &r), "cannot negate a string");
    ERR(rl_mod(&cx, rl_int(5), rl_int(0), &r), "division by zero");
    ERR(rl_mod(&cx, rl_int(5), rl_float(0.0), &r), "division by zero");
    ERR(rl_div(&cx, rl_float(5.0), rl_int(0), &r), "division by zero");

    ERR(rl_compare(&cx, "<", none, rl_int(1), &r), "cannot compare a none and an int");
    ERR(rl_compare(&cx, "<=", l, l, &r), "cannot compare a list and a list");
    ERR(rl_compare(&cx, ">=", rl_int(1), T("a"), &r), "cannot compare an int and a string");
    ERR(rl_compare(&cx, ">", m, m, &r), "cannot compare a map and a map");
    ERR(rl_compare(&cx, "<", rl_bool(1), rl_bool(0), &r), "cannot compare a bool and a bool");
    CHECK(rl_compare(&cx, "<", rl_int(1), rl_float(1.5), &r) == RL_OK && r.u.b == 1);
    CHECK(rl_compare(&cx, ">=", T("b"), T("a"), &r) == RL_OK && r.u.b == 1);
    CHECK(rl_compare(&cx, "<", T("B"), T("a"), &r) == RL_OK && r.u.b == 1);   /* by code point */

    /* == never fails: mixed types are just unequal, 1 == 1.0, none only equals none */
    CHECK(rl_equal(rl_int(1), rl_float(1.0)));
    CHECK(!rl_equal(T("1"), rl_int(1)));
    CHECK(rl_equal(none, none));
    CHECK(!rl_equal(none, rl_bool(0)));
    CHECK(rl_equal(T("ab"), T("ab")));
    CHECK(rl_equal(l, l));
    CHECK(!rl_equal(l, m));

    /* conditions */
    CHECK(rl_cond(&cx, rl_bool(1), "'if' condition", &truth) == RL_OK && truth == 1);
    CHECK(rl_cond(&cx, rl_bool(0), "'if' condition", &truth) == RL_OK && truth == 0);
    ERR(rl_cond(&cx, T("s"), "'if' condition", &truth), "'if' condition must be true or false, got string");
    ERR(rl_cond(&cx, none, "'while' condition", &truth), "'while' condition must be true or false, got none");

    /* a loop walks a copy of a list, the characters of a string, the keys of a map */
    CHECK(rl_loop_items(&cx, l, &r) == RL_OK && r.k == RL_LIST && r.u.o != l.u.o && L(r)->n == 1);
    rl_release(r);
    CHECK(rl_loop_items(&cx, T("hé"), &r) == RL_OK);
    SHOW(r, "[\"h\", \"é\"]");
    set(m, "k", rl_int(1));
    CHECK(rl_loop_items(&cx, m, &r) == RL_OK);
    SHOW(r, "[\"k\"]");
    ERR(rl_loop_items(&cx, none, &r), "cannot loop over a none");
    ERR(rl_loop_items(&cx, rl_int(5), &r), "cannot loop over an int");

    /* hints: the message is the caller's prefix, then "<type>, got <type>" */
    CHECK(rl_check_type(&cx, "int", rl_int(1), "'n' must be") == RL_OK);
    CHECK(rl_check_type(&cx, "number", rl_float(1.5), "'n' must be") == RL_OK);
    CHECK(rl_check_type(&cx, "any", none, "'n' must be") == RL_OK);
    ERR(rl_check_type(&cx, "int", T("a"), "'n' must be"), "'n' must be int, got string");
    ERR(rl_check_type(&cx, "float", rl_int(1), "'n' must be"), "'n' must be float, got int");
    ERR(rl_check_type(&cx, "list", m, "'n' must be"), "'n' must be list, got map");

    /* removing a map key hands the value back, once */
    CHECK(rl_map_remove(M(m), "k", 1, &r) && r.k == RL_INT && r.u.i == 1);
    CHECK(!rl_map_remove(M(m), "k", 1, &r));
    SHOW(rl_retain(m), "{}");

    /* the built-in table */
    CHECK(!rl_builtin("no_such_builtin", &r));
    CHECK(rl_builtin("len", &r) && r.k == RL_FUN);
    rl_release(r);

    rl_release(l);
    rl_release(m);
    drop_temps();
}

int main(void) {
    char *argv[2];
    argv[0] = (char *)"one";
    argv[1] = (char *)"two";
    rl_ctx_init(&cx, 2, argv);
    test_numbers();
    test_strings();
    test_string_offsets();
    test_num_parse();
    test_containers();
    test_errors_and_builtins();
    test_codegen_support();
    test_host();
    test_cycles();
    test_gfx_scale();
    test_small_api();
    test_operator_errors();
    drop_temps();
    CHECK(!cx.has_err);
    rl_ctx_free(&cx);
    CHECK(rl_live_objects() == 0);
    CHECK(rl_live_strings() == 0);
    if (rl_live_objects() || rl_live_strings())
        fprintf(stderr, "leak: %lu containers, %lu strings\n", (unsigned long)rl_live_objects(),
                (unsigned long)rl_live_strings());
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
