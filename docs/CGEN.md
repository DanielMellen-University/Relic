# The code generator

`selfhost/cgen.relic` turns a Relic program, and every module it uses, into one C99 file that links against the C runtime in `runtime/c/` ([C-RUNTIME.md](C-RUNTIME.md)). It is milestones C2 to C5 of the plan to compile Relic to C ([PROJECT_PLAN.md](PROJECT_PLAN.md#self-host-and-bootstrap)). It is written in Relic and reads programs with the Relic-written lexer and parser in `selfhost/`. It compiles itself, and its own C is committed as `bootstrap/cgen.c`, so the default build makes it with only a C compiler: that build's `build/relicc` is cgen (see [Bootstrap](#bootstrap)). It also still runs on the C++ host, which a host build uses to cross-check it.

The rule is the same as for the other copies in `selfhost/`: the C program prints what `relic FILE` prints, and fails with the same message at the same line and column.

## Contents

- [Using it](#using-it) (and [in CMake](#in-cmake))
- [What it covers](#what-it-covers)
- [Shape of the generated C](#shape-of-the-generated-c)
- [Names and scopes](#names-and-scopes)
- [Modules](#modules)
- [Errors and positions](#errors-and-positions)
- [Tests](#tests)
- [Compiling itself](#compiling-itself)
- [Bootstrap](#bootstrap)
- [Speed](#speed)
- [Decided choices](#decided-choices)
- [Open choices](#open-choices)

## Using it

```sh
build/relicc prog.relic -o prog.c
cc -std=c99 -O2 -I runtime/c -o prog prog.c build/runtime/c/librelic_rt.a -lm
./prog
```

On the host the same is `relic selfhost/cgen.relic prog.relic -o prog.c`; both write the same C. With MSVC: `cl /std:c11 /O2 /I runtime\c prog.c build\runtime\c\Release\relic_rt.lib`. In CMake, link the `relic_rt` target and call `relic_c_target(name)` for the same strict flags the runtime uses.

Exit codes: 0 when the C was written, 1 when the file has a lex or parse error (printed the way the host prints it), 2 when the program uses something not supported yet (the message names it, its file and position; nothing in the language is refused today), 3 for bad usage.

`--stub` is for builds that compile every file in a folder. On a lex or parse error it writes a small C program that prints that same error and exits 1. On an unsupported program it writes one that prints `cgen-skip: <reason>` and exits 0. Either way cgen itself exits 0 and prints nothing.

`--root DIR` writes the file names that go into the C (the comment at the top and `RLC_FILES`, used in error messages) relative to `DIR` when the file is inside it. `bootstrap/cgen.c` is written with `--root` set to the repo root, so it is the same wherever the checkout is.

### In CMake

`cmake/Relic.cmake` (Daniel's pick, Oct 21 2026) gives `relic_add_program(name file [GRAPHICS] [DEPENDS files...])`. It runs `relicc` on `file` to write `relic_programs/name.c` in the build folder, builds that with `relic_rt` and the runtime's strict flags, and makes the executable target `name`. It is there after `add_subdirectory` of the Relic tree, or in the tree itself:

```cmake
add_subdirectory(relic)                       # the Relic checkout, e.g. a submodule
relic_add_program(hello src/hello.relic)
relic_add_program(tool tools/tool.relic DEPENDS ${CMAKE_SOURCE_DIR}/shared/util.relic)
relic_add_program(paint apps/paint.relic GRAPHICS)   # links relic_rt_gfx (SDL2)
```

`GRAPHICS` links the program with `relic_rt_gfx`, the runtime with the SDL2 graphics module, which exists when Relic is configured with `-DRELIC_C_SDL=ON`. Without that option the program links `relic_rt` with a status message: `std/graphics` loads and `available()` is false, so a program that checks it still runs. Programs without `GRAPHICS` never link SDL2.

When the C is written again: when `relicc` changes, when `file` changes, or when any `.relic` file under the folder of `file` (subfolders too) or in Relic's `std/` changes. That is a safe superset of what the program uses, since a module is usually in the program's folder or below it. A module somewhere else (`use ../shared/util`) is not seen on its own, so list it under `DEPENDS`. A new `.relic` file is picked up at the next build (the folder is globbed with `CONFIGURE_DEPENDS`), and files inside the build folder are left out.

Added to another project, Relic builds `relicc` and the runtime but leaves out its own 135 compiled goldens (`RELIC_CGEN_TESTS` defaults to off there). The program reads `std/` from the Relic tree it was built from; `$RELIC_STD` overrides that at run time.

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

Graphics: by default the runtime's stub answers, so `std/graphics` loads and reports that graphics are not available, as a non-SDL host does. A program built with `relic_add_program(... GRAPHICS)` in a build with `-DRELIC_C_SDL=ON` links the SDL2 module and opens windows, draws and takes input like the host ([C-RUNTIME.md](C-RUNTIME.md#graphics)). cgen writes the same C either way; the choice is made when the program is linked.

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

A `let` with an `as` hint makes later assignments check the type, until a `let` without one clears it. When a name's `let`s disagree about the hint and the name is in a scope a nested function can reach, the hint in effect is kept in a hidden slot next to it, so an assignment from the nested function checks against the latest `let` that ran. That is what the tree-walker (`relic --tree`) and the VM do (golden `hint_relet`). A hinted parameter starts its slot with the parameter's hint.

## Modules

Modules are compiled into the same C file as the program (option 1, decided by Daniel, Oct 21 2026, see [Decided choices](#decided-choices)). All the code that knows this is in the "modules" section of `cgen.relic`; per-module C files would change only that section.

- While compiling, each `use` is resolved the way the host resolves it: `std/name` is `host_std_root()` plus `name.relic`, anything else is relative to the folder of the file that has the `use`. The key is the absolute path with `.` and `dir/..` folded away. A file seen before is not compiled again.
- The main file is module 0, registered before anything runs, so a module that uses it back gets the main scope instead of running the main file twice (golden `use_main_cycle`).
- `use` runs `rlc_use_i`. The first time, it makes the module's scope and value and runs its top level. After that, and while it is still running (a cycle), it hands out the same value, so names the module has not reached yet are not there "(yet)" (goldens `use_cycle`, `err_use_field`). A module that failed partway stays loaded as far as it got (golden `use_cache`).
- A missing file fails at the `use` with `cannot find module 'x' (no file x.relic)`. A module with a lex or parse error fails at the `use` with that error at its own file and position (golden `err_use_parse`).
- A module value (`rl_module_scoped`) reads its names from the module's top-level scope, with the host's messages for private `_names` and missing ones.
- The std boundary: a std function called from code outside `std/` that fails inside `std/` reports the error at the call, and a `host_*` message gets the std name: `files.list(): cannot list x` (goldens `err_std_name`, `std_folders`). Generated code outside `std/` calls `rl_std_boundary` after a failed call; functions defined in a std module carry their module's name (`rl_fun_std`).

## Errors and positions

Positions use a mark at each call site that can fail (option A, decided by Daniel, Oct 21 2026, see [PROJECT_PLAN.md](PROJECT_PLAN.md#decisions-needed)). The first argument says which module's file the code is in:

```c
if (rl_sub(cx, L[0], rl_int(1LL), &T[2])) RLC_ERR(0, 5, 18, 5, 18, 5, 18, fail);
```

`RLC_ERR` takes the node's line and column plus those of its first and second operand. The runtime says which one an error is about (`cx->err_arg`; index and key errors point at the index, a bad range end at that end), and `rl_at3` picks it. `on_error()` in `cgen.relic` and the `RLC_ERR` macro are the only places that know how positions are recorded, so moving to a per-function line table later changes those two and nothing else.

Errors such as `'x' is not defined`, `'x' is not declared; use 'let x = ...'`, `'break' outside a loop`, failed hints and `int is too large` come out when the program reaches them, as on the host, with the host's text.

## Tests

Every golden program in `tests/golden/` except REPL tests (`repl_*`) and those with a `.args` file (they test CLI flags) has a ctest case `cgen_name`. At build time cgen writes `build/cgen/name.c` (with `--stub`; `relicc` in the default build, cgen on the host in a host build), CMake compiles it with the runtime's flags (`-std=c99 -Wall -Wextra -pedantic`, or `/W4` on MSVC), and `tools/run_cgen.cmake` runs it in `tests/golden/` with `RELIC_LEAK_CHECK=1` and compares stdout with `name.out`, or stderr with `name.err`. A program cgen refused would show as skipped; none is today.

Today: 135 cases, all pass. Cases for the compiled side only can go in `tests/cgen/`, with expected output from `relic --tree` (`cgen_only_name`); there are none now. The generated C compiles with no warnings, so CI builds it with warnings as errors. The one exception is MSVC's C4702 (unreachable code), turned off at the top of each generated file: the code after a jump to `fail:` or a `catch` label is still compiled, and that is expected. `relic_cgen_test(name file)` in the top-level `CMakeLists.txt` adds a case for any other program with a golden `.out`, and `relic_cgen_exe(name file)` just builds one.

The `build`, `no-cxx` and `cgen` CI jobs build and run these with relicc, and the `host` jobs with the host. The `cgen` job does it strict (warnings as errors) on gcc and clang on Linux (with ASan, UBSan and LSan, UBSan errors fatal), clang on macOS and MSVC on Windows. `-DRELIC_CGEN_TESTS=OFF` leaves them out of a build.

## Compiling itself

The Relic-written front end and cgen compile to C, and the results are checked in ctest:

| Test | Checks |
|---|---|
| `cgen_selfhost_lexer` | `selfhost/tokens.relic` compiled to C prints exactly what `relic --tokens` prints, for every golden, helper, `std/` and `selfhost/` file (173 today) |
| `cgen_selfhost_parser` | `selfhost/ast.relic` compiled to C prints exactly what `relic --ast` prints, for the same files |
| `cgen_fixed_point` | cgen compiled by cgen on the host (stage 1) writes, for `selfhost/cgen.relic`, exactly the C that cgen on the host (stage 0) wrote. Stage 2 is built from that same C, so it is the same program: a fixed point. It runs with the leak check on |

These three need the host (`-DRELIC_BUILD_HOST=ON`). The C-only build has its own fixed point check, `bootstrap_fixed_point` (below).

Also checked by hand: stage 1 writes the same C as stage 0 for all 131 compiled goldens, and `selfhost/run.relic` compiled to C runs 127 goldens (all but the REPL and `.args` ones and the four `selfhost_interp` skips) and matches the host on every one.

## Bootstrap

C5, Daniel's choice (Oct 21 2026): the C for cgen is committed, so a fresh checkout builds with only a C compiler, and the C++ host stays as an optional cross-check.

- `bootstrap/cgen.c` is `selfhost/cgen.relic` with its five modules (`lexer`, `parser`, `errfmt`, `std/files`, `std/strings`) compiled to one C file by cgen with `--root` at the repo root, plus a comment on top (kept in `tools/bootstrap.cmake`). About 21,000 lines, 1 MB. [bootstrap/README.md](../bootstrap/README.md) has the short version.
- The default build makes `relicc_stage0` from it and `runtime/c/`. `relicc_stage0` compiles `selfhost/cgen.relic` from the checkout to `build/bootstrap/stage1.c`, built as `relicc`. `relicc` compiles the goldens. A change to `selfhost/` or `std/` is in effect in the same build, before `bootstrap/cgen.c` is regenerated.
- `cmake -DBUILD=build -P tools/regen_bootstrap.cmake` rewrites `bootstrap/cgen.c`: it builds `relicc`, has it compile `cgen.relic` and writes that, round after round until nothing changes (one round, unless the change was to how cgen writes C, then two). If the committed C can no longer compile the new cgen, the host can write it: `relic selfhost/cgen.relic selfhost/cgen.relic --root . -o out.c`, then add the comment.

| Test | Build | Checks |
|---|---|---|
| `bootstrap_fresh` | all | `relicc_stage0` writes, for `cgen.relic`, exactly the committed C (after its comment). Fails when `bootstrap/cgen.c` is stale |
| `bootstrap_fixed_point` | all | `relicc` writes, for `cgen.relic`, exactly the C it was built from (stage 2 equals stage 1), with the leak check on |
| `bootstrap_matches_host` | host | cgen on the host writes exactly the committed C |
| `cgen_stage1_matches_host` | host | `relicc` writes exactly the C the host wrote, for every compiled golden (135) |

CI runs the C-only build on Linux (gcc), macOS (clang) and Windows (MSVC) in the `build` job, strict with sanitizers in the `cgen` job, and on Alpine Linux with no C++ compiler installed in the `no-cxx` job, which first checks that `c++`, `g++` and `clang++` are all missing. The `host` job builds the host on all three and runs everything, cross-checks included.

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

Two fixes got there: indexing a string whose characters are all one byte is now direct (it used to walk from the start, which made the compiled lexer slower than the host), and `strings.join` joins halves instead of growing one string. A long string with multi-byte characters (256 bytes or more) gets a table of the byte offset of every 64th character on its first index, so an index walks at most 63 characters; a loop over 40,000 characters, half of them two bytes, went from 1.3 s to 17 ms (the host took 0.8 s there; it now keeps the same table for the last few strings it indexed).

The hand-written C in `runtime/c/tests/demo.c` ran `fib(30)` in about 84 ms, so the generated code has room to improve: it copies more values than it needs to and calls through `rl_call` every time.

## Decided choices

Recorded in [PROJECT_PLAN.md](PROJECT_PLAN.md#decisions-needed):

- `gc()`'s count is approximate in compiled code (it makes fewer scopes than the host). Decided by Engineer, Oct 20 2026, internal; revisit with "count only Relic values" if anything depends on the number.
- Locals in functions with no nested `fun` are released when their block starts again or the function returns, not at block end. Decided by Engineer, Oct 20 2026, internal.
- A program with something cgen does not support is refused as a whole. Decided by Engineer, Oct 20 2026, internal.
- Re-hinted names assigned from a nested function check the hint of the latest `let` that ran; the VM was fixed to do the same. Decided by Engineer, Oct 20 2026.
- The std is compiled in from source each time (from `$RELIC_STD` or the build's `std/`). `host_std_root()` in the program still reports the runtime's folder. Decided by Engineer, Oct 20 2026.
- Compiled errors name the file, line and column; `rl_report` prints the source line only if it can read the file when the error happens. No source text is embedded in the C for now. Decided by Engineer, Oct 20 2026.
- Modules: one C file per program with every module it uses compiled in. Decided by Daniel, Oct 21 2026. The emission is isolated, so per-module C files would still be a contained change.
- Error positions: a mark at each call site that can fail. Decided by Daniel, Oct 21 2026.
- C5: the C for cgen is committed in `bootstrap/`, and the C++ host is an optional cross-check. Decided by Daniel, Oct 21 2026.
- Building a program in one step: the CMake helper `relic_add_program(name file)`, not a `relicc run` mode. Decided by Daniel, Oct 21 2026.
- `relic_add_program` reruns relicc when anything under the program's folder or `std/` changes (a superset), with `DEPENDS` for modules elsewhere, instead of an exact list of the modules used. Decided by Engineer, Oct 21 2026, internal.
- The committed C is one file with names relative to the repo root; a stale file fails `bootstrap_fresh` rather than being rewritten by the build; the default build compiles the goldens with `relicc`, a host build with the host plus `cgen_stage1_matches_host`; `-DRELIC_SDL=ON` turns the host on. Decided by Engineer, Oct 21 2026, internal.

## Open choices

None right now.
