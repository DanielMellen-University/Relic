# Project plan

This plan turns the [roadmap](ROADMAP.md) into milestones you can work through in order. The table shows where each one stands.

## Milestone map

| # | Milestone | Depends on | Status |
|---|---|---|---|
| M0 | Repo and docs | none | done |
| M1 | Host skeleton: build, CI, test runner | M0 | done |
| M2 | Lexer | M1 | done |
| M3 | Parser and AST | M2 | done |
| M4 | Tree-walk interpreter | M3 | done |
| M5 | REPL and file runner | M4 | done |
| M6 | Relic 1.0 release | M5 | done (`v1.0.0`; human FizzBuzz check still open) |
| M7 | Memory model and modules | M6 | done (cycle collector added later) |
| M8 | Standard library core | M7 | done |
| M9 | Self-host | M8 | done (lexer, parser, interpreter in Relic; limits in ARCHITECTURE.md) |
| M10 | Bytecode VM | M9 | host VM done and default; a VM in Relic is no longer planned (see C1 to C5) |
| M11 | Graphics API | M10 | SDL2 slices 1 to 3 done; `std/vfs` done. The desktop shell moved to M0N0LITH-2 |
| M12 | Apps and IDE | M11 | moved to M0N0LITH-2 (the IDE is planned there, not started) |
| SH1 | Self-hosted interpreter runs programs with args | M9 | done |
| SH2 | Self-hosted interpreter runs itself | SH1 | done |
| SH3 | Self-host parity: recursion, `gc()`, `num()` | SH2 | `num()` done; rest open, low priority |
| SH4 | `check` and REPL in Relic | SH2 | not started, low priority |
| C1 | C runtime | M9 | done: `runtime/c/`, tests `c_runtime` and `c_demo` |
| C2 | Relic-to-C codegen, core subset, in CI | C1 | done (1.0.4): `selfhost/cgen.relic`, 106 of 129 goldens, the other 23 need modules |
| C3 | Full language in C | C2 | not started |
| C4 | Stage 1 fixed point | C3 | not started |
| C5 | C++ host out of the default build | C4 | not started |

Phases in the roadmap map to milestones like this: phase 1 is M1, phase 2 is M2 to M6, phase 3 is M9, SH1 to SH4 and C1 to C5, phase 4 is M7, M8 and M10, phase 5 is M11, phase 6 is M12.

**Default:** the memory model and modules (phase 4 work) come before self-hosting, because a self-hosted compiler needs imports and real memory handling. That pulls part of phase 4 ahead of phase 3.

## Milestones

### M1: Host skeleton
- Work: folder layout from [ARCHITECTURE.md](ARCHITECTURE.md), CMake build, CI, golden test runner from [TESTING.md](TESTING.md).
- Exit: `relic --version` prints a version. CI builds on push. One empty golden test passes.
- Risk: build tool churn. Keep to CMake and the standard library.

### M2: Lexer
- Work: tokens for numbers, strings, names, keywords, operators, `#` comments, and INDENT/DEDENT from leading spaces.
- Exit: a `--tokens` debug flag prints tokens for any file. Lexer tests pass, including bad input.
- Risk: string escapes and UTF-8. Support `\n \t \" \\` and `{}` interpolation only in 1.0. Indentation errors need clear messages.

### M3: Parser and AST
- Work: expressions with precedence, statements, blocks, functions.
- Exit: a `--ast` flag prints the tree. Syntax errors give file, line and column.
- Risk: syntax is not settled. Lock the 1.0 syntax in DESIGN.md before starting M3 (done; the rules now live in LANGUAGE.md).

### M4: Interpreter
- Work: walk the AST, environments for scope, function calls, lists, `print`.
- Exit: FizzBuzz, factorial and a sort program pass as golden tests.
- Risk: closures and scope bugs. Add scope tests early.

### M5: REPL and file runner
- Work: `relic` opens a REPL, `relic file.relic` runs a file. Same pipeline for both.
- Exit: REPL keeps state between lines and survives errors.

### M6: Relic 1.0
- Exit: every box in [1.0-CHECKLIST.md](1.0-CHECKLIST.md) ticked, CI green, tag `v1.0.0`.
- Status: tagged `v1.0.0` with CI green on all three OSes. **Open:** a person still needs to write FizzBuzz from the README.

