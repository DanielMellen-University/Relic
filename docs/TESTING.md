# Testing

## Golden tests

Each test is a pair of files in `tests/golden/`:

- `name.relic`: the program
- `name.out`: the exact expected output

The runner executes `relic name.relic` (the VM), captures stdout, and compares it byte for byte with `name.out`. Any difference fails the test and prints a diff.

### Error tests

- `name.relic` plus `name.err`: expected error text, including line and column.
- The test passes when the program exits with an error and stderr matches `name.err`.

### Rules

- One feature per test. Name tests after the feature, like `while_break.relic`.
- Every bug fix adds a test that failed before the fix.
- Line endings are normalized to `\n` before comparing.

## Running tests

```
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Each `tests/golden/*.relic` file becomes its own ctest case, run by `tools/run_golden.cmake`. In `.err` files, paths are written relative to `tests/golden/`. An optional `name.args` file holds flags passed before the file, such as `--tokens`. Files named `repl_*.relic` are typed into `relic` on stdin instead; stdout must match `.out` and stderr must match `.err` (each empty if the file is missing).

## Unit tests

C++ tests in `tests/unit/` for the lexer and parser, where checking tokens or tree shape is easier than checking output. **Default:** a small hand-written test harness, no test library.

## CI

- Build, then run unit tests, then golden tests, on every push.
- CI runs on Ubuntu, macOS and Windows, without SDL.
- A red CI blocks merges to main.
- To save Actions minutes, batch work into few pushes and run the tests locally first.

## Comparison tests

Extra ctest cases check that different engines agree, byte for byte, on stdout and stderr:

| Test | Compares | Files |
|---|---|---|
| `selfhost_lexer` | `selfhost/tokens.relic` against `relic --tokens` | every golden, helper, `std/` and `selfhost/` file |
| `selfhost_parser` | `selfhost/ast.relic` against `relic --ast` | same set |
| `selfhost_interp` | `selfhost/run.relic` against `relic` | goldens except `repl_*`, modules (`use_*`, `std_*`, `err_use_*`) and `rt_recursion` |
| `vm_matches_tree_walker` | `relic` (VM) against `relic --tree` | goldens except `repl_*` |

The skips are limits of the Relic-written interpreter, listed in DESIGN.md (Self-host).

## Benchmarks

`tools/bench.sh build/relic` times each `bench/*.relic` on the tree-walker and the VM and checks both print the same. It is local only, not part of CI. Use a Release build.

## Graphics

The default build has no SDL, so tests only check that `std/graphics` loads (`std_graphics`). With `-DRELIC_SDL=ON`, try `examples/hello_window.relic`; on a machine with no display, `SDL_VIDEODRIVER=dummy` runs it headless. SDL builds also get a `gfx_smoke` ctest (`tests/sdl/`) that runs input, text and image calls headless.
