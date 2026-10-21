# Vision

## The goal

Build a language, and then a whole desktop, from scratch. Relic is the base layer of MONOLITH 2. Over time every piece above the operating system should be written in Relic or by us.

## Principles

- **Domestic first.** Write our own pieces instead of pulling in libraries.
- **Bootstrap, then replace.** A C/C++ host is fine for phase 1. Each borrowed piece is tracked and removed once Relic can take its place.
- **Small and readable.** A new reader should be able to follow the language and its interpreter.
- **Proof by use.** Relic is done when the MONOLITH 2 shell and apps run on it. They live in [M0N0LITH-2](https://github.com/DanielMellen-University/M0N0LITH-2).
- **Interpreter first.** Get a working language early. Native code generation comes later.

## Borrowed today, replaced later

| Piece | Today | Target |
|---|---|---|
| Interpreter host | C++17 host runs everything; lexer, parser and interpreter also exist in Relic (`selfhost/`) | Relic runs itself |
| Standard library | Relic code in `std/` on top of a few `host_*` built-ins (files, clocks, float math) | Relic all the way down |
| Graphics | SDL2, optional build | Relic drawing layer |
| Desktop shell | MONOLITH 1 (C++) | Relic, in M0N0LITH-2 |

## Non-goals

- Our own CPU, kernel, GPU driver or network stack.
- Compatibility with any existing language.
- Speed before correctness. The bytecode VM had to match the tree-walker on every test before it became the default.
- Putting Relic or its IDE into the MONOLITH 1.0 repo.
