# Bootstrap: empty repo to `print("hello")`

Steps for the phase 1 host. This is a plan, not code.

## Tools

- C++17 compiler (GCC, Clang or MSVC)
- CMake 3.16 or newer
- No other libraries. **Default:** the C++ standard library is the only borrowed dependency.

## Status

All 7 steps are done (M1 to M4). `relic file.relic` runs programs, `tests/golden/hello.relic` prints `hello`, `relic --tokens` prints tokens and `relic --ast` prints the tree.

## Steps

1. **Layout.** Create the folders in [ARCHITECTURE.md](ARCHITECTURE.md) and a `CMakeLists.txt` with the three targets.
2. **Entry point.** `relic --version` prints `relic 0.0.1`. Add CI that builds on push.
3. **Golden runner.** Add the runner from [TESTING.md](TESTING.md) and one test.
4. **Tokens.** Lex names, string literals, `(`, `)`, newlines and end of file. INDENT/DEDENT can wait until blocks arrive. Add `--tokens`.
5. **Parse a call.** Parse `name(expr)` where expr is a string. Add `--ast`.
6. **Run it.** Interpreter looks up `print` in builtins and writes the string plus a newline.
7. **First test.** `tests/golden/hello.relic` holds `print("hello")`, `hello.out` holds `hello`. CI goes green.

After step 7, grow each stage toward the [1.0 checklist](1.0-CHECKLIST.md).

## Borrowed pieces list

Track everything Relic relies on that we did not write. Each row needs a planned replacement phase.

| Piece | Used for | Replace in |
|---|---|---|
| C++ compiler and stdlib | host interpreter | phase 3 (self-host) |
| CMake | build | **Open** (Relic build tool later) |
| CMake script mode | golden test runner (`tools/run_golden.cmake`) | phase 3 (runner in Relic) |
| GitHub Actions | CI on Linux, macOS, Windows | not planned |
