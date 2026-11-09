/* Boundary and ownership regressions for the portable runtime. */
#include "../relic.h"

#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define S(v) ((rl_str *)(v).u.o)
#define L(v) ((rl_list *)(v).u.o)
#define M(v) ((rl_map *)(v).u.o)

static rl_ctx cx;
static int checks;

/* Stop on failure so later assertions never inspect an invalid result. */
#define CHECK(c) do { ++checks; if (!(c)) { \
    fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #c); exit(1); \
} } while (0)

static void error_is(int status, const char *message) {
    rl_value caught;
    CHECK(status == RL_ERR && cx.has_err);
    CHECK(cx.err_msg->len == strlen(message));
    CHECK(memcmp(cx.err_msg->data, message, strlen(message)) == 0);
    CHECK(rl_catch(&cx, &caught));
    rl_release(caught);
}

static void test_range_limits(void) {
    rl_value r;
    error_is(rl_range(&cx, rl_int(LLONG_MIN), rl_int(LLONG_MAX), &r), "range is too large");
    CHECK(r.k == RL_NONE);
    CHECK(rl_range(&cx, rl_int(LLONG_MAX), rl_int(LLONG_MIN), &r) == RL_OK);
    CHECK(L(r)->n == 0);
    rl_release(r);
    CHECK(rl_range(&cx, rl_int(LLONG_MAX - 2), rl_int(LLONG_MAX), &r) == RL_OK);
    CHECK(L(r)->n == 2 && L(r)->items[0].u.i == LLONG_MAX - 2
          && L(r)->items[1].u.i == LLONG_MAX - 1);
    rl_release(r);
    CHECK(rl_range(&cx, rl_int(LLONG_MIN), rl_int(LLONG_MIN + 2), &r) == RL_OK);
    CHECK(L(r)->n == 2 && L(r)->items[1].u.i == LLONG_MIN + 1);
    rl_release(r);
}

static void test_error_message_alias(void) {
    size_t before = rl_live_strings();
    rl_value message = rl_str_c("retry"), caught, field;
    CHECK(rl_fail_str(&cx, S(message)) == RL_ERR);
    rl_release(message);
    rl_at3(&cx, "first.relic", 1, 3, 4, 3, 4, 3, 4);
    CHECK(rl_fail_str(&cx, cx.err_msg) == RL_ERR);
    CHECK(cx.err_line == 0 && cx.err_col == 0 && !cx.err_file && !cx.err_std);
    CHECK(cx.err_msg->len == 5 && memcmp(cx.err_msg->data, "retry", 5) == 0);
    CHECK(rl_catch(&cx, &caught));
    CHECK(rl_field(&cx, caught, "message", &field) == RL_OK);
    rl_release(caught);
    CHECK(S(field)->len == 5 && memcmp(S(field)->data, "retry", 5) == 0);
    rl_release(field);
    CHECK(rl_live_strings() == before);
}

static void test_number_spans(void) {
    const char raw[2] = {'4', '2'};
    const char prefix[] = "-12.5junk";
    const char binary[3] = {'1', 0, '2'};
    char long_number[300];
    rl_value r;
    CHECK(rl_parse_num(&cx, raw, sizeof raw, &r) == RL_OK);
    CHECK(r.k == RL_INT && r.u.i == 42);
    CHECK(rl_parse_num(&cx, prefix, 5, &r) == RL_OK);
    CHECK(r.k == RL_FLOAT && r.u.f == -12.5);
    CHECK(rl_parse_num(&cx, binary, sizeof binary, &r) == RL_ERR);
    CHECK(r.k == RL_NONE && cx.err_msg->len == 19);
    CHECK(memcmp(cx.err_msg->data, "not a number: \"1\0" "2\"", 19) == 0);
    CHECK(rl_catch(&cx, &r));
    rl_release(r);
    memset(long_number, '0', sizeof long_number);
    long_number[sizeof long_number - 1] = '7';
    CHECK(rl_parse_num(&cx, long_number, sizeof long_number, &r) == RL_OK);
    CHECK(r.k == RL_INT && r.u.i == 7);
    CHECK(rl_parse_num(&cx, "1e-400", 6, &r) == RL_OK);
    CHECK(r.k == RL_FLOAT && r.u.f == 0.0);
}

