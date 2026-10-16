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

- [x] Lexer rewritten in Relic (`selfhost/lexer.relic`, matches `--tokens` on every file)
- [x] Parser rewritten in Relic (`selfhost/parser.relic`, matches `--ast` on every file)
- [x] Interpreter rewritten in Relic (`selfhost/interp.relic`, tree-walk; `use` for local files, no `std/` yet)
- [ ] Relic builds and runs itself without the C/C++ host for language logic (still runs on the host; needs `std/` in the self-hosted interpreter and a Relic-written VM)

## Phase 4: Runtime and standard library

- [x] Memory model: reference counting plus a cycle collector (see DESIGN.md)
- [x] Language feature complete: `try` / `catch`, int and float, `as` hints, private `_` names, UTF-8 strings
- [x] Modules and imports: `use path/name`, cycles allowed
- [x] Standard library: strings, lists, maps, files, time, math (`std/`, M8)
- [x] Bytecode VM (host, M10): default engine, 1.2x to 2.1x faster on `bench/`. **Open:** VM written in Relic
- [ ] Native code generation explored (stretch)

## Phase 5: Desktop shell in Relic

- [ ] Windowing and drawing API for Relic programs (started: SDL2 `std/graphics` with drawing, keyboard/mouse input, text, BMP and PNG images, TrueType fonts; optional build, M11)
- [ ] Window manager, taskbar and Start menu rewritten in Relic (started: `shell/desktop.relic` with windows, a taskbar, a Start menu and two apps; no dragging or resizing yet)
- [ ] Virtual filesystem reachable from Relic

## Phase 6: Apps as proof

- [ ] Terminal
- [ ] Text editor
- [ ] File browser
- [ ] Drawing app
- [ ] At least one game
- [ ] Relic IDE
