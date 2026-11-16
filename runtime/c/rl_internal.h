/* rl_internal.h: shared by the runtime's own .c files, not by generated code. */
#ifndef RL_INTERNAL_H
#define RL_INTERNAL_H
#include "relic.h"

typedef struct { const char *name; int arity; rl_code code; } rl_builtin_def;

const rl_builtin_def *rl_host_defs(void);   /* rl_host.c */
const rl_builtin_def *rl_gfx_defs(void);    /* rl_gfx_stub.c or rl_gfx_sdl.c */
void rl_gfx_release(void);                  /* at exit: close the window, free the clipboard */
size_t rl_builtin_count(void);
const rl_builtin_def *rl_builtin_at(size_t i);
/* Receiver mask for a real built-in (list 1, map 2, string 4), or 0.
 * *arity is set when the mask is not 0. */
int rl_builtin_receivers(const char *name, int *arity);
void rl_builtins_release(void);
/* Std folder with a trailing slash, into buf. 0 when it does not fit. */
size_t rl_std_root(char *buf, size_t n);
/* Drops the cached std function names used by field hints. */
void rl_method_hints_release(void);

int rl_fail_wrap(rl_ctx *cx, const char *before, const char *mid, size_t mid_len, const char *after);
int rl_arg_num(rl_ctx *cx, rl_value v, const char *what, double *d);
int rl_arg_whole(rl_ctx *cx, rl_value v, const char *what, long long *r);
int rl_arg_str(rl_ctx *cx, rl_value v, const char *what);
#endif