static void test_binary_std_error(void) {
    static const char message[] = "host_read_file(): missing\0suffix";
    static const char expected[] = "files.read(): missing\0suffix";
    rl_value f = rl_fun_std(rl_fun_new("read", NULL, 1, NULL), "files");
    rl_value text = rl_str_new(message, sizeof message - 1), caught, field;
    CHECK(rl_fail_str(&cx, S(text)) == RL_ERR);
    rl_release(text);
    rl_at3(&cx, "std/files.relic", 1, 7, 8, 7, 8, 7, 8);
    rl_std_boundary(&cx, f);
    CHECK(cx.err_msg->len == sizeof expected - 1);
    CHECK(memcmp(cx.err_msg->data, expected, sizeof expected - 1) == 0);
    CHECK(cx.err_line == 0 && cx.err_std == 0);
    rl_at(&cx, "caller.relic", 2, 3);
    CHECK(rl_catch(&cx, &caught));
    CHECK(rl_field(&cx, caught, "message", &field) == RL_OK);
    CHECK(S(field)->len == sizeof expected - 1);
    rl_release(field);
    rl_release(caught);
    rl_release(f);
}

static void test_report_without_source(void) {
    FILE *stream = tmpfile();
    char bytes[128];
    size_t n;
    rl_value caught;
    CHECK(stream != NULL);
    CHECK(rl_fail(&cx, "missing source") == RL_ERR);
    rl_at(&cx, "rt_hardening_absent/source.relic", 2, 4);
    rl_report(&cx, stream, NULL);
    rewind(stream);
    n = fread(bytes, 1, sizeof bytes, stream);
    CHECK(n == strlen("rt_hardening_absent/source.relic:2:4: error: missing source\n"));
    CHECK(memcmp(bytes, "rt_hardening_absent/source.relic:2:4: error: missing source\n", n) == 0);
    fclose(stream);
    CHECK(rl_catch(&cx, &caught));
    rl_release(caught);
}

static int call(const char *name, int argc, const rl_value *argv, rl_value *out) {
    rl_value f;
    int status;
    CHECK(rl_builtin(name, &f));
    status = rl_call(&cx, f, argc, argv, out);
    rl_release(f);
    return status;
}

static void test_file_write_errors(void) {
    rl_value args[3], out;
    args[0] = rl_str_c("rt_hardening_absent/file.bin");
    args[1] = rl_str_c("data");
    args[2] = rl_bool(0);
    error_is(call("host_write_file", 3, args, &out),
             "host_write_file(): cannot write rt_hardening_absent/file.bin");
    CHECK(out.k == RL_NONE);
    rl_release(args[0]);
    rl_release(args[1]);
#if defined(__linux__)
    /* /dev/full opens successfully but rejects both buffered flushes and
     * larger writes. Other platforms still run the open-failure case. */
    {
        char large[8192];
        memset(large, 'x', sizeof large);
        args[0] = rl_str_c("/dev/full");
        args[1] = rl_str_c("x");
        error_is(call("host_write_file", 3, args, &out), "host_write_file(): cannot write /dev/full");
        CHECK(out.k == RL_NONE);
        rl_release(args[1]);
        args[1] = rl_str_new(large, sizeof large);
        error_is(call("host_write_file", 3, args, &out), "host_write_file(): cannot write /dev/full");
        CHECK(out.k == RL_NONE);
        rl_release(args[0]);
        rl_release(args[1]);
    }
#endif
}

