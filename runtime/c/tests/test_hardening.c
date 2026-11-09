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

int main(void) {
    rl_ctx_init(&cx, 0, NULL);
    test_range_limits();
    test_error_message_alias();
    test_number_spans();
    CHECK(!cx.has_err && cx.depth == 0);
    rl_ctx_free(&cx);
    CHECK(rl_gc_collect() == 0);
    CHECK(rl_live_objects() == 0 && rl_live_strings() == 0);
    printf("%d hardening checks passed\n", checks);
    return 0;
}
