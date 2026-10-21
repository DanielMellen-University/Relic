/* demo.c: tests/golden/c_demo.relic compiled by hand, the way the C2
 * code generator will write it. It proves the calling convention:
 *
 *   - every Relic function is an rl_code: (cx, self, argc, argv, out) -> RL_OK/RL_ERR
 *   - arguments are borrowed, *out is owned
 *   - every call that can fail is checked; on failure the call site adds
 *     its position with rl_at and jumps to the enclosing catch or `fail`
 *   - names live in scope slots; a closure keeps its scope alive
 *   - try/catch is a label; rl_catch turns the error into {message, line, col}
 *
 * Output must equal tests/golden/c_demo.out (the C++ host's output). */
#include "../relic.h"

#include <stdio.h>
#include <stdlib.h>

#define FILE_NAME "c_demo.relic"
#define CHECKED(call, line, col) do { if (call) { rl_at(cx, FILE_NAME, line, col); goto fail; } } while (0)
#define SET(slot, v) do { rl_value old_ = (slot); (slot) = (v); rl_release(old_); } while (0)

/* Module scope slots. */
enum { G_FIB, G_MAKE_COUNTER, G_C, G_RISKY, G_DOWN, G_PARTS, G_I, G_ERR, G_CH, G_COUNT };

/* Built-ins the program uses, looked up once. */
static rl_value b_print, b_fail, b_str, b_append, b_len;

static int print1(rl_ctx *cx, rl_value a) {
    rl_value r;
    int e = rl_call(cx, b_print, 1, &a, &r);
    rl_release(r);
    return e;
}

/* fun fib(n) */
static int fn_fib(rl_ctx *cx, rl_fun *self, int argc, const rl_value *argv, rl_value *out) {
    rl_value n = argv[0], t1 = rl_none(), t2 = rl_none(), t3 = rl_none(), t4 = rl_none();
    rl_value *g = self->env->slots;
    int r = RL_OK, truth;
    (void)argc;
    *out = rl_none();
    /* if n < 2 */
    CHECKED(rl_compare(cx, "<", n, rl_int(2), &t1), 4, 10);
    CHECKED(rl_cond(cx, t1, "'if' condition", &truth), 4, 8);
    if (truth) { *out = rl_retain(n); goto done; }
    /* return fib(n - 1) + fib(n - 2) */
    CHECKED(rl_sub(cx, n, rl_int(1), &t1), 6, 18);
    CHECKED(rl_call(cx, g[G_FIB], 1, &t1, &t2), 6, 15);
    CHECKED(rl_sub(cx, n, rl_int(2), &t3), 6, 31);
    CHECKED(rl_call(cx, g[G_FIB], 1, &t3, &t4), 6, 28);
    CHECKED(rl_add(cx, t2, t4, out), 6, 24);
    goto done;
fail:
    r = RL_ERR;
done:
    rl_release(t1); rl_release(t2); rl_release(t3); rl_release(t4);
    return r;
}

/* fun bump(), inside make_counter: n is slot 0 of the captured scope. */
static int fn_bump(rl_ctx *cx, rl_fun *self, int argc, const rl_value *argv, rl_value *out) {
    rl_value t1 = rl_none();
    rl_value *up = self->env->slots;
    (void)argc; (void)argv;
    *out = rl_none();
    CHECKED(rl_add(cx, up[0], rl_int(1), &t1), 13, 15);
    SET(up[0], t1);
    *out = rl_retain(up[0]);
    return RL_OK;
fail:
    return RL_ERR;
}

/* fun make_counter(): a new scope {n, bump} whose parent is the module scope. */
static int fn_make_counter(rl_ctx *cx, rl_fun *self, int argc, const rl_value *argv, rl_value *out) {
    rl_scope *s = rl_scope_new(self->env, 2);
    (void)cx; (void)argc; (void)argv;
    s->slots[0] = rl_int(0);
    s->slots[1] = rl_fun_new("bump", fn_bump, 0, s);
    *out = rl_retain(s->slots[1]);
    rl_scope_release(s);   /* bump's env keeps it alive */
    return RL_OK;
}