static void test_integer_overflow_branches(void) {
    typedef int (*binary_op)(rl_ctx *, rl_value, rl_value, rl_value *);
    static const struct {
        binary_op op;
        long long a, b, result;
        int overflow;
    } cases[] = {
        {rl_add, LLONG_MIN, -1, 0, 1},
        {rl_add, LLONG_MIN, LLONG_MAX, -1, 0},
        {rl_add, LLONG_MAX, -1, LLONG_MAX - 1, 0},
        {rl_sub, LLONG_MAX, -1, 0, 1},
        {rl_sub, LLONG_MIN, 1, 0, 1},
        {rl_sub, -1, LLONG_MIN, LLONG_MAX, 0},
        {rl_sub, LLONG_MIN, LLONG_MIN, 0, 0},
        {rl_mul, -1, LLONG_MIN, 0, 1},
        {rl_mul, LLONG_MIN, -1, 0, 1},
        {rl_mul, LLONG_MIN, 2, 0, 1},
        {rl_mul, 2, LLONG_MIN, 0, 1},
        {rl_mul, -3037000500LL, -3037000500LL, 0, 1},
        {rl_mul, -3037000499LL, -3037000499LL, 9223372030926249001LL, 0},
        {rl_mul, LLONG_MIN, 1, LLONG_MIN, 0},
        {rl_mul, LLONG_MIN, 0, 0, 0},
        {rl_mul, 0, LLONG_MIN, 0, 0}
    };
    size_t i;
    for (i = 0; i < sizeof cases / sizeof cases[0]; ++i) {
        rl_value out = rl_int(123);
        int status = cases[i].op(&cx, rl_int(cases[i].a), rl_int(cases[i].b), &out);
        if (cases[i].overflow) {
            error_is(status, "int overflow");
            CHECK(out.k == RL_NONE);
        } else {
            CHECK(status == RL_OK && out.k == RL_INT && out.u.i == cases[i].result);
        }
    }
}

static void test_division_modulo_limits(void) {
    static const struct { long long a, b, result; } cases[] = {
        {LLONG_MIN, LLONG_MAX, LLONG_MAX - 1},
        {LLONG_MAX, LLONG_MIN, -1},
        {LLONG_MIN + 1, LLONG_MIN, LLONG_MIN + 1},
        {-6, -3, 0}, {7, 3, 1}, {-7, -3, -1}
    };
    size_t i;
    rl_value r;
    for (i = 0; i < sizeof cases / sizeof cases[0]; ++i) {
        CHECK(rl_mod(&cx, rl_int(cases[i].a), rl_int(cases[i].b), &r) == RL_OK);
        CHECK(r.k == RL_INT && r.u.i == cases[i].result);
    }
    CHECK(rl_div(&cx, rl_int(LLONG_MIN), rl_int(-1), &r) == RL_OK);
    CHECK(r.k == RL_FLOAT && r.u.f == 9223372036854775808.0);
    CHECK(rl_mod(&cx, rl_float(-7.5), rl_int(3), &r) == RL_OK && r.u.f == 1.5);
    CHECK(rl_mod(&cx, rl_int(7), rl_float(-3.0), &r) == RL_OK && r.u.f == -2.0);
    CHECK(rl_mod(&cx, rl_float(-6.0), rl_float(3.0), &r) == RL_OK);
    CHECK(r.k == RL_FLOAT && r.u.f == 0.0 && signbit(r.u.f));
    error_is(rl_div(&cx, rl_int(1), rl_float(-0.0), &r), "division by zero");
    CHECK(r.k == RL_NONE);
    error_is(rl_mod(&cx, rl_float(1.0), rl_float(-0.0), &r), "division by zero");
    CHECK(r.k == RL_NONE);
}

static void bytes_are(rl_value v, const char *bytes, size_t len) {
    CHECK(v.k == RL_STR && S(v)->len == len);
    CHECK(memcmp(S(v)->data, bytes, len) == 0 && S(v)->data[len] == 0);
}

