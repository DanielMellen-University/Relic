# The code generator

`selfhost/cgen.relic` turns a Relic program, and every module it uses, into one C99 file that links against the C runtime in `runtime/c/` ([C-RUNTIME.md](C-RUNTIME.md)). It is milestones C2 and C3 of the plan to compile Relic to C ([PROJECT_PLAN.md](PROJECT_PLAN.md#self-host-and-bootstrap)). It is written in Relic and reads programs with the Relic-written lexer and parser in `selfhost/`. It runs on the C++ host (stage 0), and it also compiles itself: cgen compiled by cgen (stage 1) writes the same C as stage 0 (see [Compiling itself](#compiling-itself)).

The rule is the same as for the other copies in `selfhost/`: the C program prints what `relic FILE` prints, and fails with the same message at the same line and column.

## Contents

- [Using it](#using-it)
- [What it covers](#what-it-covers)
- [Shape of the generated C](#shape-of-the-generated-c)
- [Names and scopes](#names-and-scopes)
- [Modules](#modules)
- [Errors and positions](#errors-and-positions)
- [Tests](#tests)
- [Compiling itself](#compiling-itself)
- [Speed](#speed)
- [Decided choices](#decided-choices)
- [Open choices](#open-choices)

## Using it

```sh
relic selfhost/cgen.relic prog.relic -o prog.c
cc -std=c99 -O2 -I runtime/c -o prog prog.c build/runtime/c/librelic_rt.a -lm
./prog
```

With MSVC: `cl /std:c11 /O2 /I runtime\c prog.c build\runtime\c\Release\relic_rt.lib`. In CMake, link the `relic_rt` target and call `relic_c_target(name)` for the same strict flags the runtime uses.

Exit codes: 0 when the C was written, 1 when the file has a lex or parse error (printed the way the host prints it), 2 when the program uses something not supported yet (the message names it, its file and position; nothing in the language is refused today), 3 for bad usage.

`--stub` is for builds that compile every file in a folder. On a lex or parse error it writes a small C program that prints that same error and exits 1. On an unsupported program it writes one that prints `cgen-skip: <reason>` and exits 0. Either way cgen itself exits 0 and prints nothing.

The generated program takes its command-line args like `relic FILE args...` does. With `RELIC_LEAK_CHECK` set in the environment it checks at exit that every container and string was freed, and exits 3 with `leak: ...` if not.

## What it covers

The whole language:

- literals (int, float, string, bool, none, lists, maps), string interpolation
- arithmetic, comparison, `and` / `or` / `not`, `in`, ranges, indexing, fields, assigning into lists and maps
- `let`, assignment, `if` / `else if` / `else`, `while`, `for` over lists, strings, maps and ranges, `break`, `next`
- functions, `return`, recursion, `fun name(x) = expr`, closures (capture by reference, as on the host), names declared later in an enclosing function
- `as` hints on `let`, parameters and returns
- `try` / `catch`, `fail`, errors from deep calls
- every built-in in the runtime: `print`, `len`, `str`, `gc`, the `host_*` primitives and the rest
- `use` for the program's own files and for `std/`, `use ... as`, module values and their fields, cycles between modules, a module that uses the main file back (see [Modules](#modules))

Graphics still go through the runtime's stub (no SDL2 module in C yet), so `std/graphics` loads and reports that graphics are not available, as a non-SDL host does.

## Shape of the generated C

One C file per program:

- a few macros (`RLC_IS`, `RLC_DROP`, `RLC_SET`, `RLC_ERR`)
- the module tables: `RLC_FILES[]` and `RLC_STD[]` (each module's file, and whether it is in `std/`), `MOD[]` (each module's value once it has started), `MS[]` (its top-level scope), `NMi[]` (its top-level names)
- `K[]`: string constants, made once in `main`
- `B[]`: the built-ins the program uses, looked up once in `main` with `rl_builtin`
- one C function per Relic function, of type `rl_code`, named `fnN_name`
- `rlc_mod_i` for the top level of module `i`, and `rlc_use_i`, which runs it the first time and hands out the module value; module 0 is the main file
- `main`: sets up the context, registers the main file as module 0, runs `rlc_mod_0`, prints an uncaught error with `rl_report`, frees everything

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

A `let` with an `as` hint makes later assignments check the type, until a `let` without one clears it. When a name's `let`s disagree about the hint and the name is in a scope a nested function can reach, the hint in effect is kept in a hidden slot next to it, so an assignment from the nested function checks against the latest `let` that ran. That is what the tree-walker (`relic --tree`) does. The VM checks the hint that was in effect where the nested function was defined instead, so the two engines differ there; `tests/cgen/hint_relet.relic` pins the tree-walker's answer (see [Open choices](#open-choices)).

## Modules

Modules are compiled into the same C file as the program (interim option 1, see [Decided choices](#decided-choices)). All the code that knows this is in the "modules" section of `cgen.relic`; per-module C files would change only that section.

- While compiling, each `use` is resolved the way the host resolves it: `std/name` is `host_std_root()` plus `name.relic`, anything else is relative to the folder of the file that has the `use`. The key is the absolute path with `.` and `dir/..` folded away. A file seen before is not compiled again.
- The main file is module 0, registered before anything runs, so a module that uses it back gets the main scope instead of running the main file twice (golden `use_main_cycle`).
- `use` runs `rlc_use_i`. The first time, it makes the module's scope and value and runs its top level. After that, and while it is still running (a cycle), it hands out the same value, so names the module has not reached yet are not there "(yet)" (goldens `use_cycle`, `err_use_field`). A module that failed partway stays loaded as far as it got (golden `use_cache`).
- A missing file fails at the `use` with `cannot find module 'x' (no file x.relic)`. A module with a lex or parse error fails at the `use` with that error at its own file and position (golden `err_use_parse`).
- A module value (`rl_module_scoped`) reads its names from the module's top-level scope, with the host's messages for private `_names` and missing ones.
- The std boundary: a std function called from code outside `std/` that fails inside `std/` reports the error at the call, and a `host_*` message gets the std name: `files.list(): cannot list x` (goldens `err_std_name`, `std_folders`). Generated code outside `std/` calls `rl_std_boundary` after a failed call; functions defined in a std module carry their module's name (`rl_fun_std`).

## Errors and positions

Positions use a mark at each call site that can fail (option A, an interim decision, see [PROJECT_PLAN.md](PROJECT_PLAN.md#decisions-needed)). The first argument says which module's file the code is in:

```c
if (rl_sub(cx, L[0], rl_int(1LL), &T[2])) RLC_ERR(0, 5, 18, 5, 18, 5, 18, fail);
```

`RLC_ERR` takes the node's line and column plus those of its first and second operand. The runtime says which one an error is about (`cx->err_arg`; index and key errors point at the index, a bad range end at that end), and `rl_at3` picks it. `on_error()` in `cgen.relic` and the `RLC_ERR` macro are the only places that know how positions are recorded, so moving to a per-function line table later changes those two and nothing else.

Errors such as `'x' is not defined`, `'x' is not declared; use 'let x = ...'`, `'break' outside a loop`, failed hints and `int is too large` come out when the program reaches them, as on the host, with the host's text.

## Tests

Every golden program in `tests/golden/` except REPL tests (`repl_*`) and those with a `.args` file (they test CLI flags) has a ctest case `cgen_name`. At build time cgen writes `build/cgen/name.c` (with `--stub`), CMake compiles it with the runtime's flags (`-std=c99 -Wall -Wextra -pedantic`, or `/W4` on MSVC), and `tools/run_cgen.cmake` runs it in `tests/golden/` with `RELIC_LEAK_CHECK=1` and compares stdout with `name.out`, or stderr with `name.err`. A program cgen refused would show as skipped; none is today.

Today: 131 cases, all pass. `tests/cgen/` holds cases for the compiled side only, with expected output from `relic --tree` (`cgen_only_hint_relet`). The generated C compiles with no warnings, so CI builds it with warnings as errors. The one exception is MSVC's C4702 (unreachable code), turned off at the top of each generated file: the code after a jump to `fail:` or a `catch` label is still compiled, and that is expected. `relic_cgen_test(name file)` in the top-level `CMakeLists.txt` adds a case for any other program with a golden `.out`, and `relic_cgen_exe(name file)` just builds one.

The `cgen` CI job builds and runs these, and the three below, on gcc and clang on Linux (with ASan, UBSan and LSan, UBSan errors fatal), clang on macOS and MSVC on Windows. `-DRELIC_CGEN_TESTS=OFF` leaves them out of a build.

## Compiling itself

The Relic-written front end and cgen compile to C, and the results are checked in ctest:

| Test | Checks |
|---|---|
| `cgen_selfhost_lexer` | `selfhost/tokens.relic` compiled to C prints exactly what `relic --tokens` prints, for every golden, helper, `std/` and `selfhost/` file (168 today) |
| `cgen_selfhost_parser` | `selfhost/ast.relic` compiled to C prints exactly what `relic --ast` prints, for the same files |
| `cgen_fixed_point` | cgen compiled by cgen on the host (stage 1) writes, for `selfhost/cgen.relic`, exactly the C that cgen on the host (stage 0) wrote. Stage 2 is built from that same C, so it is the same program: a fixed point. It runs with the leak check on |

Also checked by hand: stage 1 writes the same C as stage 0 for all 131 compiled goldens, and `selfhost/run.relic` compiled to C runs 127 goldens (all but the REPL and `.args` ones and the four `selfhost_interp` skips) and matches the host on every one.

## Speed

`bench/` programs, generated C built with gcc `-O2`, against the host's VM and tree-walker (Release build, best of 5, one Linux box):

| Program | Tree-walker | VM | Generated C |
|---|---|---|---|
| `fib(30)` | 984 ms | 361 ms | 146 ms |
| `fib.relic` (`fib(25)`) | 84 ms | 35 ms | 14 ms |
| `loops.relic` | 427 ms | 169 ms | 86 ms |
| `lists.relic` | 77 ms | 45 ms | 21 ms |
| `strings.relic` | 53 ms | 36 ms | 12 ms |

The Relic-written tools on `selfhost/cgen.relic` (about 1,000 lines), run on the host VM and compiled:

| Tool | On the host | Compiled |
|---|---|---|
| `tokens.relic` | 586 ms | 48 ms |
| `ast.relic` | 655 ms | 72 ms |
| `cgen.relic` compiling itself | 1,060 ms | 205 ms |

Two fixes got there: indexing a string whose characters are all one byte is now direct (it used to walk from the start, which made the compiled lexer slower than the host), and `strings.join` joins halves instead of growing one string. A string with multi-byte characters still walks from the start on each index.

The hand-written C in `runtime/c/tests/demo.c` ran `fib(30)` in about 84 ms, so the generated code has room to improve: it copies more values than it needs to and calls through `rl_call` every time.

## Decided choices

Recorded in [PROJECT_PLAN.md](PROJECT_PLAN.md#decisions-needed):

- `gc()`'s count is approximate in compiled code (it makes fewer scopes than the host). Decided by Engineer, Oct 20 2026, internal; revisit with "count only Relic values" if anything depends on the number.
- Locals in functions with no nested `fun` are released when their block starts again or the function returns, not at block end. Decided by Engineer, Oct 20 2026, internal.
- A program with something cgen does not support is refused as a whole. Decided by Engineer, Oct 20 2026, internal.
- Modules: one C file per program with every module it uses compiled in. **Interim, Oct 20 2026, pending Daniel's confirmation.** The emission is isolated so per-module C files are a contained change.
- Error positions: a mark at each call site that can fail. **Interim, Oct 20 2026, pending Daniel's confirmation.**

## Open choices

New in C3, not decided. The code does the recommended thing for now.

1. **Re-hinted names assigned from a nested function.** The tree-walker checks the hint of the latest `let` that ran; the VM checks the hint in effect where the nested function was defined, and for top-level names the last hinted `let` in the file. cgen follows the tree-walker. Options: (a) fix the VM to track the hint at run time, like the tree-walker; (b) make the rule static (the hint where the function was defined) and change the tree-walker and cgen; (c) make changing a name's hint with a second `let` an error. Recommendation: (a), since the tree-walker is the reference and the case is rare.
2. **Where a compiled program's std comes from.** The std modules are compiled in from the std folder cgen saw (`$RELIC_STD` or the build's `std/`); `host_std_root()` in the program still reports the runtime's folder. Options: (a) compile time, as now; (b) load std modules at run time from `$RELIC_STD` (needs per-module C or an interpreter). Recommendation: (a).
3. **Source lines in error messages.** File names in errors are baked in at compile time, and `rl_report` reads the source line from that file when the error happens, as the host does. A compiled program moved to another machine prints the error line but no source line. Options: (a) leave it; (b) embed each module's source in the C file for the snippet; (c) embed only the lines that can fail. Recommendation: (a) until compiled programs ship on their own (C5), then (b).