/* fun risky(x) */
static int fn_risky(rl_ctx *cx, rl_fun *self, int argc, const rl_value *argv, rl_value *out) {
    rl_value x = argv[0], t1 = rl_none(), t2 = rl_none(), t3 = rl_none(), t4 = rl_none();
    int r = RL_OK, truth;
    (void)self; (void)argc;
    *out = rl_none();
    CHECKED(rl_compare(cx, ">", x, rl_int(2), &t1), 22, 10);
    CHECKED(rl_cond(cx, t1, "'if' condition", &truth), 22, 8);
    if (truth) {
        t2 = rl_str_c("too big: ");
        CHECKED(rl_call(cx, b_str, 1, &x, &t3), 23, 31);
        CHECKED(rl_add(cx, t2, t3, &t4), 23, 29);
        SET(t1, rl_none());
        CHECKED(rl_call(cx, b_fail, 1, &t4, &t1), 23, 13);
    }
    CHECKED(rl_mul(cx, x, rl_int(10), out), 24, 14);
    goto done;
fail:
    r = RL_ERR;
done:
    rl_release(t1); rl_release(t2); rl_release(t3); rl_release(t4);
    return r;
}

/* fun down(k) = down(k + 1) */
static int fn_down(rl_ctx *cx, rl_fun *self, int argc, const rl_value *argv, rl_value *out) {
    rl_value t1 = rl_none();
    int r = RL_OK;
    (void)argc;
    *out = rl_none();
    CHECKED(rl_add(cx, argv[0], rl_int(1), &t1), 32, 22);
    CHECKED(rl_call(cx, self->env->slots[G_DOWN], 1, &t1, out), 32, 19);
    goto done;
fail:
    r = RL_ERR;
done:
    rl_release(t1);
    return r;
}

/* Prints "<message> <line>" or more fields of a caught error. */
static int print_err(rl_ctx *cx, rl_value err, int with_col) {
    rl_value parts[3], r = rl_none();
    int n = with_col ? 3 : 2, i, e;
    for (i = 0; i < 3; ++i) parts[i] = rl_none();
    e = rl_field(cx, err, "message", &parts[0]) || rl_field(cx, err, "line", &parts[1]) ||
        (with_col && rl_field(cx, err, "col", &parts[2])) || rl_call(cx, b_print, n, parts, &r);
    for (i = 0; i < 3; ++i) rl_release(parts[i]);
    rl_release(r);
    return e;
}

