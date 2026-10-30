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
| M11 | Graphics API | M10 | SDL2 slices 1 to 3 done; `std/vfs` done; C module over SDL2 for compiled programs done (1.0.9); window, cursor and frame slice done (1.0.11); high-DPI done (1.0.12). The roadmap item is closed. The desktop shell moved to M0N0LITH-2 |
| M12 | Apps and IDE | M11 | moved to M0N0LITH-2 (the IDE is planned there, not started) |
| SH1 | Self-hosted interpreter runs programs with args | M9 | done |
| SH2 | Self-hosted interpreter runs itself | SH1 | done |
| SH3 | Self-host parity: recursion, `gc()`, `num()` | SH2 | `num()` done; `gc()` done (1.0.13); deep recursion done (option C, frame stack, slices 1 to 6 in 1.0.14, 1.0.15, 1.0.19, 1.0.21 and 1.0.22) |
| SH4 | `check` and REPL in Relic | SH2 | done: `check` in 1.0.27 (`selfhost/check.relic`), the REPL in 1.0.31 (`selfhost/repl.relic`, on the new `os.read_line` / `os.prompt`) |
| C1 | C runtime | M9 | done: `runtime/c/`, tests `c_runtime` and `c_demo` |
| C2 | Relic-to-C codegen, core subset, in CI | C1 | done (1.0.4): `selfhost/cgen.relic`, 106 of 129 goldens, the other 23 need modules |
| C3 | Full language in C | C2 | done (1.0.5): modules and the std compile; all 131 compiled goldens pass |
| C4 | Stage 1 fixed point | C3 | done (1.0.5): cgen compiled by cgen writes the same C as cgen on the host (`cgen_fixed_point`, all OSes) |
| C5 | C++ host out of the default build | C4 | done (1.0.7): `bootstrap/cgen.c` committed; the default build needs only C; the host is `-DRELIC_BUILD_HOST=ON` |

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
  - Stage 3, interpreter: **done.** `selfhost/interp.relic` runs every golden program but the REPL ones (182 today, the same set as `selfhost_interp` in TESTING.md), `std_*` included, with the same stdout and stderr as the host (`selfhost_interp` test). `gc_cycles` runs since 1.0.13, and `rt_recursion`, `try_recursion` and `c_demo` since 1.0.21; see ARCHITECTURE.md.
  - **M9 done** (with those limits). The C++ host still runs the Relic-written stages; dropping the host for good needs a faster runtime (M10). `use std/...` works in the self-hosted interpreter too, since the MONOLITH 2 host pieces added `host_call`.
- M10 exit: VM passes all golden tests and beats the tree-walker on a benchmark set.
  - Host VM: **done.** The VM is the default engine for `relic FILE`, including `use`, cycles and `std/`. It matches the tree-walker (`relic --tree FILE`) on all 133 golden programs (everything except the REPL tests) (`vm_matches_tree_walker` test) and is 1.2x to 2.1x faster on `bench/` (`tools/bench.sh`, local only). The REPL still runs on the tree-walker.
  - The VM written in Relic is dropped: Daniel picked a compiler to C as the bootstrap (see below). VM speed work stays possible but is not planned.
