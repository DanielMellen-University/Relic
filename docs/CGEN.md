# The code generator

`selfhost/cgen.relic` turns a Relic program into C99 that links against the C runtime in `runtime/c/` ([C-RUNTIME.md](C-RUNTIME.md)). It is milestone C2 of the plan to compile Relic to C ([PROJECT_PLAN.md](PROJECT_PLAN.md#self-host-and-bootstrap)). It is written in Relic, reads programs with the Relic-written lexer and parser in `selfhost/`, and runs on the C++ host (stage 0).

The rule is the same as for the other copies in `selfhost/`: the C program prints what `relic FILE` prints, and fails with the same message at the same line and column.

## Contents

- [Using it](#using-it)
- [What it covers](#what-it-covers)
- [Shape of the generated C](#shape-of-the-generated-c)
- [Names and scopes](#names-and-scopes)
- [Errors and positions](#errors-and-positions)
- [Tests](#tests)
- [Speed](#speed)
- [Open choices](#open-choices)

## Using it

```sh
relic selfhost/cgen.relic prog.relic -o prog.c
cc -std=c99 -O2 -I runtime/c -o prog prog.c build/runtime/c/librelic_rt.a -lm
./prog
```

With MSVC: `cl /std:c11 /O2 /I runtime\c prog.c build\runtime\c\Release\relic_rt.lib`. In CMake, link the `relic_rt` target and call `relic_c_target(name)` for the same strict flags the runtime uses.

Exit codes: 0 when the C was written, 1 when the file has a lex or parse error (printed the way the host prints it), 2 when the program uses something outside the C2 subset (the message names it and its position), 3 for bad usage.

`--stub` is for builds that compile every file in a folder. On a lex or parse error it writes a small C program that prints that same error and exits 1. On an unsupported program it writes one that prints `cgen-skip: <reason>` and exits 0. Either way cgen itself exits 0 and prints nothing.

The generated program takes its command-line args like `relic FILE args...` does. With `RELIC_LEAK_CHECK` set in the environment it checks at exit that every container and string was freed, and exits 3 with `leak: ...` if not.

## What it covers

Everything in the language except `use`:

- literals (int, float, string, bool, none, lists, maps), string interpolation
- arithmetic, comparison, `and` / `or` / `not`, `in`, ranges, indexing, fields, assigning into lists and maps
- `let`, assignment, `if` / `else if` / `else`, `while`, `for` over lists, strings, maps and ranges, `break`, `next`
- functions, `return`, recursion, `fun name(x) = expr`, closures (capture by reference, as on the host), names declared later in an enclosing function
- `as` hints on `let`, parameters and returns
- `try` / `catch`, `fail`, errors from deep calls
- every built-in in the runtime: `print`, `len`, `str`, `gc`, the `host_*` primitives and the rest

Not covered yet (C3): `use`, so modules and the std. A program with `use` is refused as a whole with code 2. One more corner is refused: a name that has `let`s with different `as` hints and is also used from a nested function.

## Shape of the generated C

One C file per program:

- a few macros (`RLC_FILE`, `RLC_IS`, `RLC_DROP`, `RLC_SET`, `RLC_ERR`)
- `K[]`: string constants, made once in `main`
- `B[]`: the built-ins the program uses, looked up once in `main` with `rl_builtin`
- one C function per Relic function, of type `rl_code`, named `fnN_name`
- `rlc_module`, the top level of the file
- `main`: sets up the context, runs `rlc_module`, prints an uncaught error with `rl_report`, frees everything

Inside a function:

| Name | Holds |
|---|---|
| `T[]` | temporaries; an expression writes its owned result to `T[d]` and the next free slot is `d + 1` |
| `L[]` | locals, when the function has no nested `fun` (see below) |
| `S[]` | scopes of inner blocks, when it does |
| `V[]`, `I[]` | the value and position of each `for` loop in progress |
| `R` | the return value |
| `E` | `self->env`, the scope the function was made in |

Every call that can fail is checked. On failure the code marks the position and jumps to the innermost `catch` label, or to `fail:`. Each function ends in `done:`, which releases every temporary and local, so every path frees what it holds. Loops use labels `top_N`, `cont_N` (only when there is a `next`) and `brk_N`.

## Names and scopes

Names follow the host's environments, which are the rules: a function call's scope holds its parameters and the `let`s at the top of its body; each run of an `if` or `while` body gets a new scope; each turn of a `for` gets one for the loop name and the body's `let`s; a `try` body gets one; a `catch` gets one for the error name and its body.

Each block's names get slots up front. A slot starts out unset (`RL_UNSET`), which is how a `let` that has not run yet is told apart from one that holds `none`.

There are two ways to keep the slots:

- **Stack mode.** A function with no nested `fun` cannot have its names captured, so they live in the C array `L[]`. A block's slots are set back to unset each time the block starts.
- **Heap mode.** The top level, and any function with a nested `fun`, keeps each block that has names in an `rl_scope` whose parent is the scope around it, so a closure can keep it alive.

A name is looked up at compile time where possible. If it is declared earlier in the same function, or in an enclosing function before the inner `fun` line, the slot is used directly. If an enclosing function declares it only further down, the code checks at run time whether that slot is set yet (`RLC_IS`) and otherwise falls back to the next scope out, then to a built-in, then to the error `'x' is not defined`. That matches the host for code like a nested function that reads a name before and after the outer `let` runs (golden `vm_late_local`).

## Errors and positions

Positions use a mark at each call site that can fail (option A, an interim decision, see [PROJECT_PLAN.md](PROJECT_PLAN.md#decisions-needed)):

```c
if (rl_sub(cx, L[0], rl_int(1LL), &T[2])) RLC_ERR(5, 18, 5, 18, 5, 18, fail);
```

`RLC_ERR` takes the node's line and column plus those of its first and second operand. The runtime says which one an error is about (`cx->err_arg`; index and key errors point at the index, a bad range end at that end), and `rl_at3` picks it. `on_error()` in `cgen.relic` and the `RLC_ERR` macro are the only places that know how positions are recorded, so moving to a per-function line table later changes those two and nothing else.

Errors such as `'x' is not defined`, `'x' is not declared; use 'let x = ...'`, `'break' outside a loop`, failed hints and `int is too large` come out when the program reaches them, as on the host, with the host's text.

## Tests

Every golden program in `tests/golden/` except REPL tests (`repl_*`) and those with a `.args` file (they test CLI flags) has a ctest case `cgen_name`. At build time cgen writes `build/cgen/name.c` (with `--stub`), CMake compiles it with the runtime's flags (`-std=c99 -Wall -Wextra -pedantic`, or `/W4` on MSVC), and `tools/run_cgen.cmake` runs it in `tests/golden/` with `RELIC_LEAK_CHECK=1` and compares stdout with `name.out`, or stderr with `name.err`. Programs cgen refuses show as skipped.

Today: 129 cases, 106 pass, 23 skip (all of them use `use`), none fail. The generated C compiles with no warnings, so CI builds it with warnings as errors. The one exception is MSVC's C4702 (unreachable code), turned off at the top of each generated file: the code after a jump to `fail:` or a `catch` label is still compiled, and that is expected. `relic_cgen_test(name file)` in the top-level `CMakeLists.txt` adds a case for any other program with a golden `.out`.

The `cgen` CI job builds and runs these on gcc and clang on Linux (with ASan, UBSan and LSan, UBSan errors fatal), clang on macOS and MSVC on Windows. `-DRELIC_CGEN_TESTS=OFF` leaves them out of a build.

## Speed

`bench/` programs, generated C built with gcc `-O2`, against the host's VM and tree-walker (Release build, best of 5, one Linux box):

| Program | Tree-walker | VM | Generated C |
|---|---|---|---|
| `fib(30)` | 947 ms | 363 ms | 141 ms |
| `fib.relic` (`fib(25)`) | 84 ms | 34 ms | 14 ms |
| `loops.relic` | 426 ms | 169 ms | 83 ms |
| `lists.relic` | 74 ms | 44 ms | 20 ms |
| `strings.relic` | 51 ms | 34 ms | 12 ms |

The hand-written C in `runtime/c/tests/demo.c` ran `fib(30)` in about 84 ms, so the generated code has room to improve: it copies more values than it needs to and calls through `rl_call` every time.

## Open choices

These came up while writing C2 and are not decided. The code does the recommended thing for now, which in each case is what the host does or the smallest change.

1. **What `gc()` returns.** The host counts freed scopes as well as lists, maps and functions. Generated code makes fewer scopes (none in stack mode), so the number can differ; the goldens only check ranges. Options: (a) leave it, and document `gc()`'s number as approximate; (b) make generated code allocate a scope wherever the host does, to match exactly; (c) have both count only Relic values (lists, maps, functions). Recommendation: (a) now, (c) if a program ever depends on the number.
2. **When stack-mode locals are freed.** In stack mode a block's locals are released when the block starts again or the function returns, not when the block ends. Output is the same; only the moment memory goes back differs. Options: (a) leave it; (b) release at block end too. Recommendation: (a), revisit with the C4 memory numbers.
3. **Programs outside the subset.** Options: (a) refuse the whole file (now); (b) compile it and fail at run time when the unsupported part runs. Recommendation: (a), so a missing feature never looks like a program bug.
4. **How modules compile (C3).** Options: (a) one C file per program: cgen follows `use` and compiles every module into the same file; (b) one C file per module, linked together, each with an init function run on first `use`; (c) (b) plus a generated table of modules for loading by path at run time. Recommendation: (a) for C3, since it is the smallest step and C4 only needs whole programs; (b) when build times matter.
