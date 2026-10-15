# Roadmap

Phases run in order. A phase is done when every box is ticked.

## Phase 1: Host bootstrap

- [ ] Pick the host language (**Default:** C++17, to match MONOLITH 1)
- [ ] Build setup and CI that runs on every push
- [ ] Test runner that runs `.relic` files and compares output
- [ ] Track every borrowed piece in a list so it can be replaced later

## Phase 2: Language core

- [ ] Lexer: turns source into tokens
- [ ] Parser: turns tokens into an AST
- [ ] Tree-walk interpreter over the AST
- [ ] REPL
- [ ] Numbers, strings, bools, variables
- [ ] `if`, `while`, functions, lists
- [ ] Clear error messages with file, line and column
- [ ] Meets the [1.0 checklist](1.0-CHECKLIST.md)

## Phase 3: Self-host

- [ ] Lexer rewritten in Relic
- [ ] Parser rewritten in Relic
- [ ] Interpreter or bytecode VM rewritten in Relic
- [ ] Relic builds and runs itself without the C/C++ host for language logic

## Phase 4: Runtime and standard library

- [ ] Memory model decided and built (see DESIGN.md)
- [ ] Modules and imports
- [ ] Standard library: strings, lists, maps, files, time, math
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
