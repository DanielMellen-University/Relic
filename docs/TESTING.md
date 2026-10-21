# Testing

How Relic is tested, and how to run it. A default build has 283 ctest cases (148, plus 135 `cgen_*` cases for compiled programs); an SDL build with SDL2_image, SDL2_ttf and DejaVu fonts adds 6 more.

## Running the tests

```
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

- Add `-j8` to `ctest` to run cases in parallel.
- With a multi-config generator (Visual Studio), pass `--config Release` to the build and `-C Release` to `ctest`.
- `ctest --test-dir build -R golden_std_json` runs one case.

## Golden tests

Each test is a set of files in `tests/golden/` with the same name:

| File | Holds |
|---|---|
| `name.relic` | the program |
| `name.out` | the exact stdout |
| `name.err` | for error tests: the exact stderr; the program must exit with an error |
| `name.args` | optional flags passed before the file, such as `--tokens` |

- Each `tests/golden/*.relic` file is its own ctest case (`golden_name`), run by `tools/run_golden.cmake` from inside `tests/golden/`. Helper modules live in `tests/golden/modules/` and are not run on their own.
- Paths in `.err` files are written relative to `tests/golden/`.
- Files named `repl_*.relic` are typed into `relic` on stdin. Their stdout must match `.out` and their stderr `.err` (each empty if the file is missing).
- Line endings are normalized to `\n` before comparing.
- The README sample is checked by `readme_sample`. Change both together.

Rules:

- One feature per test, named after the feature, like `while_break.relic`.
- Every bug fix adds a test that failed before the fix.
- Every syntax and runtime error message has an error test.

## Comparison tests

These check that different engines agree, byte for byte, on stdout and stderr:

| Test | Compares | Files |
|---|---|---|
| `selfhost_lexer` | `selfhost/tokens.relic` against `relic --tokens` | every golden, helper, `std/` and `selfhost/` file (168 today) |
| `selfhost_parser` | `selfhost/ast.relic` against `relic --ast` | same set |
| `selfhost_interp` | `selfhost/run.relic` against `relic` | goldens except `repl_*`, `rt_recursion`, `try_recursion`, `gc_cycles` and `c_demo` (132 today) |
| `vm_matches_tree_walker` | `relic` (VM) against `relic --tree` | goldens except `repl_*` (136 today) |

Two more self-host cases check the interpreter on real use:

| Test | Runs | Checks |
|---|---|---|
| `selfhost_args` | `selfhost/run.relic tests/selfhost/args.relic one --two` | the program gets its args |
| `selfhost_nested` | `selfhost/run.relic selfhost/run.relic tests/selfhost/args.relic deep` | the interpreter runs itself |

The skips are limits of the Relic-written interpreter, listed in [ARCHITECTURE.md](ARCHITECTURE.md#self-host).

## Unit tests

`tests/unit/main.cpp` (`relic_tests`, ctest case `unit`) checks the lexer, parser, interpreter and REPL from C++, where token lists or tree shape are easier to check than output. It uses a small hand-written harness, no test library.

## C runtime tests

Built with the C runtime (`runtime/c/`, on by default). Details in [C-RUNTIME.md](C-RUNTIME.md#tests).

| Test | Checks |
|---|---|
| `c_runtime` | unit tests for values, operators, messages, built-ins, files, the cycle collector, and that nothing leaks |
| `c_demo` | the hand-compiled `tests/demo.c` prints exactly `tests/golden/c_demo.out` |

`golden_c_demo` runs the same program on the C++ host, so a change to either side shows up. `c_demo` is skipped by `selfhost_interp` because it recurses 800 calls deep.

## Compiled golden tests

Every golden except `repl_*` and those with a `.args` file also runs compiled to C: `cgen_name` (131 cases). At build time `selfhost/cgen.relic` writes `build/cgen/name.c`, CMake builds it against `relic_rt` with the runtime's strict flags, and `tools/run_cgen.cmake` runs it in `tests/golden/` with `RELIC_LEAK_CHECK=1` (exit 3 if anything is left alive) and checks it the same way as `golden_name`. A program the generator does not cover yet would print `cgen-skip: <reason>` and ctest would report it as skipped. Today all 131 pass. `tests/cgen/*.relic` are cases for the compiled side only, with `.out` files from `relic --tree` (`cgen_only_name`). Details in [CGEN.md](CGEN.md#tests).

Three more cases compile the Relic-written tools and check them ([CGEN.md](CGEN.md#compiling-itself)):

| Test | Checks |
|---|---|
| `cgen_selfhost_lexer` | `selfhost/tokens.relic` compiled to C against `relic --tokens`, on the same files as `selfhost_lexer` |
| `cgen_selfhost_parser` | `selfhost/ast.relic` compiled to C against `relic --ast`, same files |
| `cgen_fixed_point` | cgen compiled by cgen writes the same C for `cgen.relic` as cgen on the host |

To run only these: `ctest --test-dir build -R "^cgen_"`. To check them under ASan, UBSan and LSan: configure with `-DRELIC_C_SANITIZE=ON` (gcc or clang).

## Graphics tests

The default build has no SDL, so only `golden_std_graphics` runs: it checks that `std/graphics` loads and that `available()` gives a bool.

An SDL build (`-DRELIC_SDL=ON`) adds headless cases. ctest sets `SDL_VIDEODRIVER=dummy` for them.

| Test | Checks | Needs |
|---|---|---|
| `gfx_smoke` | input, text and image calls run | SDL2 |
| `gfx_events` | the event queue: keys, text, clicks, wheel, resize; drawing; clipboard | SDL2 |
| `gfx_simulate` | `simulate` for keys with mods, mouse, move, quit | SDL2 |
| `mini_desktop` | the `examples/mini_desktop` example, driven by its own simulated input | SDL2 |
| `gfx_png` | loading a PNG | SDL2_image |
| `gfx_ttf` | TrueType text size | SDL2_ttf and a DejaVuSans.ttf |

On Debian or Ubuntu: `apt install libsdl2-dev libsdl2-image-dev libsdl2-ttf-dev fonts-dejavu-core`.

## CI

- `.github/workflows/ci.yml` builds Release and runs `ctest` on every push and pull request, on Ubuntu, macOS and Windows, without SDL.
- A second job, `c-runtime`, builds `runtime/c/` on its own with warnings as errors: gcc and clang on Linux with ASan, UBSan and LSan, clang on macOS, MSVC (`/std:c11 /W4`) on Windows.
- A third job, `cgen`, builds the whole project the same four ways and runs the `c_*` and `cgen_*` tests, so the generated C, the compiled lexer and parser and the fixed point are checked strict and sanitized on every OS.
- Keep main green. A red run gets fixed before new work lands.
- To save Actions minutes, run the tests locally first and push a few commits at once.

## Benchmarks

`tools/bench.sh build/relic` times each `bench/*.relic` on the tree-walker and the VM and checks both print the same. It skips `redraw.relic` without graphics. It is local only, not part of CI. Use a Release build. Numbers for the same programs compiled to C are in [CGEN.md](CGEN.md#speed).
