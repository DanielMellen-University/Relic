# The C runtime

`runtime/c/` is the runtime that compiled Relic programs link against. It is milestone C1 of the plan to compile Relic to C (see [PROJECT_PLAN.md](PROJECT_PLAN.md#self-host-and-bootstrap)). The code generator that writes C for it, `selfhost/cgen.relic` (C2), has its own page: [CGEN.md](CGEN.md). The C++ host is still the `relic` command and the default build, and this runtime is built and tested next to it.

The header [`runtime/c/relic.h`](../runtime/c/relic.h) is the reference. This page explains it.

## Contents

- [Decisions](#decisions)
- [Files and building](#files-and-building)
- [Values](#values)
- [Ownership](#ownership)
- [Objects](#objects)
- [Calling convention](#calling-convention)
- [Errors and try / catch](#errors-and-try--catch)
- [Built-ins and host primitives](#built-ins-and-host-primitives)
- [Memory and the cycle collector](#memory-and-the-cycle-collector)
- [Graphics](#graphics)
- [Calls for generated code](#calls-for-generated-code)
- [Tests](#tests)
- [Not in the runtime yet](#not-in-the-runtime-yet)

## Decisions

Decided by Daniel on Oct 20 2026:

| Question | Answer |
|---|---|
| Memory | reference counts plus a cycle collector, the same as the C++ host |
| C standard and compilers | C99, built with gcc, clang and MSVC (MSVC in `/std:c11`, its closest mode) |
| `try` / `catch` | every call returns an error flag, and generated code checks it after every call |
| Graphics | an optional C module over SDL2 |

Where a smaller choice came up (map order, int overflow, closure capture, number text), the runtime does what the C++ host does.

## Files and building

| File | What it holds |
|---|---|
| `relic.h` | the whole public ABI: types, ownership rules, every call |
| `rl_core.c` | objects, retain and release, the cycle collector, strings, lists, maps, scopes, functions, modules, `show` and `repr`, equality, errors, `rl_call` |
| `rl_ops.c` | operators, indexing, fields, ranges, conditions, loop items, type hints, number parsing |
| `rl_builtins.c` | the language built-ins (`print`, `len`, `str`, `num`, ...) and the shared built-in table |
| `rl_host.c` | the `host_*` primitives: files, folders, clocks, float math, process |
| `rl_gfx_stub.c` | graphics in a build without the SDL2 module |
| `rl_internal.h` | shared by the runtime's own files only |
| `tests/` | unit tests, the hand-compiled demo, the output compare script |

The main build includes it (`-DRELIC_C_RUNTIME=OFF` leaves it out). It also builds on its own:

```sh
cmake -S runtime/c -B build-c -DRELIC_C_WERROR=ON
cmake --build build-c
ctest --test-dir build-c
```

Options: `RELIC_C_WERROR` turns warnings into errors, `RELIC_C_SANITIZE` adds ASan and UBSan on gcc and clang (LSan comes with ASan on Linux). The library is `relic_rt`. It needs only the C library and libm.

## Values

A value is a small struct passed by copy:

```c
typedef struct {
    int k;    /* RL_NONE, RL_BOOL, RL_INT, RL_FLOAT, RL_STR, RL_LIST, RL_MAP, RL_FUN, RL_MODULE */
    union { int b; long long i; double f; rl_obj *o; } u;
} rl_value;
```

`none`, `bool`, `int` (64-bit) and `float` (double) live in the struct. Strings, lists, maps, functions and modules are heap objects behind `u.o`. Make values with `rl_none()`, `rl_bool(b)`, `rl_int(i)`, `rl_float(f)`, `rl_str_c(s)`, `rl_str_new(bytes, len)`, `rl_list_new(cap)`, `rl_map_new()`, `rl_fun_new(...)`, `rl_module_new(name)`.

The rules match the host: `int` with `int` stays `int` except for `/`; overflow is the error `int overflow`; `%` takes the sign of the right side; `1 == 1.0` is true; floats print with `%.15g` and always show a point or exponent.

## Ownership

One rule covers the whole ABI:

- A value passed **into** a runtime call or a compiled function is **borrowed**. The callee calls `rl_retain` if it keeps it.
- A value written to an **`out`** parameter is **owned** by the caller, who calls `rl_release` when done.

So `rl_list_push(l, v)` retains `v`, and `rl_index(cx, list, i, &out)` hands back a new reference. Immediate values (none, bool, int, float) ignore retain and release, so generated code can release every temporary without checking its type.

## Objects

Every heap object starts with `rl_obj`: a kind, a reference count, and links for the collector.

- **Strings** (`rl_str`) are immutable UTF-8 bytes with a trailing 0. The character count is worked out once and cached. A string whose characters are all one byte is indexed directly; a long one (256 bytes or more) with multi-byte characters gets, on its first index or slice, a table with the byte offset of every 64th character (`rl_str_offset`), so a lookup walks at most 63 characters. A malformed byte counts as one character, as on the host. Strings are not interned (the host does not intern either).
- **Lists** (`rl_list`) are growable arrays of values.
- **Maps** (`rl_map`) keep keys in insertion order, with a hash index on the side. Keys are strings. Removing a key keeps the order of the rest.
- **Scopes** (`rl_scope`) are a block of slots plus a parent scope. Generated code puts a function's names in slots. They are not Relic values.
- **Functions** (`rl_fun`) are a name, a C function pointer (`rl_code`), an arity (`-1` for any), and the scope they were made in. A closure captures by reference: it keeps its whole defining scope alive and sees later changes to it, which is how the host's environments work.
- **Modules** (`rl_module`) are a name plus either a map of top-level names (`rl_module_new`) or, for compiled modules, the module's top-level scope and the names of its slots (`rl_module_scoped`). A slot still unset is a name the module has not reached yet. `rl_field` on a module enforces private `_names` and gives `module 'm' has no 'x' (yet)`, with the host's messages.

## Calling convention

Every compiled Relic function and every built-in has this C type:

```c
typedef int (*rl_code)(rl_ctx *cx, rl_fun *self, int argc, const rl_value *argv, rl_value *out);
```

- `cx` is the program context: output stream, the current error, call depth, command-line args.
- `self` is the function being called; `self->env` is its captured scope.
- `argv` is borrowed. `*out` gets an owned result, or none.
- The return is `RL_OK` (0) or `RL_ERR` (1).

Call any function value through `rl_call(cx, f, argc, argv, &out)`. It checks that `f` is a function (`cannot call an int`), checks the arity (`fib() takes 1 argument, got 2`), enforces the 800-call depth limit (`too much recursion (more than 800 calls deep)`), adds the `name(): ` prefix to errors from built-ins (except `fail`, which raises its message as written), and gives the cycle collector a chance to run.

## Errors and try / catch

An error is a message plus a position, kept in `cx`. Runtime calls that can fail set it with `rl_fail(cx, fmt, ...)` and return `RL_ERR`. Generated code checks every such call and, on failure, records where it was and jumps. In the simplest form:

```c
#define CHECKED(call, line, col) \
    do { if (call) { rl_at(cx, FILE_NAME, line, col); goto fail; } } while (0)

CHECKED(rl_sub(cx, n, rl_int(1), &t1), 6, 18);
CHECKED(rl_call(cx, fib, 1, &t1, &t2), 6, 15);
```

`rl_at` only sets the position if it is not set yet, so the innermost call site wins, as on the host. The code generator uses `rl_at3`, which also knows which operand an error is about (see [Calls for generated code](#calls-for-generated-code)). A function that fails releases its temporaries at a `done:` label and returns `RL_ERR`, and its caller does the same.

`try` / `catch err` is a jump target inside the function:

```c
if (rl_call(cx, risky, 1, &i, &t)) { rl_at(cx, FILE_NAME, 28, 21); goto catch_1; }
...
goto end_try_1;
catch_1:
    rl_catch(cx, &err);      /* err = {message, line, col}; the error is cleared */
    ...
end_try_1:
```

At the top, an uncaught error is printed with `rl_report(cx, stderr, source)`: the same `file:line:col: error: message` line, source line and caret as the host. With `source` NULL it reads the file named in the error, as the host does.

## Built-ins and host primitives

`rl_builtin(name, &out)` gives the shared function object for any built-in, so `print == print` holds. Generated code looks each one up once.

- Language built-ins: `print`, `eprint`, `len`, `append`, `str`, `keys`, `fail`, `gc`, `type`, `slice`, `ord`, `chr`, `num`, `int`, `float`, `pop`, `insert`, `remove`, `args`.
- Host primitives, with the host's names, arity and messages: `host_args`, `host_read_file`, `host_write_file`, `host_file_exists`, `host_list_dir`, `host_is_dir`, `host_make_dir`, `host_remove`, `host_rename`, `host_file_size`, `host_cwd`, `host_env`, `host_exit`, `host_clock`, `host_now`, `host_sleep`, `host_floor`, `host_sqrt`, `host_sin`, `host_cos`, `host_exp`, `host_log`, `host_pow`, `host_same`, `host_std_root`, `host_names`, `host_call`.
- Graphics stubs and the clipboard: see below.

`host_names()` lists the same 62 host built-ins as a non-SDL C++ host. File paths are UTF-8. POSIX systems use `stat`, `opendir`, `mkdir` and `clock_gettime`; Windows uses the wide-character APIs (`_wfopen`, `FindFirstFileW`, `MoveFileExW`, `QueryPerformanceCounter`).

`num("...")` follows the host: digits that fit give an int, otherwise the whole text must read as a float through `strtod`, so hex floats, `inf` and `nan` work and an out-of-range number is not a number.

## Memory and the cycle collector

Reference counting frees most things right away. Releasing never recurses: a container that dies pushes children that also die onto a work list, so a list nested a million deep frees in a loop.

Lists, maps, scopes, functions and modules are tracked in one list for the cycle collector. `rl_gc_collect()` uses the host's algorithm from `src/interp/gc.cpp`:

1. For each tracked object, count the references it gets from other tracked objects.
2. An object with more references than that is held from outside (a C local, a temporary, the context). It is a root.
3. Mark everything reachable from roots.
4. The rest is garbage: hold it, clear its references, then let go, so the counts free it.

`gc()` runs it and gives the number freed. `rl_gc_maybe()`, called by `rl_call` and at the end of each loop turn, runs it once more than 100,000 containers are alive and doubles the threshold after, like the host. A function stored in the scope it captures is a cycle; so is the module scope holding its own functions. Both are freed by the collector.

`rl_live_objects()` and `rl_live_strings()` count what is alive, for leak checks.

The runtime keeps the collector list and the built-in table in process-wide variables, as the host does, so it runs one program per process.

## Graphics

Graphics stays an optional C module over SDL2. It is not part of C1. `rl_gfx_stub.c` answers like the C++ host built without `-DRELIC_SDL=ON`: `host_gfx_available()`, `host_gfx_has_png()` and `host_gfx_has_fonts()` give false, every other `host_gfx_*` fails with `graphics are not built in; rebuild with cmake -DRELIC_SDL=ON (needs SDL2)`, and `host_clipboard_set` / `host_clipboard_get` keep text in the process.

The plan for the module, after C2:

- A file `rl_gfx_sdl.c` that defines the same `host_gfx_*` table (`rl_gfx_defs()`) over SDL2, with SDL2_image and SDL2_ttf optional, as in `src/builtins/graphics.cpp`.
- A CMake option that builds it in place of the stub. Builds without SDL2 keep the stub.
- The headless `gfx_smoke` and `SDL_VIDEODRIVER=dummy` tests run against it too.

## Calls for generated code

Added in C2 and C3 for `selfhost/cgen.relic`:

| Call | What it does |
|---|---|
| `rl_unset()` | a value of kind `RL_UNSET`: a slot whose `let` has not run yet. New scope slots start unset. It never reaches Relic code |
| `rl_at3(cx, file, in_std, l, c, l1, c1, l2, c2)` | sets the error position to the node (`l`, `c`), or to its first or second operand when the failing call said the error is about that one. Calls say so in `cx->err_arg` (0 node, 1 first, 2 second): index and key errors point at the index, a bad range start or end at that end, as on the host. `rl_fail` resets it. `in_std` says the code is in a `std/` module; it is kept in `cx->err_std` with the position |
| `rl_module_scoped(name, scope, n, names)` | a module value over a compiled module's top-level scope (see Objects) |
| `rl_fun_std(f, stem)` | marks a function as defined in the std module `stem` (`"files"`); returns `f` |
| `rl_std_boundary(cx, f)` | called by code outside `std/` after a call to `f` failed. If `f` is a std function and the error happened inside `std/`, a `host_*` message gets the std name (`host_list_dir(): x` becomes `files.list(): x`) and the position is cleared, so the caller marks its call site, as the host does |
| `rl_fail_undefined(cx, name, n, names, slots)` | the error `'x' is not defined`, with the host's `(did you mean 'y'?)` hint chosen from the visible names whose slot is set (or NULL, meaning always visible) and every built-in |
| `rl_interp(n, parts, &out)` | string interpolation: joins `rl_show` of each part |

## Tests

| Test | What it checks |
|---|---|
| `c_runtime` | `tests/test_runtime.c`: numbers and overflow, number text, UTF-8, maps and order, lists, errors and their exact messages, the calls for generated code (compiled modules, the std boundary), built-ins, files and clocks, sorted folder listings, character offsets in long mixed-width strings, cycle collection (self-holding lists and maps, a closure in its own scope, the call-site threshold), a million-deep list, and that every container and string is freed at the end |
| `cgen_*` | golden programs compiled by `selfhost/cgen.relic`, see [CGEN.md](CGEN.md#tests) |
| `c_demo` | `tests/demo.c`: `tests/golden/c_demo.relic` compiled by hand the way C2 will: recursion (`fib`), a closure counter, `try` / `catch` with positions, the recursion limit, a loop over a UTF-8 string. Its output must equal `tests/golden/c_demo.out`, which the C++ host also checks as a golden test |

The expected strings in `test_runtime.c` were taken from the C++ host. CI builds the runtime on its own with warnings as errors on gcc and clang (Linux, both with ASan, UBSan and LSan), clang on macOS and MSVC on Windows. The `cgen` job does the same for the whole build and runs the `c_*` and `cgen_*` tests.

## Not in the runtime yet

Left for later:

- The SDL2 graphics module (above).
