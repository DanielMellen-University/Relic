# Roadmap

Phases run in order, with one exception: the memory model and modules from phase 4 come before self-hosting (see [PROJECT_PLAN.md](PROJECT_PLAN.md)). A phase is done when every box is ticked.

## Phase 1: Host bootstrap

- [x] Pick the host language (**Default:** C++17, to match MONOLITH 1)
- [x] Build setup (**Default:** CMake) and CI that runs on every push
- [x] `relic` prints `hello` from a golden test (`tests/golden/hello.relic`) (see [BOOTSTRAP.md](BOOTSTRAP.md))
- [x] Test runner that runs `.relic` files and compares output
- [x] Track every borrowed piece in a list so it can be replaced later

## Phase 2: Language core

- [x] Lexer: turns source into tokens
- [x] Parser: turns tokens into an AST
- [x] Tree-walk interpreter over the AST
- [x] REPL
- [x] Numbers, strings, bools, variables
- [x] Indentation blocks, `if`, `while`, `for`, `fun` functions, lists, maps
- [x] Clear error messages with file, line and column
- [x] Meets the [1.0 checklist](1.0-CHECKLIST.md); tagged `v1.0.0`. **Open:** the human FizzBuzz-from-the-README check

## Phase 3: Self-host

The goal, approved by Daniel in October 2026: a Relic compiler written in Relic that emits C, built by a normal C compiler, so no C++ language code is left. The C++ host was stage 0 until the compiler could compile itself (C4); since C5 the committed C in `bootstrap/` is stage 0 and the host is optional. Milestone details are in [PROJECT_PLAN.md](PROJECT_PLAN.md#self-host-and-bootstrap).

Relic-written front end and interpreter. These run on the host today and are kept as a testing aid; the interpreter is not the end state.

- [x] Lexer rewritten in Relic (`selfhost/lexer.relic`, matches `--tokens` on every file)
- [x] Parser rewritten in Relic (`selfhost/parser.relic`, matches `--ast` on every file)
- [x] Interpreter rewritten in Relic (`selfhost/interp.relic`, tree-walk; `use` for local files and `std/`)
- [x] SH1: the self-hosted interpreter runs real programs with their args (`selfhost_args`)
- [x] SH2: it runs itself: `run.relic` on `run.relic` on a test program (`selfhost_nested`)
- [ ] SH3: parity with the host: `num()` (done), deep recursion and `gc()` (open), every skipped comparison test passing
- [ ] SH4: `relic check` and the REPL written in Relic

Compiler to C:

- [x] C1: C runtime: values, strings, lists and maps, reference counts plus cycle collection, errors, `host_*` primitives in plain C99 (`runtime/c/`, [C-RUNTIME.md](C-RUNTIME.md))
- [x] C2: Relic-to-C code generation, compiled and run in CI against golden outputs (`selfhost/cgen.relic`, [CGEN.md](CGEN.md)): everything but `use`, 106 of 129 goldens, `fib(30)` 2.6x faster than the host VM
- [x] C3: full language coverage: modules and the std (closures, `try` / `catch` and hints came in C2); all 131 compiled goldens pass
- [x] C4: stage 1: the compiler compiles itself to C, and stage 1 output equals stage 0 output (fixed point, `cgen_fixed_point`); the compiled lexer and parser match the host on every file
- [x] C5: the C++ host leaves the default build; it stays only as an optional reference (`-DRELIC_BUILD_HOST=ON`). `bootstrap/cgen.c` is committed, a fresh checkout builds with only a C compiler (CI also checks this on Alpine with no C++ compiler), and CI fails if the committed C is stale

## Phase 4: Runtime and standard library

- [x] Memory model: reference counting plus a cycle collector (see ARCHITECTURE.md)
- [x] Language feature complete: `try` / `catch`, int and float, `as` hints, private `_` names, UTF-8 strings
- [x] Modules and imports: `use path/name`, cycles allowed
- [x] Standard library: strings, lists, maps, files, time, math (`std/`, M8); later paths, json, os, vfs and graphics
- [x] Bytecode VM (host, M10): default engine, 1.2x to 2.1x faster on `bench/`. A VM written in Relic is no longer planned; the C backend (phase 3) takes its place
- [ ] Native code generation explored (stretch; the C backend in phase 3 is the first step)

## Phase 5: Platform for the desktop

The desktop itself (window manager, taskbar, Start menu, apps) is built in Relic in [M0N0LITH-2](https://github.com/DanielMellen-University/M0N0LITH-2) and tracked in its roadmap. This phase covers what the language gives it.

- [x] Windowing and drawing API for Relic programs (SDL2 `std/graphics` with drawing, keyboard/mouse input, text, BMP and PNG images, TrueType fonts; optional build, M11). Compiled programs have it too since 1.0.9 (`runtime/c/rl_gfx_sdl.c` in `relic_rt_gfx`, `-DRELIC_C_SDL=ON`, `relic_add_program(... GRAPHICS)`), tested headless in the CI job `sdl` on Linux, macOS and Windows (1.0.10). 1.0.11 added the last slice Daniel picked (option B): window title, size and fullscreen, cursor shapes and visibility, vsync and frame timing, and the pixel scale. 1.0.12 made drawing sharp on high-DPI screens with coordinates kept in window units (Daniel's pick A). Done; new needs come from the M0N0LITH-2 apps (phase 6)
- [x] Virtual filesystem reachable from Relic (`std/vfs`, golden `std_vfs`)
- [x] Desktop shell moved out to M0N0LITH-2 with its history (1.0.1)

## Phase 6: Apps as proof

The apps (terminal, text editor, file browser, drawing app, a game and the Relic IDE) are tracked in the [M0N0LITH-2 roadmap](https://github.com/DanielMellen-University/M0N0LITH-2/blob/main/docs/ROADMAP.md). Relic's job here is to fix whatever those apps run into.
