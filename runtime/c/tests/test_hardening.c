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

int main(void) {
    rl_ctx_init(&cx, 0, NULL);
    test_range_limits();
    test_error_message_alias();
    test_number_spans();
    test_binary_std_error();
    test_report_without_source();
    test_file_write_errors();
    test_integer_overflow_branches();
    CHECK(!cx.has_err && cx.depth == 0);
    rl_ctx_free(&cx);
    CHECK(rl_gc_collect() == 0);
    CHECK(rl_live_objects() == 0 && rl_live_strings() == 0);
    printf("%d hardening checks passed\n", checks);
    return 0;
}