/* The module body. g is the module scope. */
static int module_main(rl_ctx *cx, rl_scope *top) {
    rl_value *g = top->slots;
    rl_value t1 = rl_none(), t2 = rl_none(), t3 = rl_none(), items = rl_none(), args[4];
    size_t k;
    int r = RL_OK, i;
    for (i = 0; i < 4; ++i) args[i] = rl_none();

    g[G_FIB] = rl_fun_new("fib", fn_fib, 1, top);
    t1 = rl_int(20);
    CHECKED(rl_call(cx, g[G_FIB], 1, &t1, &t2), 8, 10);
    CHECKED(print1(cx, t2), 8, 6);
    SET(t2, rl_none());

    g[G_MAKE_COUNTER] = rl_fun_new("make_counter", fn_make_counter, 0, top);
    CHECKED(rl_call(cx, g[G_MAKE_COUNTER], 0, NULL, &t1), 17, 21);
    SET(g[G_C], t1);
    t1 = rl_none();
    CHECKED(rl_call(cx, g[G_C], 0, NULL, &t1), 18, 2);
    SET(t1, rl_none());
    CHECKED(rl_call(cx, g[G_C], 0, NULL, &t2), 19, 18);
    args[0] = rl_str_c("count");
    args[1] = t2;
    t2 = rl_none();
    CHECKED(rl_call(cx, b_print, 2, args, &t1), 19, 6);
    SET(args[0], rl_none());
    SET(args[1], rl_none());

    g[G_RISKY] = rl_fun_new("risky", fn_risky, 1, top);
    /* for i in 0..5: try print(risky(i)) catch err ... */
    CHECKED(rl_range(cx, rl_int(0), rl_int(5), &t1), 26, 11);
    CHECKED(rl_loop_items(cx, t1, &items), 26, 10);
    SET(t1, rl_none());
    for (k = 0; k < ((rl_list *)items.u.o)->n; ++k) {
        SET(g[G_I], rl_retain(((rl_list *)items.u.o)->items[k]));
        if (rl_call(cx, g[G_RISKY], 1, &g[G_I], &t2)) { rl_at(cx, FILE_NAME, 28, 21); goto catch_1; }
        if (print1(cx, t2)) { rl_at(cx, FILE_NAME, 28, 14); goto catch_1; }
        SET(t2, rl_none());
        goto end_try_1;
    catch_1:
        SET(t2, rl_none());
        rl_catch(cx, &t3);
        SET(g[G_ERR], t3);
        t3 = rl_none();
        args[0] = rl_str_c("caught");
        CHECKED(rl_field(cx, g[G_ERR], "message", &args[1]), 30, 31);
        args[2] = rl_str_c("at line");
        CHECKED(rl_field(cx, g[G_ERR], "line", &args[3]), 30, 57);
        CHECKED(rl_call(cx, b_print, 4, args, &t1), 30, 18);
        for (i = 0; i < 4; ++i) SET(args[i], rl_none());
        SET(t1, rl_none());
    end_try_1:
        rl_gc_maybe();
    }
    SET(items, rl_none());

    g[G_DOWN] = rl_fun_new("down", fn_down, 1, top);
    t1 = rl_int(0);
    if (rl_call(cx, g[G_DOWN], 1, &t1, &t2)) { rl_at(cx, FILE_NAME, 34, 9); goto catch_2; }
    SET(t2, rl_none());
    goto end_try_2;
catch_2:
    rl_catch(cx, &t3);
    SET(g[G_ERR], t3);
    t3 = rl_none();
    CHECKED(print_err(cx, g[G_ERR], 0), 36, 10);
end_try_2:

    g[G_PARTS] = rl_list_new(0);
    t1 = rl_str_c("h\xc3\xa9llo");
    CHECKED(rl_loop_items(cx, t1, &items), 39, 11);
    SET(t1, rl_none());
    for (k = 0; k < ((rl_list *)items.u.o)->n; ++k) {
        SET(g[G_CH], rl_retain(((rl_list *)items.u.o)->items[k]));
        args[0] = g[G_PARTS];
        args[1] = g[G_CH];
        CHECKED(rl_call(cx, b_append, 2, args, &t1), 40, 11);
        args[0] = args[1] = rl_none();
        SET(t1, rl_none());
        rl_gc_maybe();
    }
    SET(items, rl_none());
    CHECKED(rl_call(cx, b_len, 1, &g[G_PARTS], &t2), 41, 18);
    args[0] = g[G_PARTS];
    args[1] = t2;
    CHECKED(rl_call(cx, b_print, 2, args, &t1), 41, 6);
    args[0] = rl_none();
    SET(args[1], rl_none());
    t2 = rl_none();
    SET(t1, rl_none());

    if (rl_index(cx, g[G_PARTS], rl_int(9), &t2)) { rl_at(cx, FILE_NAME, 43, 17); goto catch_3; }
    if (print1(cx, t2)) { rl_at(cx, FILE_NAME, 43, 10); goto catch_3; }
    SET(t2, rl_none());
    goto end_try_3;
catch_3:
    rl_catch(cx, &t3);
    SET(g[G_ERR], t3);
    t3 = rl_none();
    CHECKED(print_err(cx, g[G_ERR], 1), 45, 10);
end_try_3:
    goto done;
fail:
    r = RL_ERR;
done:
    rl_release(t1); rl_release(t2); rl_release(t3); rl_release(items);
    for (i = 0; i < 4; ++i) rl_release(args[i]);
    return r;
}

int main(int argc, char **argv) {
    rl_ctx cx;
    rl_scope *top;
    int r;
    rl_ctx_init(&cx, argc - 1, argv + 1);
    rl_builtin("print", &b_print);
    rl_builtin("fail", &b_fail);
    rl_builtin("str", &b_str);
    rl_builtin("append", &b_append);
    rl_builtin("len", &b_len);
    top = rl_scope_new(NULL, G_COUNT);
    r = module_main(&cx, top);
    if (r) rl_report(&cx, stderr, NULL);
    fflush(stdout);
    rl_scope_release(top);
    rl_release(b_print); rl_release(b_fail); rl_release(b_str); rl_release(b_append); rl_release(b_len);
    rl_ctx_free(&cx);
    /* Functions in the module scope point back at it: a cycle for the collector. */
    rl_gc_collect();
    if (rl_live_objects() || rl_live_strings()) {
        fprintf(stderr, "leak: %lu containers, %lu strings still alive\n", (unsigned long)rl_live_objects(),
                (unsigned long)rl_live_strings());
        return 3;
    }
    return r ? 1 : 0;
}
