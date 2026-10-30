/* relic.h: the C runtime for compiled Relic (milestone C1).
 *
 * Plain C99. Generated C code includes this file and links librelic_rt.
 * The ABI is described in docs/C-RUNTIME.md; this header is the reference.
 *
 * Ownership, in one rule: a value passed INTO a runtime call is borrowed
 * (the callee retains it if it keeps it), and a value written to an `out`
 * parameter is owned by the caller (release it when done).
 *
 * Errors: every call that can fail returns RL_OK (0) or RL_ERR (1). On
 * RL_ERR the error is stored in the context and `*out` is left as none.
 * Generated code checks every result:
 *
 *     if (rl_add(cx, a, b, &r)) { rl_at(cx, 3, 20); goto fail; }
 */
#ifndef RELIC_H
#define RELIC_H

#include <stddef.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RL_OK 0
#define RL_ERR 1
#define RL_MAX_DEPTH 800
/* A scope slot whose `let` has not run yet. Never a Relic value. */
#define RL_UNSET (-1)

typedef enum {
    RL_NONE, RL_BOOL, RL_INT, RL_FLOAT, RL_STR, RL_LIST, RL_MAP, RL_FUN, RL_MODULE, RL_SCOPE
} rl_kind;

/* Every heap object starts with this header. Strings are counted but not
 * tracked by the cycle collector (they hold no references). */
typedef struct rl_obj {
    int kind;                 /* rl_kind */
    long rc;                  /* reference count */
    struct rl_obj *gc_prev, *gc_next;   /* tracked containers only */
    long gc_aux;              /* collector scratch */
} rl_obj;

typedef struct {
    int k;                    /* rl_kind: RL_NONE .. RL_MODULE */
    union { int b; long long i; double f; rl_obj *o; } u;
} rl_value;

typedef struct rl_str {
    rl_obj h;
    size_t len;               /* bytes, not counting the trailing 0 */
    size_t chars;             /* UTF-8 characters, (size_t)-1 until counted */
    size_t *marks;            /* byte offset of every 64th character, built on first index; or NULL */
    char data[1];             /* len bytes plus a 0; allocated larger */
} rl_str;

/* items[0..n) are the list. Removing near the front moves the items before
   the removed one up instead of the rest down, so the allocation can start
   `off` slots before items; cap counts the slots from items on. */
typedef struct rl_list {
    rl_obj h;
    size_t n, cap;
    rl_value *items;
    size_t off;
} rl_list;
void rl_list_insert(rl_list *l, size_t i, rl_value v); /* retains v; i <= n */
rl_value rl_list_take(rl_list *l, size_t i);          /* removes item i < n, gives its reference */

/* Maps keep insertion order. Keys are strings. */
/* Entries are in insertion order. rl_map_remove leaves a hole (a NULL key)
   so it does not shift the rest; holes are squeezed out when they are over
   half of the entries. Loops over keys[0..used) skip NULL keys. */
typedef struct rl_map {
    rl_obj h;
    size_t count;             /* keys in the map */
    size_t used, cap;         /* entries in order, holes included */
    size_t holes;
    rl_str **keys;            /* NULL for a hole */
    rl_value *vals;
    size_t *index;            /* open addressing: entry position + 1, 0 = empty */
    size_t index_cap;         /* power of two, or 0 */
} rl_map;

/* A block or function scope: closures keep their scope alive. */
typedef struct rl_scope {
    rl_obj h;
    struct rl_scope *parent;
    size_t n;
    rl_value *slots;
} rl_scope;

typedef struct rl_ctx rl_ctx;
struct rl_fun;

/* The one calling convention for compiled functions and built-ins.
 * argv is borrowed; *out receives an owned value. */
typedef int (*rl_code)(rl_ctx *cx, struct rl_fun *self, int argc, const rl_value *argv, rl_value *out);

typedef struct rl_fun {
    rl_obj h;
    rl_str *name;
    rl_code code;
    int arity;                /* -1: any number */
    int builtin;              /* errors get a "name(): " prefix */
    rl_scope *env;            /* captured scope, or NULL */
    const char *std_module;   /* "files" for a function from std/files, else NULL */
} rl_fun;

typedef struct rl_module {
    rl_obj h;
    rl_str *name;
    rl_map *names;            /* the module's top-level names, or NULL ... */
    rl_scope *scope;          /* ... a compiled module's top-level scope */
    size_t nnames;            /* and the names of its slots */
    const char *const *snames;
} rl_module;

/* Per-program state: output, the current error, call depth, args. */
struct rl_ctx {
    FILE *out;
    int has_err;
    rl_str *err_msg;
    int err_line, err_col;    /* 0 until rl_at sets them */
    int err_arg;              /* which operand the error is about: 0 the operation, 1 the first, 2 the second */
    const char *err_file;
    int err_std;              /* the position is in a std module */
    int depth;
    int argc;
    char **argv;
};

/* ---- values ---- */
rl_value rl_none(void);
rl_value rl_unset(void);
rl_value rl_bool(int b);
rl_value rl_int(long long i);
rl_value rl_float(double f);
rl_value rl_obj_value(rl_obj *o);      /* takes the caller's reference */
rl_value rl_retain(rl_value v);        /* returns v */
void rl_release(rl_value v);
const char *rl_type_name(rl_value v);  /* "none", "int", "function", ... */
const char *rl_a_type(rl_value v);     /* "an int", "a list", ... for messages */

/* ---- strings ---- */
rl_value rl_str_new(const char *bytes, size_t len);
rl_value rl_str_c(const char *s);
size_t rl_str_chars(rl_str *s);
/* Byte offset of character `index` (s->len when index >= the count). */
size_t rl_str_offset(rl_str *s, size_t index);
rl_value rl_show(rl_value v);          /* how print shows it */
rl_value rl_repr(rl_value v);          /* how it shows inside a list or map */
size_t rl_utf8_char_size(const char *s, size_t len, size_t pos);

