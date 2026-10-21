# Testing

How Relic is tested, and how to run it. A default build has 143 ctest cases; an SDL build with SDL2_image, SDL2_ttf and DejaVu fonts has 149.

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
| `selfhost_lexer` | `selfhost/tokens.relic` against `relic --tokens` | every golden, helper, `std/` and `selfhost/` file (161 today) |
| `selfhost_parser` | `selfhost/ast.relic` against `relic --ast` | same set |
| `selfhost_interp` | `selfhost/run.relic` against `relic` | goldens except `repl_*`, `rt_recursion`, `try_recursion` and `gc_cycles` (130 today) |
| `vm_matches_tree_walker` | `relic` (VM) against `relic --tree` | goldens except `repl_*` (133 today) |

Two more self-host cases check the interpreter on real use:

| Test | Runs | Checks |
|---|---|---|
| `selfhost_args` | `selfhost/run.relic tests/selfhost/args.relic one --two` | the program gets its args |
| `selfhost_nested` | `selfhost/run.relic selfhost/run.relic tests/selfhost/args.relic deep` | the interpreter runs itself |

The skips are limits of the Relic-written interpreter, listed in [ARCHITECTURE.md](ARCHITECTURE.md#self-host).

## Unit tests

`tests/unit/main.cpp` (`relic_tests`, ctest case `unit`) checks the lexer, parser, interpreter and REPL from C++, where token lists or tree shape are easier to check than output. It uses a small hand-written harness, no test library.

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
- Keep main green. A red run gets fixed before new work lands.
- To save Actions minutes, run the tests locally first and push a few commits at once.

## Benchmarks

`tools/bench.sh build/relic` times each `bench/*.relic` on the tree-walker and the VM and checks both print the same. It skips `redraw.relic` without graphics. It is local only, not part of CI. Use a Release build.