static void test_unicode_boundaries(void) {
    static const struct { long long cp; const char *bytes; size_t n; } cases[] = {
        {0, "\0", 1}, {0x7F, "\x7F", 1}, {0x80, "\xC2\x80", 2},
        {0x7FF, "\xDF\xBF", 2}, {0x800, "\xE0\xA0\x80", 3},
        {0xD7FF, "\xED\x9F\xBF", 3}, {0xE000, "\xEE\x80\x80", 3},
        {0xFFFF, "\xEF\xBF\xBF", 3}, {0x10000, "\xF0\x90\x80\x80", 4},
        {0x10FFFF, "\xF4\x8F\xBF\xBF", 4}
    };
    static const long long invalid[] = {-1, 0xD800, 0xDFFF, 0x110000};
    size_t i;
    rl_value arg, text, code;
    for (i = 0; i < sizeof cases / sizeof cases[0]; ++i) {
        arg = rl_int(cases[i].cp);
        CHECK(call("chr", 1, &arg, &text) == RL_OK);
        bytes_are(text, cases[i].bytes, cases[i].n);
        CHECK(rl_str_chars(S(text)) == 1);
        CHECK(call("ord", 1, &text, &code) == RL_OK);
        CHECK(code.k == RL_INT && code.u.i == cases[i].cp);
        rl_release(text);
    }
    for (i = 0; i < sizeof invalid / sizeof invalid[0]; ++i) {
        arg = rl_int(invalid[i]);
        error_is(call("chr", 1, &arg, &text),
                 "chr(): code must be 0 to 1114111 and not 55296 to 57343 (surrogates)");
        CHECK(text.k == RL_NONE);
    }
    arg = rl_float(NAN);
    error_is(call("chr", 1, &arg, &text), "chr(): argument must be a whole number");
    arg = rl_str_c("");
    error_is(call("ord", 1, &arg, &code), "ord(): needs a one-character string");
    rl_release(arg);
}

static void test_utf8_fragments_and_empty_strings(void) {
    static const char raw[] = {'x', (char)0xE2, (char)0x82, 'y', (char)0x80, (char)0xF0};
    rl_value text = rl_str_new(raw, sizeof raw), out, args[3], empty = rl_str_c("");
    size_t i;
    CHECK(rl_str_chars(S(text)) == sizeof raw);
    for (i = 0; i < sizeof raw; ++i) {
        CHECK(rl_str_offset(S(text), i) == i);
        CHECK(rl_index(&cx, text, rl_int((long long)i), &out) == RL_OK);
        bytes_are(out, raw + i, 1);
        rl_release(out);
    }
    CHECK(rl_in(&cx, empty, empty, &out) == RL_OK && out.u.b);
    CHECK(rl_in(&cx, text, empty, &out) == RL_OK && !out.u.b);
    CHECK(rl_str_offset(S(empty), (size_t)-1) == 0);
    error_is(rl_index(&cx, empty, rl_int(0), &out), "string index 0 is out of range (size 0)");
    error_is(rl_index(&cx, text, rl_int(LLONG_MAX), &out),
             "string index 9223372036854775807 is out of range (size 6)");
    error_is(rl_index(&cx, text, rl_int(4294967296LL), &out),
             "string index 4294967296 is out of range (size 6)");
    args[0] = text; args[1] = rl_int(1); args[2] = rl_int(3);
    CHECK(call("slice", 3, args, &out) == RL_OK);
    bytes_are(out, raw + 1, 2);
    rl_release(out);
    rl_release(text);
    rl_release(empty);
}

static void append_in_place(rl_value *slot, rl_value suffix) {
    rl_value out;
    CHECK(rl_add_to(&cx, slot, suffix, &out) == RL_OK && out.u.o == slot->u.o);
    rl_release(*slot);
    *slot = out;
}