- M11 exit: a window opens, draws and takes input from a Relic program. The shell running on it is tracked in M0N0LITH-2.
  - Slice 1: **done.** `std/graphics` (open, clear, rect, pixel, present, quit_requested, wait) on SDL2 behind `-DRELIC_SDL=ON`; `examples/hello_window.relic`. 
  - Slice 2: **done.** Keyboard and mouse input, built-in bitmap text, BMP images; `examples/input_demo.relic`; headless `gfx_smoke` ctest in SDL builds.
  - Slice 3: **done.** PNG and JPG via optional SDL2_image, TrueType text via optional SDL2_ttf.
  - C module: **done (1.0.9).** `runtime/c/rl_gfx_sdl.c` gives compiled programs the same `std/graphics` as the host, over SDL2, in `relic_rt_gfx`, built with `-DRELIC_C_SDL=ON` (off by default; no host needed) and linked by `relic_add_program(name file GRAPHICS)`. The headless SDL tests and the mini desktop also run compiled (`c_gfx_*`, `c_mini_desktop`) with the same expected output and the leak check, in the CI job `sdl` (with the host's SDL tests), on Ubuntu, and since 1.0.10 on macOS (Homebrew) and Windows (vcpkg, MSVC) too.
  - Window, cursor and frames: **done (1.0.11).** Daniel picked option B on Oct 22 2026: one more slice before closing the roadmap item. Host and C module: `title`, `set_title`, `window_size`, `set_window_size`, `scale`, `fullscreen`, `set_fullscreen`, `set_cursor` (11 system shapes; false when the system cannot show one), `show_cursor`, `cursor_visible`, `set_vsync`, `vsync`, `ticks`, `frame_time`, `present_capped`. Tests `gfx_window` and `gfx_window_errors`, host and compiled (`c_gfx_window`, `c_gfx_window_errors`), with the same expected output. The high-DPI part followed in 1.0.12.
  - High-DPI: **done (1.0.12).** Daniel picked option A on Oct 22 2026. Windows open with `SDL_WINDOW_ALLOW_HIGHDPI` (and SDL's Windows DPI scaling hint), and the renderer is scaled by `scale()` so drawing and mouse positions stay in window units. TrueType fonts are opened at `size * scale()` pixels and drawn back at window-unit size; `font_size` gives window units, rounded up. Images and canvases keep their window-unit size. The scale is checked again on every input call, `present`, `set_window_size`, `set_fullscreen` and `draw_to(none)`, and fonts are reopened when it changes. The rounding is in `src/builtins/gfx_scale.h`, shared by the host and the C module and unit tested in `c_runtime`. Headless SDL has no high-DPI screens, so the tests check that the scale is 1.0 there and every existing output is unchanged.
  - Shell: started here (window model, Start menu, drag, resize, Terminal, Files, Editor), then moved with its history and the `shell_wm` test to [M0N0LITH-2](https://github.com/DanielMellen-University/M0N0LITH-2) in 1.0.1. In-memory `std/vfs` (golden `std_vfs`) stays in Relic.
- Language completion (after M11 slice 2): **done.** Catchable errors, int and float, `as` hints, private names, UTF-8 strings and the cycle collector, each with golden tests, on both engines and the self-hosted interpreter.
- M12 exit: terminal, editor, file browser, drawing app, one game and the IDE run in Relic. All of these, the IDE included, are apps built in M0N0LITH-2; Relic fixes what they find.

## Self-host and bootstrap

**Decided (Daniel, October 2026):** the end state is a Relic compiler written in Relic that emits C, built by a normal C compiler. No C++ language code is left. The other options considered were a Relic interpreter on a small C++ runtime, and a Relic compiler emitting bytecode for a small C++ VM. The C++ host stays as stage 0 until C4.

### SH1 to SH4: the Relic-written interpreter

A testing aid that checks the Relic-written front end on real programs. Not the end state, so speed and deep recursion get little time.

- SH1 exit: `relic selfhost/run.relic FILE args...` passes the args through. **Done:** `selfhost_args`.
- SH2 exit: `run.relic` runs `run.relic` running a test program. **Done:** `selfhost_nested`, about 1 s. A third level runs out of host recursion before 1.0.21; since then it works (slow).
- SH3 exit: no skipped comparison tests. `num()` now gives the host's answers (golden `num_edges`). `gc()` runs the host's collector and `gc_cycles` matches (1.0.13). Recursion reaches the host's 800-call limit since 1.0.21, with the host's error, catchable (before, it stopped at about 262 Relic calls for a block body and 395 for an expression body, with several host calls per Relic call), and `rt_recursion`, `try_recursion` and `c_demo` are compared; only the REPL tests are skipped. Daniel picked option C on Oct 24 2026: `eval` moves onto an explicit frame stack in 6 slices, each staying under 2x the 1.0.13 time of `selfhost_interp` (Daniel's budget). The slices: (1) the stack and driver loop in front of the recursive code, leaves answered at once (1.0.14, done); (2) expressions (1.0.15, done); (3) statements and control flow (1.0.19, done); (4) calls and returns with no host recursion and (5) `try` / `catch` and errors unwinding through calls, done together in 1.0.21 with `rt_recursion`, `try_recursion` and `c_demo` no longer skipped; (6) remove the old recursive path (`eval`, `exec_body`, `call`, `condition`, `assign`, `load_module`), done in 1.0.22 with golden `recursion_limit`. The depth limit and its error text stay the host's.
- SH4 exit: `relic check` and the REPL run as Relic programs and match the host. **`check` is done (1.0.27):** `selfhost/check.relic` gives the host's warnings, order and exit code on the 15 cases in `tests/check_cases` and on every `.relic` file in the tree (`selfhost_check`), compiled to C (`cgen_selfhost_check`) and on the Relic-written interpreter (`selfhost_check_nested`). **The REPL is done (1.0.31):** Daniel approved a standard input built-in on Oct 27 2026, so `host_read_line(prompt)` (wrapped as `os.read_line()` and `os.prompt(text)`) went into the host, the VM, the C runtime and cgen, and the self-hosted interpreter reaches it through `host_call`. `selfhost/repl.relic` gives the host REPL's output, errors and exit code on every script in `tests/golden/repl_*.relic` and `tests/repl_cases/` (`selfhost_repl`), compiled to C (`cgen_selfhost_repl`) and on the Relic-written interpreter (`selfhost_repl_nested`). A golden can have a `name.in` file that is fed to it on stdin.

### C1 to C5: the compiler to C

- C1 exit: a C runtime (values, strings, lists and maps, reference counts plus cycle collection, errors and `try`, `host_*` primitives in plain C), with the choices below decided by Daniel. **Done:** `runtime/c/`, plain C99, built next to the host and in its own strict CI job (gcc and clang with ASan, UBSan and LSan on Linux, clang on macOS, MSVC on Windows). Tests: `c_runtime` (unit, cycle and leak checks) and `c_demo` (a program compiled by hand the way C2 will, matching the host's output). The ABI is in [C-RUNTIME.md](C-RUNTIME.md). The SDL2 graphics module came later, in 1.0.9 (M11). (Compiled modules and the std-boundary names came in C3.)
- C2 exit: a Relic program in `selfhost/` turns a core subset (numbers, strings, lists, maps, functions, control flow) into C. CI compiles that C and runs it against golden outputs on all three OSes. **Done:** `selfhost/cgen.relic` ([CGEN.md](CGEN.md)) covers the whole language except `use`, so closures, `try` / `catch` and hints already work. Every golden except REPL and `.args` tests is a ctest case `cgen_name`: 106 of 129 pass and 23 skip because they use modules; none fail. The generated C builds with warnings as errors and runs clean under ASan, UBSan and LSan, with a leak check at exit. CI job `cgen` runs them on Linux (gcc and clang), macOS and Windows (MSVC). `fib(30)`: 141 ms against 363 ms on the host VM.
- C3 exit: modules and the std compile (closures, `try` / `catch` and hints came early, in C2), and every golden program matches (REPL tests and CLI-flag tests are not programs to compile). **Done:** cgen follows every `use` and compiles each module into the program's C file (interim decision below). First-use order, caching by path, cycles, a module that uses the main file back, a module that fails partway, missing modules, lex and parse errors in modules, private names, "(yet)" names and the std names at the boundary (`files.list(): ...`) all match the host. The C2 corner (a name with differently hinted `let`s, assigned from a nested function) compiles too and follows the tree-walker; the VM now does the same (golden `hint_relet`). All 131 compiled goldens pass, clean under ASan, UBSan and LSan and the leak check, on Linux, macOS and Windows. New goldens `use_cache` and `err_use_parse`.
- C4 exit: the compiler compiles itself. Stage 1 (compiled by the stage 0 host) and stage 2 (compiled by stage 1) produce identical C: a fixed point. **Done:** `cgen_fixed_point` builds stage 1 and checks that it writes, for `cgen.relic`, the same C as stage 0, so stage 2 is built from identical C. Stage 1 also writes the same C as stage 0 for every compiled golden (checked by hand). The compiled lexer and parser match `--tokens` and `--ast` on every corpus file (178 today) (`cgen_selfhost_lexer`, `cgen_selfhost_parser`). Stage 1 compiles cgen in about 0.2 s against about 1.1 s on the host. Still on the host: running stage 0 in the build to make stage 1, which is what C5 removes.
- C5 exit: the default build is the C runtime plus generated C. The C++ host builds only on request, as a reference to compare against. **Done (1.0.7):** `bootstrap/cgen.c` is cgen compiled to C by itself, committed (Daniel's choice, below). The default CMake build is C only: `relicc_stage0` from the committed C, then `relicc` (stage 1) from `selfhost/cgen.relic` in the checkout, which compiles every golden. `bootstrap_fresh` fails if the committed C is stale (`tools/regen_bootstrap.cmake` rewrites it) and `bootstrap_fixed_point` checks relicc writes the C it was built from. `-DRELIC_BUILD_HOST=ON` builds the host and all its tests as before, plus two cross-checks: the host writes `bootstrap/cgen.c` exactly (`bootstrap_matches_host`) and relicc writes the same C as the host for every golden (`cgen_stage1_matches_host`). CI proves the C-only build on Linux, macOS and MSVC, and on Alpine with no C++ compiler installed. cgen gained `--root DIR` so the committed C has no checkout paths in it.

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
| ~~Error positions in generated C~~ option A: generated code marks the position at each call site that can fail (`rl_at` style), rather than a line table per function (B). It sits behind one helper (`on_error()` in `cgen.relic` and the `RLC_ERR` macro) (Decided by Daniel, Oct 21 2026) | done |
| ~~What `gc()` returns in compiled code~~ approximate (option a); revisit with "count only Relic values" (c) if anything depends on the number (Decided by Engineer, Oct 20 2026, internal, no output change) | done |
| ~~When stack-held locals in compiled code are freed~~ on block re-entry and return, as now (a) (Decided by Engineer, Oct 20 2026, internal, no output change) | done |
| ~~Programs outside the compiled subset~~ refused as a whole (a) (Decided by Engineer, Oct 20 2026, internal, no output change) | done |
| ~~How modules compile~~ option 1: one C file per program with every module it uses compiled in, rather than one C file per module (2) or that plus loading by path at run time (3). The module emission is one section of `cgen.relic` (Decided by Daniel, Oct 21 2026) | done |
| ~~Re-hinted names assigned from a nested function~~ fix the VM to check at run time like the tree-walker (a); it was a bug (Decided by Engineer, Oct 20 2026) | done |
| ~~Where a compiled program's std comes from~~ compiled in from source each time (a) (Decided by Engineer, Oct 20 2026) | done |
| ~~Source lines in compiled programs' errors~~ no source text embedded for now (a) (Decided by Engineer, Oct 20 2026) | done |
| ~~How C5 bootstraps without the host~~ commit the generated C for cgen (`bootstrap/cgen.c`) so a fresh checkout builds with only a C compiler, and keep the C++ host as an optional cross-check, rather than the host as an optional stage 0 builder or both (Decided by Daniel, Oct 21 2026) | done |
| ~~How the committed C is laid out and kept fresh~~ one file, `bootstrap/cgen.c`, with a comment on top and file names relative to the repo root (`--root`); the build always makes stage 1 from the checkout's `selfhost/`, and a stale file fails `bootstrap_fresh` instead of being rewritten by the build (Decided by Engineer, Oct 21 2026, internal) | done |
| ~~Which compiler the compiled goldens use~~ `relicc` in the default build; in a host build the host runs `cgen.relic` as before and `cgen_stage1_matches_host` checks relicc writes the same C (Decided by Engineer, Oct 21 2026, internal) | done |
| ~~Building a compiled program in one step~~ a CMake helper, `relic_add_program(name file)` in `cmake/Relic.cmake`, usable in this tree and through `add_subdirectory`; no `relicc run` mode (Decided by Daniel, Oct 21 2026) | done |
| ~~What makes `relic_add_program` rerun relicc~~ the file, every `.relic` under its folder and `std/`, plus `DEPENDS` for modules elsewhere; not an exact module list from relicc (Decided by Engineer, Oct 21 2026, internal; an exact list would need a depfile flag in cgen and CMake 3.21) | done |
| ~~`-DRELIC_SDL=ON` without the host~~ it turns the host on, since graphics live there (Decided by Engineer, Oct 21 2026, internal) | done |
| ~~How compiled programs get graphics in the build~~ option B: a separate `relic_rt_gfx` library (`-DRELIC_C_SDL=ON`) and `relic_add_program(name file GRAPHICS)`, so only programs that ask link SDL2; `relicc` and the rest link `relic_rt` with the stub. Not chosen: the module inside `relic_rt` for the whole build (A), one switch for host and compiled (C), building it whenever SDL2 is found (D) (Decided by Daniel, Oct 21 2026) | done |
| ~~`GRAPHICS` in a build without `-DRELIC_C_SDL`~~ the program links the stub with a status message, so `available()` is false, rather than a configure error (Decided by Engineer, Oct 21 2026, internal) | done |
| ~~SDL in CI~~ one Ubuntu job, `sdl`, with the host and compiled graphics tests headless (Approved by Daniel, Oct 21 2026); extended to macOS and Windows in 1.0.10 (requested by Daniel, Oct 22 2026) | done |
| ~~How to finish the windowing and drawing item~~ option B: one more slice (window title, size and fullscreen, cursor, vsync and frame timing, pixel scale) before closing it, rather than closing it as is (A), multiple windows (C) or moving to SDL3 (D) (Decided by Daniel, Oct 22 2026) | done |
| ~~High-DPI drawing~~ option A: windows open in high-DPI mode and the renderer is scaled by `scale()`, so every coordinate stays in window units (no change for programs), shapes are drawn at full resolution, fonts are loaded at `size * scale()` for sharp text, and images are drawn at their unit size. Not chosen: pixel coordinates (B), an opt-in to B per window (C), leaving it to the system (D) (Decided by Daniel, Oct 22 2026) | done |
