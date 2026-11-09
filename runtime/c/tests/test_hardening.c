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

int main(void) {
    rl_ctx_init(&cx, 0, NULL);
    test_range_limits();
    CHECK(!cx.has_err && cx.depth == 0);
    rl_ctx_free(&cx);
    CHECK(rl_gc_collect() == 0);
    CHECK(rl_live_objects() == 0 && rl_live_strings() == 0);
    printf("%d hardening checks passed\n", checks);
    return 0;
}