### M7 to M12
Planned at a coarser level than M1 to M6. Status under each exit line.
- M7 exit: memory model chosen and written up. Two files can import each other. **Done:** reference counting (LANGUAGE.md, ARCHITECTURE.md), `use` with cycles, golden tests `use_*` and `err_use_*`.
- M8 exit: stdlib covers strings, lists, maps, files, time, math, each with tests. **Done:** `std/*.relic`, golden tests `std_*`.
- M9 exit: the Relic lexer, parser and interpreter written in Relic pass the same golden tests as the host.
  - Stage 1, lexer: **done.** `selfhost/lexer.relic` matches `relic --tokens` on every golden, std and selfhost file, errors included (`selfhost_lexer` test).
  - Stage 2, parser: **done.** `selfhost/parser.relic` gives the same tree and the same errors as `relic --ast` on every file (`selfhost_parser` test).
  - Stage 3, interpreter: **done.** `selfhost/interp.relic` runs 130 golden programs, `std_*` included, with the same stdout and stderr as the host (`selfhost_interp` test). Skipped, see ARCHITECTURE.md: REPL tests, `rt_recursion`, `try_recursion` and `gc_cycles`.
  - **M9 done** (with those limits). The C++ host still runs the Relic-written stages; dropping the host for good needs a faster runtime (M10). `use std/...` works in the self-hosted interpreter too, since the MONOLITH 2 host pieces added `host_call`.
- M10 exit: VM passes all golden tests and beats the tree-walker on a benchmark set.
  - Host VM: **done.** The VM is the default engine for `relic FILE`, including `use`, cycles and `std/`. It matches the tree-walker (`relic --tree FILE`) on all 133 golden programs (everything except the REPL tests) (`vm_matches_tree_walker` test) and is 1.2x to 2.1x faster on `bench/` (`tools/bench.sh`, local only). The REPL still runs on the tree-walker.
  - The VM written in Relic is dropped: Daniel picked a compiler to C as the bootstrap (see below). VM speed work stays possible but is not planned.
- M11 exit: a window opens, draws and takes input from a Relic program. The shell running on it is tracked in M0N0LITH-2.
  - Slice 1: **done.** `std/graphics` (open, clear, rect, pixel, present, quit_requested, wait) on SDL2 behind `-DRELIC_SDL=ON`; `examples/hello_window.relic`. 
  - Slice 2: **done.** Keyboard and mouse input, built-in bitmap text, BMP images; `examples/input_demo.relic`; headless `gfx_smoke` ctest in SDL builds.
  - Slice 3: **done.** PNG and JPG via optional SDL2_image, TrueType text via optional SDL2_ttf.
  - Shell: started here (window model, Start menu, drag, resize, Terminal, Files, Editor), then moved with its history and the `shell_wm` test to [M0N0LITH-2](https://github.com/DanielMellen-University/M0N0LITH-2) in 1.0.1. In-memory `std/vfs` (golden `std_vfs`) stays in Relic.
- Language completion (after M11 slice 2): **done.** Catchable errors, int and float, `as` hints, private names, UTF-8 strings and the cycle collector, each with golden tests, on both engines and the self-hosted interpreter.
- M12 exit: terminal, editor, file browser, drawing app, one game and the IDE run in Relic. All of these, the IDE included, are apps built in M0N0LITH-2; Relic fixes what they find.

## Self-host and bootstrap

**Decided (Daniel, October 2026):** the end state is a Relic compiler written in Relic that emits C, built by a normal C compiler. No C++ language code is left. The other options considered were a Relic interpreter on a small C++ runtime, and a Relic compiler emitting bytecode for a small C++ VM. The C++ host stays as stage 0 until C4.

### SH1 to SH4: the Relic-written interpreter

A testing aid that checks the Relic-written front end on real programs. Not the end state, so speed and deep recursion get little time.

- SH1 exit: `relic selfhost/run.relic FILE args...` passes the args through. **Done:** `selfhost_args`.
- SH2 exit: `run.relic` runs `run.relic` running a test program. **Done:** `selfhost_nested`, about 0.7 s. A third level runs out of host recursion.
- SH3 exit: no skipped comparison tests. `num()` now gives the host's answers (golden `num_edges`). Still open: recursion stops at about 130 Relic calls (the host's 800-call limit, with several host calls per Relic call), and `gc()` is missing.
- SH4 exit: `relic check` and the REPL run as Relic programs and match the host.

### C1 to C5: the compiler to C

