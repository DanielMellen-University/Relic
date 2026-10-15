# Testing

## Golden tests

Each test is a pair of files in `tests/golden/`:

- `name.relic`: the program
- `name.out`: the exact expected output

The runner executes `relic name.relic`, captures stdout, and compares it byte for byte with `name.out`. Any difference fails the test and prints a diff.

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
- A red CI blocks merges to main.

## Self-host check (phase 3)

The same golden folder runs against both the C++ host and the Relic-written version. Both must pass before the host stage is retired.
