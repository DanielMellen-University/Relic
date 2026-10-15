# Roadmap

Phases run in order, with one exception: the memory model and modules from phase 4 come before self-hosting (see [PROJECT_PLAN.md](PROJECT_PLAN.md)). A phase is done when every box is ticked.

## Phase 1: Host bootstrap

- [x] Pick the host language (**Default:** C++17, to match MONOLITH 1)
- [x] Build setup (**Default:** CMake) and CI that runs on every push
- [ ] `relic` prints `hello` from a golden test (see [BOOTSTRAP.md](BOOTSTRAP.md))
- [x] Test runner that runs `.relic` files and compares output
- [x] Track every borrowed piece in a list so it can be replaced later

## Phase 2: Language core

- [x] Lexer: turns source into tokens
- [x] Parser: turns tokens into an AST
- [x] Tree-walk interpreter over the AST
- [ ] REPL
- [x] Numbers, strings, bools, variables
- [x] Indentation blocks, `if`, `while`, `for`, `fun` functions, lists, maps
- [x] Clear error messages with file, line and column
- [ ] Meets the [1.0 checklist](1.0-CHECKLIST.md)

## Phase 3: Self-host

- [ ] Lexer rewritten in Relic
- [ ] Parser rewritten in Relic
- [ ] Interpreter or bytecode VM rewritten in Relic
- [ ] Relic builds and runs itself without the C/C++ host for language logic

## Phase 4: Runtime and standard library

- [ ] Memory model decided and built (see DESIGN.md)
- [ ] Modules and imports
- [ ] Standard library: strings, lists, maps (methods beyond literals), files, time, math
- [ ] Bytecode VM if the tree-walker is too slow
- [ ] Native code generation explored (stretch)

## Phase 5: Desktop shell in Relic

- [ ] Windowing and drawing API for Relic programs
- [ ] Window manager, taskbar and Start menu rewritten in Relic
- [ ] Virtual filesystem reachable from Relic

## Phase 6: Apps as proof

- [ ] Terminal
- [ ] Text editor
- [ ] File browser
- [ ] Drawing app
- [ ] At least one game
- [ ] Relic IDE