- C1 exit: a C runtime (values, strings, lists and maps, reference counts plus cycle collection, errors and `try`, `host_*` primitives in plain C), with the choices below decided by Daniel. **Done:** `runtime/c/`, plain C99, built next to the host and in its own strict CI job (gcc and clang with ASan, UBSan and LSan on Linux, clang on macOS, MSVC on Windows). Tests: `c_runtime` (unit, cycle and leak checks) and `c_demo` (a program compiled by hand the way C2 will, matching the host's output). The ABI is in [C-RUNTIME.md](C-RUNTIME.md). Left for later milestones: loading compiled modules and the std-boundary error names (C2, C3), the SDL2 graphics module (after C2).
- C2 exit: a Relic program in `selfhost/` turns a core subset (numbers, strings, lists, maps, functions, control flow) into C. CI compiles that C and runs it against golden outputs on all three OSes. **Done:** `selfhost/cgen.relic` ([CGEN.md](CGEN.md)) covers the whole language except `use`, so closures, `try` / `catch` and hints already work. Every golden except REPL and `.args` tests is a ctest case `cgen_name`: 106 of 129 pass and 23 skip because they use modules; none fail. The generated C builds with warnings as errors and runs clean under ASan, UBSan and LSan, with a leak check at exit. CI job `cgen` runs them on Linux (gcc and clang), macOS and Windows (MSVC). `fib(30)`: 141 ms against 363 ms on the host VM.
- C3 exit: modules and the std compile (closures, `try` / `catch` and hints came early, in C2), and every golden program matches. Also left from C2: one refused corner (a name with differently hinted `let`s used from a nested function), and the open choices in [CGEN.md](CGEN.md#open-choices).
- C4 exit: the compiler compiles itself. Stage 1 (compiled by the stage 0 host) and stage 2 (compiled by stage 1) produce identical C: a fixed point.
- C5 exit: the default build is the C runtime plus generated C. The C++ host builds only on request, as a reference to compare against.

## Risks

| Risk | Effect | Plan |
|---|---|---|
| Syntax keeps changing | Rework in parser and tests | Freeze 1.0 syntax before M3 |
| Memory model picked late | Rewrite of runtime | Decided in M7: reference counting |
| Self-host too large | Stall at phase 3 | Ported one stage at a time; all three stages done |
| Self-hosted side too slow | Host never retired | Compile to C instead of interpreting (C1 to C5) |
| Tree-walker too slow for desktop | Laggy shell | Host VM done and default; more speed work before the shell |
| Borrowed pieces creep in | Breaks the domestic goal | Keep the borrowed list in VISION.md current |
| Generated C differs by compiler or OS | C4 fixed point fails on one OS | C99 on gcc, clang and MSVC (decided); the runtime is built strict on all three in CI |

## Decisions needed

Struck-through rows are decided. The rest are **Open** and block the milestone shown.

| Decision | Blocks |
|---|---|
| ~~Final 1.0 syntax~~ frozen in M3; colons, `++` and `const` stay out of 1.0 | done |
| ~~Number types~~ separate 64-bit `int` and `float` | done |
| ~~Catchable errors~~ `try` / `catch err`, `fail(message)` | done |
| ~~Type hints~~ optional `as` hints, checked at run time | done |
| ~~Private names~~ leading `_` in a module | done |
| ~~Strings~~ UTF-8, counted by character | done |
| ~~License~~ MIT, chosen by Daniel | done |
| ~~Memory model~~ reference counting plus a cycle collector | done |
| ~~Module system shape~~ `use path/name`, one file one module | done |
| ~~Graphics backend for phase 5~~ SDL2 by default (matches MONOLITH 1), optional build | done |
| ~~Bootstrap strategy~~ Relic compiler in Relic emitting C | done |
| ~~Memory in the C runtime~~ reference counts plus a cycle collector, like the host (Decided, Daniel, Oct 20 2026) | done |
| ~~C standard and compilers~~ C99 on gcc, clang and MSVC (Decided, Daniel, Oct 20 2026) | done |
| ~~How `try` / `catch` works in C~~ an error flag returned from every call, checked by generated code (Decided, Daniel, Oct 20 2026) | done |
| ~~Graphics in C~~ an optional C module over SDL2 (Decided, Daniel, Oct 20 2026) | done |
| Error positions in generated C. **Interim decision Oct 20 2026: option A, pending Daniel's confirmation.** Option A: generated code marks the position at each call site that can fail (`rl_at` style). Option B: a line table per function, looked up when an error leaves it. A is in place behind one helper (`on_error()` in `cgen.relic` and the `RLC_ERR` macro), so moving to B later is a local change | nothing yet |
| What `gc()` returns in compiled code: approximate, exact like the host, or only Relic values in both; see [CGEN.md](CGEN.md#open-choices) (**Open**) | nothing yet |
| When stack-held locals in compiled code are freed: on block re-entry and return (now), or also at block end (**Open**) | nothing yet |
| Programs outside the compiled subset: refuse the file (now) or fail when the part runs (**Open**) | nothing yet |
| How modules compile: one C file per program, one per module with init functions, or that plus a load table (**Open**) | C3 |
