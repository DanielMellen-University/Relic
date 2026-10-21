/* rl_internal.h: shared by the runtime's own .c files, not by generated code. */
#ifndef RL_INTERNAL_H
#define RL_INTERNAL_H
#include "relic.h"

typedef struct { const char *name; int arity; rl_code code; } rl_builtin_def;

const rl_builtin_def *rl_host_defs(void);   /* rl_host.c */
const rl_builtin_def *rl_gfx_defs(void);    /* rl_gfx_stub.c or the SDL2 module */
size_t rl_builtin_count(void);
const rl_builtin_def *rl_builtin_at(size_t i);
void rl_builtins_release(void);

int rl_arg_num(rl_ctx *cx, rl_value v, const char *what, double *d);
int rl_arg_whole(rl_ctx *cx, rl_value v, const char *what, long long *r);
int rl_arg_str(rl_ctx *cx, rl_value v, const char *what);
#endif