static void test_append_utf8_cache(void) {
    char raw[512];
    size_t i;
    rl_value text, suffix;
    for (i = 0; i < sizeof raw; i += 2) { raw[i] = (char)0xC3; raw[i + 1] = (char)0xA9; }
    text = rl_str_new(raw, sizeof raw);
    CHECK(rl_str_offset(S(text), 200) == 400 && S(text)->marks != NULL);
    suffix = rl_str_c("\xE2\x82\xAC");
    append_in_place(&text, suffix);
    CHECK(S(text)->marks == NULL && rl_str_chars(S(text)) == 257);
    CHECK(rl_str_offset(S(text), 256) == 512);
    CHECK(rl_str_chars(S(suffix)) == 1);
    append_in_place(&text, suffix);
    CHECK(rl_str_chars(S(text)) == 258 && rl_str_offset(S(text), 257) == 515);
    rl_release(suffix);
    rl_release(text);

    text = rl_str_new("\xF0\x9F", 2);
    CHECK(rl_str_chars(S(text)) == 2);
    suffix = rl_str_new("\x98\x80", 2);
    CHECK(rl_str_chars(S(suffix)) == 2);
    append_in_place(&text, suffix);
    CHECK(rl_str_chars(S(text)) == 1 && rl_str_offset(S(text), 1) == 4);
    bytes_are(text, "\xF0\x9F\x98\x80", 4);
    rl_release(suffix);
    suffix = rl_str_c("");
    append_in_place(&text, suffix);
    CHECK(rl_str_chars(S(text)) == 1 && S(text)->len == 4);
    rl_release(suffix);
    rl_release(text);
}

static void test_list_reference_transfers(void) {
    size_t before, i;
    rl_value list = rl_list_new(32), out, saved;
    CHECK(rl_builtin("pop", &out));
    rl_release(out);
    before = rl_live_strings();
    for (i = 0; i < 24; ++i) {
        char label[24];
        rl_value text;
        snprintf(label, sizeof label, "item%lu", (unsigned long)i);
        text = rl_str_c(label);
        rl_list_push(L(list), text);
        rl_release(text);
    }
    out = rl_list_take(L(list), 3);
    CHECK(L(list)->off == 1 && out.u.o->rc == 1);
    bytes_are(out, "item3", 5);
    rl_list_insert(L(list), 4, out);
    CHECK(L(list)->off == 0 && out.u.o->rc == 2);
    rl_release(out);
    CHECK(rl_set_index(&cx, list, rl_int(4), L(list)->items[4]) == RL_OK);
    CHECK(L(list)->items[4].u.o->rc == 1);
    saved = rl_retain(L(list)->items[4]);
    CHECK(rl_set_index(&cx, list, rl_int(4), L(list)->items[5]) == RL_OK);
    bytes_are(saved, "item3", 5);
    CHECK(L(list)->items[4].u.o == L(list)->items[5].u.o
          && L(list)->items[4].u.o->rc == 2);
    CHECK(call("pop", 1, &list, &out) == RL_OK);
    CHECK(out.u.o->rc == 1);
    rl_release(list);
    bytes_are(out, "item23", 6);
    bytes_are(saved, "item3", 5);
    rl_release(out);
    rl_release(saved);
    CHECK(rl_live_strings() == before);
}

