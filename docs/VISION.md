# Vision

## The goal

Build a language, and then a whole desktop, from scratch. Relic is the base layer of MONOLITH 2. Over time every piece above the operating system should be written in Relic or by us.

## Principles

- **Domestic first.** Write our own pieces instead of pulling in libraries.
- **Bootstrap, then replace.** A C/C++ host is fine for phase 1. Each borrowed piece is tracked and removed once Relic can take its place.
- **Small and readable.** A new reader should be able to follow the language and its interpreter.
- **Proof by use.** Relic is done when the MONOLITH 2 shell and apps run on it. They live in [M0N0LITH-2](https://github.com/DanielMellen-University/M0N0LITH-2).
- **Interpreter first.** Get a working language early. The end state is a compiler to C, written in Relic.

## Borrowed today

Everything Relic relies on that we did not write. Each piece has a planned replacement. Keep this list current when something new is borrowed.

| Piece | Used for | Target | Replace in |
|---|---|---|---|
| C++17 compiler and stdlib | the host: lexer, parser, tree-walker, VM | a Relic compiler written in Relic that emits C; the C++ host stays as stage 0 until then | phase 3 (C5) |
| `host_*` built-ins | files, clocks, float math, process access under `std/` ([HOST.md](HOST.md)) | plain C primitives in the C runtime | phase 3 (C1) |
| `std::shared_ptr` reference counts | memory | the C runtime | phase 3 (C1) |
| SDL2, SDL2_image, SDL2_ttf (optional) | graphics, `-DRELIC_SDL=ON` | a Relic drawing layer | phase 5 |
| CMake | build | **Open:** a Relic build tool | |
| CMake script mode | golden runner and comparison scripts in `tools/` | **Open:** a runner in Relic | |
| GitHub Actions | CI on Linux, macOS, Windows | not planned | |

The desktop shell (MONOLITH 1 in C++ today) is being rewritten in Relic in M0N0LITH-2.

## Non-goals

- Our own CPU, kernel, GPU driver or network stack.
- Compatibility with any existing language.
- Speed before correctness. The bytecode VM had to match the tree-walker on every test before it became the default.
- Putting Relic or the Relic IDE into the MONOLITH 1.0 repo. The IDE is a planned app in M0N0LITH-2.
