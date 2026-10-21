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
| C++17 compiler and stdlib (optional since C5) | the host: tree-walker, VM, REPL, graphics; a cross-check for the compiled side | replaced in the default build by the Relic compiler in `bootstrap/` (C5); the host builds only with `-DRELIC_BUILD_HOST=ON` | done for the default build (C5); the REPL and graphics still need the host |
| `host_*` built-ins | files, clocks, float math, process access under `std/` ([HOST.md](HOST.md)) | plain C primitives in the C runtime | written in C1 (`runtime/c/rl_host.c`); the optional host keeps its C++ ones |
| `std::shared_ptr` reference counts | memory | the C runtime's own counts and collector | written in C1 (`runtime/c/rl_core.c`); the optional host keeps `shared_ptr` |
| C99 compiler, C library and libm, POSIX or Win32 file calls | the C runtime and, from C2, generated C | stays: this is the floor of the bootstrap Daniel picked | |
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