static void test_map_collisions_and_binary_keys(void) {
    static const char *const keys[] = {"a", "i", "q", "y"};
    rl_value map = rl_map_new(), out;
    size_t i, round;
    for (i = 0; i < 4; ++i) {
        rl_value key = rl_str_c(keys[i]);
        rl_map_set(M(map), S(key), rl_int((long long)i));
        rl_release(key);
    }
    /* These keys share the low three hash bits. A deleted first entry must
     * leave the rest of its probe chain searchable. */
    CHECK(M(map)->index_cap == 8);
    CHECK(rl_map_remove(M(map), "a", 1, &out));
    CHECK(rl_map_get(M(map), "y", 1)->u.i == 3);
    CHECK(rl_map_get(M(map), "i", 1)->u.i == 1);
    CHECK(!rl_map_get(M(map), "a", 1));
    for (round = 0; round < 4; ++round) {
        for (i = 0; i < 96; ++i) {
            char raw[3] = {'k', 0, 0};
            rl_value key;
            raw[2] = (char)i;
            key = rl_str_new(raw, sizeof raw);
            rl_map_set(M(map), S(key), rl_int((long long)(round * 100 + i)));
            rl_release(key);
        }
        for (i = 0; i < 96; ++i) {
            char raw[3] = {'k', 0, 0};
            rl_value *found;
            raw[2] = (char)i;
            found = rl_map_get(M(map), raw, sizeof raw);
            CHECK(found && found->u.i == (long long)(round * 100 + i));
            if (i % 4 != 0) {
                CHECK(rl_map_remove(M(map), raw, sizeof raw, &out));
                CHECK(out.u.i == (long long)(round * 100 + i));
            }
        }
        CHECK(M(map)->count == 27);
        CHECK(M(map)->used == M(map)->count + M(map)->holes);
    }
    {
        rl_value key = rl_str_c(""), text = rl_str_c("owned");
        rl_map_set(M(map), S(key), text);
        rl_release(key);
        rl_release(text);
        rl_map_set(M(map), M(map)->keys[M(map)->used - 1],
                   *rl_map_get(M(map), "", 0));
        CHECK(rl_map_remove(M(map), "", 0, &out));
        CHECK(out.u.o->rc == 1);
        rl_release(map);
        bytes_are(out, "owned", 5);
        rl_release(out);
    }
}

static void test_iteration_snapshots(void) {
    rl_value map = rl_map_new(), list = rl_list_new(0), key = rl_str_c("original");
    rl_value map_keys, copy, concat, old, out;
    rl_map_set(M(map), S(key), rl_int(1));
    rl_release(key);
    rl_list_push(L(list), map);
    CHECK(rl_loop_items(&cx, list, &copy) == RL_OK);
    CHECK(copy.u.o != list.u.o && L(copy)->items[0].u.o == map.u.o);
    CHECK(rl_loop_items(&cx, map, &map_keys) == RL_OK);
    CHECK(rl_map_remove(M(map), "original", 8, &old));
    CHECK(rl_set_field(&cx, map, "changed", rl_bool(1)) == RL_OK);
    CHECK(rl_field(&cx, L(copy)->items[0], "changed", &out) == RL_OK && out.u.b);
    CHECK(rl_add(&cx, list, copy, &concat) == RL_OK);
    CHECK(concat.u.o != list.u.o && concat.u.o != copy.u.o);
    CHECK(L(concat)->n == 2 && L(concat)->items[0].u.o == map.u.o
          && L(concat)->items[1].u.o == map.u.o);
    rl_release(list);
    rl_release(copy);
    rl_release(map);
    bytes_are(L(map_keys)->items[0], "original", 8);
    CHECK(rl_field(&cx, L(concat)->items[1], "changed", &out) == RL_OK && out.u.b);
    rl_release(map_keys);
    rl_release(concat);
}

