# Vision

## The goal

Build a language, and then a whole desktop, from scratch. Relic is the base layer of MONOLITH 2. Over time every piece above the operating system should be written in Relic or by us.

## Principles

- **Domestic first.** Write our own pieces instead of pulling in libraries.
- **Bootstrap, then replace.** A C/C++ host is fine for phase 1. Each borrowed piece is tracked and removed once Relic can take its place.
- **Small and readable.** A new reader should be able to follow the language and its interpreter.
- **Proof by use.** Relic is done when the MONOLITH 2 shell and apps run on it.
- **Interpreter first.** Get a working language early. Native code generation comes later.

## Borrowed today, replaced later

| Piece | Phase 1 | Target |
|---|---|---|
| Interpreter host | C/C++ | Relic (self-hosted) |
| Standard library | Thin C/C++ wrappers | Relic |
| Desktop shell | MONOLITH 1 (C++) | Relic |

## Non-goals

- Our own CPU, kernel, GPU driver or network stack.
- Compatibility with any existing language.
- Speed before correctness. Performance work waits until the language is stable.
- Putting Relic or its IDE into the MONOLITH 1.0 repo.