/* ---- lists, maps, scopes, functions, modules ---- */
rl_value rl_list_new(size_t cap);
void rl_list_push(rl_list *l, rl_value v);       /* retains v */
rl_value rl_map_new(void);
rl_value *rl_map_get(rl_map *m, const char *key, size_t len);
void rl_map_set(rl_map *m, rl_str *key, rl_value v);  /* retains key and v */
int rl_map_remove(rl_map *m, const char *key, size_t len, rl_value *out);
rl_scope *rl_scope_new(rl_scope *parent, size_t n);  /* owned; retains parent; slots start unset */
void rl_scope_release(rl_scope *s);
rl_value rl_fun_new(const char *name, rl_code code, int arity, rl_scope *env);
rl_value rl_module_new(const char *name);
/* A module over a compiled scope: slot i is the name names[i] (static
 * strings); a slot still unset is not there "(yet)". Retains scope. */
rl_value rl_module_scoped(const char *name, rl_scope *scope, size_t n, const char *const *names);
/* Marks a function value as defined in std module `stem`; returns f. */
rl_value rl_fun_std(rl_value f, const char *stem);

/* ---- errors ---- */
int rl_fail(rl_ctx *cx, const char *fmt, ...);   /* sets the error, returns RL_ERR */
int rl_fail_str(rl_ctx *cx, rl_str *msg);
void rl_at(rl_ctx *cx, const char *file, int line, int col); /* first position wins */
/* Like rl_at, but picks (l1, c1) or (l2, c2) when the error is about the
 * first or second operand (cx->err_arg), as the host does for index,
 * range and key errors. */
void rl_at3(rl_ctx *cx, const char *file, int in_std, int l, int c, int l1, int c1, int l2, int c2);
/* After a call to f from code outside std/ failed: if f is a std function
 * and the error happened inside std/, give it the host's std name
 * ("host_list_dir(): x" becomes "files.list(): x") and clear the position,
 * so the caller marks its own call site. */
void rl_std_boundary(rl_ctx *cx, rl_value f);
/* "'name' is not defined", with the host's "did you mean" over the names
 * given (a NULL slot means always declared; otherwise only if set) and the
 * built-ins. */
int rl_fail_undefined(rl_ctx *cx, const char *name, int n, const char *const *names, const rl_value *const *slots);
int rl_catch(rl_ctx *cx, rl_value *out);         /* error -> {message, line, col}, clears it */
/* file:line:col: error: message, then the source line and a caret, like
 * the host. source is the program text, or NULL to read the error's file now
 * (what the host does). */
void rl_report(rl_ctx *cx, FILE *to, const char *source);

/* ---- calls ---- */
int rl_call(rl_ctx *cx, rl_value f, int argc, const rl_value *argv, rl_value *out);
void rl_ctx_init(rl_ctx *cx, int argc, char **argv);
void rl_ctx_free(rl_ctx *cx);
int rl_builtin(const char *name, rl_value *out); /* any built-in or host_* by name */

/* ---- operators (all set an error and return RL_ERR on bad input) ---- */
int rl_add(rl_ctx *cx, rl_value a, rl_value b, rl_value *out);
int rl_sub(rl_ctx *cx, rl_value a, rl_value b, rl_value *out);
int rl_mul(rl_ctx *cx, rl_value a, rl_value b, rl_value *out);
int rl_div(rl_ctx *cx, rl_value a, rl_value b, rl_value *out);
int rl_mod(rl_ctx *cx, rl_value a, rl_value b, rl_value *out);
int rl_neg(rl_ctx *cx, rl_value a, rl_value *out);
int rl_not(rl_ctx *cx, rl_value a, rl_value *out);
int rl_equal(rl_value a, rl_value b);
int rl_compare(rl_ctx *cx, const char *op, rl_value a, rl_value b, rl_value *out); /* "<" "<=" ">" ">=" */
int rl_in(rl_ctx *cx, rl_value a, rl_value b, rl_value *out);
int rl_index(rl_ctx *cx, rl_value obj, rl_value idx, rl_value *out);
int rl_set_index(rl_ctx *cx, rl_value obj, rl_value idx, rl_value v);
int rl_field(rl_ctx *cx, rl_value obj, const char *name, rl_value *out);
int rl_set_field(rl_ctx *cx, rl_value obj, const char *name, rl_value v);
int rl_range(rl_ctx *cx, rl_value a, rl_value b, rl_value *out);
/* Checks a range start before its end is evaluated (the same check rl_range makes). */
int rl_range_start(rl_ctx *cx, rl_value a);
int rl_cond(rl_ctx *cx, rl_value v, const char *what, int *truth);  /* "'if' condition" */
int rl_loop_items(rl_ctx *cx, rl_value src, rl_value *out);         /* a list to walk */
int rl_check_type(rl_ctx *cx, const char *type, rl_value v, const char *what);
int rl_parse_num(rl_ctx *cx, const char *s, size_t len, rl_value *out);
void rl_interp(int n, const rl_value *parts, rl_value *out);   /* "{a} and {b}": show each part and join */

/* ---- cycle collector ---- */
size_t rl_gc_collect(void);     /* frees unreachable cycles, gives the count */
void rl_gc_maybe(void);         /* at calls and loop turns */
size_t rl_live_objects(void);   /* tracked containers alive now */
size_t rl_live_strings(void);   /* strings alive now (leak checks) */

#ifdef __cplusplus
}
#endif
#endif
