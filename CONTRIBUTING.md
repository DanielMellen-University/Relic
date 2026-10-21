# Contributing to Relic

How work gets into this repo. Relic is the language only. Desktop and app work goes to [M0N0LITH-2](https://github.com/DanielMellen-University/M0N0LITH-2).

## Before you start

- New language features, std modules, built-ins and roadmap items need Daniel's approval first. Bug fixes, tests, docs, and finishing items already on the [roadmap](docs/ROADMAP.md) do not.
- Items marked **Default** in the docs can change. Items marked **Open** need a decision (see [DESIGN.md](docs/DESIGN.md#open-questions)).
- Read [ARCHITECTURE.md](docs/ARCHITECTURE.md) for where things live.

## Build and test

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

For graphics work, also build with `-DRELIC_SDL=ON` into `build-sdl` and run its tests. Details: [TESTING.md](docs/TESTING.md).

## Rules for a change

- Every bug fix comes with a golden test that failed before the fix.
- Both engines must agree. `vm_matches_tree_walker` compares them on every golden program.
- The self-hosted side must keep up. A change to the lexer, parser or interpreter rules goes into `selfhost/` too, or `selfhost_lexer`, `selfhost_parser` and `selfhost_interp` fail.
- A new error message gets an error test (`.err`).
- Changing the README sample means changing `tests/golden/readme_sample.relic` to match.
- Docs change in the same commit as the behaviour they describe: [LANGUAGE.md](docs/LANGUAGE.md) for rules, [STDLIB.md](docs/STDLIB.md) for std, [CLI.md](docs/CLI.md) for the command line.

## Commits and versions

- Commit straight to `main` with normal commits. No force-push.
- One logical change per commit, with a message that says what changed.
- Run the tests before pushing, and check that CI is green on all three OSes after.
- The version lives in `project(relic VERSION ...)` in `CMakeLists.txt`. When it changes, also change the REPL banner shown in [CLI.md](docs/CLI.md#the-repl) and the README, and add a [CHANGELOG](CHANGELOG.md) entry.

## Writing docs

- Plain, short sentences. Tables and lists over long paragraphs.
- Run every example you put in the docs and paste the real output.
- No em dashes.