static void test_list_slice_ownership(void) {
    rl_value list = rl_list_new(0), child = rl_map_new(), args[3], slice, r;
    rl_list_push(L(list), child);
    rl_list_push(L(list), rl_int(2));
    rl_list_push(L(list), rl_int(3));
    args[0] = list; args[1] = rl_float(-HUGE_VAL); args[2] = rl_float(HUGE_VAL);
    CHECK(call("slice", 3, args, &slice) == RL_OK);
    CHECK(slice.u.o != list.u.o && L(slice)->n == 3 && L(slice)->items[0].u.o == child.u.o);
    CHECK(rl_set_index(&cx, list, rl_int(1), rl_int(9)) == RL_OK);
    CHECK(L(slice)->items[1].u.i == 2);
    args[1] = rl_int(3); args[2] = rl_int(1);
    CHECK(call("slice", 3, args, &r) == RL_OK && L(r)->n == 0);
    rl_release(r);
    args[1] = rl_float(0.5);
    error_is(call("slice", 3, args, &r), "slice(): start must be a whole number");
    CHECK(r.k == RL_NONE);
    args[1] = rl_int(0); args[2] = rl_float(NAN);
    error_is(call("slice", 3, args, &r), "slice(): end must be a whole number");
    rl_release(list);
    rl_release(child);
    CHECK(rl_set_field(&cx, L(slice)->items[0], "alive", rl_int(1)) == RL_OK);
    CHECK(rl_field(&cx, L(slice)->items[0], "alive", &r) == RL_OK && r.u.i == 1);
    rl_release(slice);
}

static int failing_result(rl_ctx *ctx, rl_fun *self, int argc, const rl_value *argv, rl_value *out) {
    (void)self; (void)argc; (void)argv;
    *out = rl_list_new(1);
    rl_list_push(L(*out), *out);
    return rl_fail(ctx, "callback failed");
}

static int release_callee(rl_ctx *ctx, rl_fun *self, int argc, const rl_value *argv, rl_value *out) {
    rl_value old = self->env->slots[0];
    (void)ctx; (void)argc; (void)argv;
    self->env->slots[0] = rl_none();
    rl_release(old);
    CHECK(self->h.rc == 1);
    *out = rl_str_c(self->name->data);
    return RL_OK;
}

static void test_call_lifetimes(void) {
    size_t before = rl_live_objects();
    rl_value f = rl_fun_new("bad", failing_result, 0, NULL), out;
    rl_scope *scope;
    error_is(rl_call(&cx, f, 0, NULL, &out), "callback failed");
    CHECK(out.k == RL_NONE && cx.depth == 0);
    rl_release(f);
    CHECK(rl_gc_collect() == 1);
    CHECK(rl_live_objects() == before);
    scope = rl_scope_new(NULL, 1);
    scope->slots[0] = rl_fun_new("self_release", release_callee, 0, scope);
    CHECK(rl_call(&cx, scope->slots[0], 0, NULL, &out) == RL_OK);
    bytes_are(out, "self_release", 12);
    CHECK(scope->slots[0].k == RL_NONE && cx.depth == 0);
    rl_release(out);
    rl_scope_release(scope);
    CHECK(rl_live_objects() == before);

    f = rl_fun_new("blocked", failing_result, 0, NULL);
    cx.depth = RL_MAX_DEPTH;
    error_is(rl_call(&cx, f, 0, NULL, &out), "too much recursion (more than 800 calls deep)");
    CHECK(out.k == RL_NONE && cx.depth == RL_MAX_DEPTH);
    CHECK(call("gc", 0, NULL, &out) == RL_OK && out.k == RL_INT && out.u.i == 0);
    CHECK(cx.depth == RL_MAX_DEPTH);
    cx.depth = 0;
    rl_release(f);
}

int main(void) {
    rl_ctx_init(&cx, 0, NULL);
    test_range_limits();
    test_error_message_alias();
    test_number_spans();
    test_binary_std_error();
    test_report_without_source();
    test_file_write_errors();
    test_integer_overflow_branches();
    test_division_modulo_limits();
    test_unicode_boundaries();
    test_utf8_fragments_and_empty_strings();
    test_append_utf8_cache();
    test_list_reference_transfers();
    test_map_collisions_and_binary_keys();
    test_iteration_snapshots();
    test_list_slice_ownership();
    test_call_lifetimes();
    CHECK(!cx.has_err && cx.depth == 0);
    rl_ctx_free(&cx);
    CHECK(rl_gc_collect() == 0);
    CHECK(rl_live_objects() == 0 && rl_live_strings() == 0);
    printf("%d hardening checks passed\n", checks);
    return 0;
}
