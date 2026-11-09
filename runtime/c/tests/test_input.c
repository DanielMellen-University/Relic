/* Run with redirected input by check_input.cmake. */
#include "../relic.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static rl_ctx cx;
static rl_value reader, prompt;
static int checks;

#define CHECK(c) do { ++checks; if (!(c)) { \
    fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #c); exit(1); \
} } while (0)
#define S(v) ((rl_str *)(v).u.o)

static rl_value read_line(void) {
    rl_value out;
    CHECK(rl_call(&cx, reader, 1, &prompt, &out) == RL_OK);
    CHECK(!cx.has_err && cx.depth == 0);
    return out;
}

int main(int argc, char **argv) {
    rl_value out;
    char expected[300];
    CHECK(argc == 2);
    rl_ctx_init(&cx, 0, NULL);
    CHECK(rl_builtin("host_read_line", &reader));
    prompt = rl_str_c("must not print this prompt");
    if (strcmp(argv[1], "lines") == 0) {
        out = read_line();
        CHECK(out.k == RL_STR && S(out)->len == 0);
        rl_release(out);
        out = read_line();
        CHECK(out.k == RL_STR && S(out)->len == 0);
        rl_release(out);
        out = read_line();
        CHECK(out.k == RL_STR && S(out)->len == 300 && S(out)->data[300] == 0);
        memset(expected, 'x', sizeof expected);
        CHECK(memcmp(S(out)->data, expected, sizeof expected) == 0);
        rl_release(out);
        out = read_line();
        CHECK(out.k == RL_STR && S(out)->len == 5 && memcmp(S(out)->data, "last\r", 5) == 0);
        rl_release(out);
    } else {
        CHECK(strcmp(argv[1], "empty") == 0);
    }
    out = read_line();
    CHECK(out.k == RL_NONE);
    out = read_line();
    CHECK(out.k == RL_NONE);
    rl_release(reader);
    rl_release(prompt);
    rl_ctx_free(&cx);
    CHECK(rl_gc_collect() == 0);
    CHECK(rl_live_objects() == 0 && rl_live_strings() == 0);
    printf("%d input checks passed\n", checks);
    return 0;
}
