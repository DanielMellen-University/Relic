# Testing

How Relic is tested, and how to run it. The default build (C only, no host) has 145 ctest cases: 140 compiled goldens (`cgen_*`), `c_runtime`, `c_demo`, `bootstrap_fresh`, `bootstrap_fixed_point` and `example_compiled`. A host build (`-DRELIC_BUILD_HOST=ON`) has 305: the host's golden, unit, self-host and VM cases come back, plus the cross-checks. An SDL build with SDL2_image, SDL2_ttf and a TrueType font adds 8 more, and `-DRELIC_C_SDL=ON` adds 8 more compiled ones (`c_gfx_*`, `c_mini_desktop`).

## Running the tests

```
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

That is the C-only build. Add `-DRELIC_BUILD_HOST=ON` to the first line for the host and all of its tests. Every section below that runs `relic` needs it: the golden, comparison, unit and graphics tests. The C runtime, compiled golden and bootstrap tests run in both.

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
| `selfhost_lexer` | `selfhost/tokens.relic` against `relic --tokens` | every golden, helper, `std/` and `selfhost/` file (178 today) |
| `selfhost_parser` | `selfhost/ast.relic` against `relic --ast` | same set |
| `selfhost_interp` | `selfhost/run.relic` against `relic` | goldens except `repl_*`, `rt_recursion`, `try_recursion` and `c_demo` (142 today) |
| `vm_matches_tree_walker` | `relic` (VM) against `relic --tree` | goldens except `repl_*` (145 today) |

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
| `c_runtime` | unit tests for values, operators, messages, built-ins, files, the cycle collector, the high-DPI rounding (`src/builtins/gfx_scale.h`), and that nothing leaks |
| `c_demo` | the hand-compiled `tests/demo.c` prints exactly `tests/golden/c_demo.out` |

`golden_c_demo` runs the same program on the C++ host, so a change to either side shows up. `c_demo` is skipped by `selfhost_interp` because it recurses 800 calls deep.

## Compiled golden tests

Every golden except `repl_*` and those with a `.args` file also runs compiled to C: `cgen_name` (140 cases). At build time `build/relicc` writes `build/cgen/name.c` (in a host build, `selfhost/cgen.relic` on the host does), CMake builds it against `relic_rt` with the runtime's strict flags, and `tools/run_cgen.cmake` runs it in `tests/golden/` with `RELIC_LEAK_CHECK=1` (exit 3 if anything is left alive) and checks it the same way as `golden_name`. A program the generator does not cover yet would print `cgen-skip: <reason>` and ctest would report it as skipped. Today all 140 pass. `tests/cgen/*.relic`, if any, are cases for the compiled side only, with `.out` files from `relic --tree` (`cgen_only_name`); there are none now. Details in [CGEN.md](CGEN.md#tests).

In a host build, three more cases compile the Relic-written tools and check them ([CGEN.md](CGEN.md#compiling-itself)):

| Test | Checks |
|---|---|
| `cgen_selfhost_lexer` | `selfhost/tokens.relic` compiled to C against `relic --tokens`, on the same files as `selfhost_lexer` |
| `cgen_selfhost_parser` | `selfhost/ast.relic` compiled to C against `relic --ast`, same files |
| `cgen_fixed_point` | cgen compiled by cgen writes the same C for `cgen.relic` as cgen on the host |
| `cgen_stage1_matches_host` | `relicc` writes the same C as the host for every compiled golden |

## Bootstrap tests

`bootstrap/cgen.c` is the committed C for cgen ([bootstrap/README.md](../bootstrap/README.md), [CGEN.md](CGEN.md#bootstrap)).

| Test | Build | Checks |
|---|---|---|
| `bootstrap_fresh` | all | the committed C is what cgen writes from the current sources; if it fails, run `cmake -DBUILD=build -P tools/regen_bootstrap.cmake` and commit `bootstrap/cgen.c` |
| `bootstrap_fixed_point` | all | `relicc` compiling `cgen.relic` writes the C it was built from |
| `bootstrap_matches_host` | host | the host running `cgen.relic` writes the committed C |

To run only these: `ctest --test-dir build -R "^cgen_"`. To check them under ASan, UBSan and LSan: configure with `-DRELIC_C_SANITIZE=ON` (gcc or clang).

## Graphics tests

The default build has no SDL, so only `golden_std_graphics` runs: it checks that `std/graphics` loads and that `available()` gives a bool.

An SDL build (`-DRELIC_SDL=ON`) adds headless cases. ctest sets `SDL_VIDEODRIVER=dummy` for them.

| Test | Checks | Needs |
|---|---|---|
| `gfx_smoke` | input, text and image calls run | SDL2 |
| `gfx_events` | the event queue: keys, text, clicks, wheel, resize; drawing; clipboard | SDL2 |
| `gfx_simulate` | `simulate` for keys with mods, mouse, move, quit | SDL2 |
| `gfx_window` | title, window size, scale (1.0 headless, also while a canvas is the target), fullscreen, cursor, vsync, `ticks`, `frame_time`, `present_capped`. What the dummy driver cannot do (real cursors, vsync) is checked by type | SDL2 |
| `gfx_window_errors` | every error those calls give, in order | SDL2 |
| `mini_desktop` | the `examples/mini_desktop` example, driven by its own simulated input | SDL2 |
| `gfx_png` | loading a PNG | SDL2_image |
| `gfx_ttf` | TrueType text size, in whole window units | SDL2_ttf and a font: DejaVuSans.ttf, else Arial (macOS, Windows), or `-DRELIC_TEST_FONT=path` |

`-DRELIC_C_SDL=ON` (no host needed) builds `relic_rt_gfx`, the C runtime with the SDL2 module, and runs the same programs compiled: `c_gfx_smoke`, `c_gfx_events`, `c_gfx_simulate`, `c_gfx_window`, `c_gfx_window_errors`, `c_mini_desktop`, `c_gfx_png` (with SDL2_image) and `c_gfx_ttf` (with SDL2_ttf and the font). Each is built with `relic_add_program(... GRAPHICS)`, runs with the leak check on (`RELIC_LEAK_CHECK=1`) and must print what the host test prints. Everything else, `relicc` and the `cgen_*` goldens included, still links `relic_rt` with the stub, so `c_runtime` and `cgen_std_graphics` are the same in every build.

`mini_desktop` and `c_mini_desktop` both write `examples/mini_desktop/data`, so they share a ctest lock and never run at the same time.

`simulate` puts its event straight into Relic's own queue, after whatever SDL already holds, instead of going through `SDL_PushEvent`. Homebrew's `sdl2` is sdl2-compat (SDL2 over SDL3), which crashes on a pushed text event and loses a pushed wheel's distance.

Getting SDL2:

- Debian or Ubuntu: `apt install libsdl2-dev libsdl2-image-dev libsdl2-ttf-dev fonts-dejavu-core`.
- macOS: `brew install sdl2 sdl2_image sdl2_ttf`, then configure with `-DCMAKE_PREFIX_PATH=$(brew --prefix)`.
- Windows (MSVC): `vcpkg install sdl2 sdl2-image sdl2-ttf --triplet x64-windows`, then configure with `-DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake`. The toolchain copies the SDL2 DLLs next to each program.

The CI job `sdl` runs a build with `-DRELIC_SDL=ON -DRELIC_C_SDL=ON` on Ubuntu 24.04 (apt), macOS (Homebrew) and Windows (vcpkg, MSVC): 321 cases each, headless with `SDL_VIDEODRIVER=dummy`. It fails if any of the 16 graphics tests is missing.

## relic_add_program

`example_compiled` builds `examples/compiled/main.relic` (a module in `lib/` and `std/strings`) with `relic_add_program` and checks its output and args. `tests/consumer/` is a separate CMake project that adds the Relic tree with `add_subdirectory` and builds `hello.relic` the same way:

```
cmake -S tests/consumer -B build-consumer
cmake --build build-consumer
ctest --test-dir build-consumer --output-on-failure
```

## CI

`.github/workflows/ci.yml` runs on every push and pull request, 18 jobs:

- `build`: the default C-only build, Release, all tests, on Ubuntu (gcc), macOS (clang) and Windows (MSVC), then the `tests/consumer` project.
- `no-cxx`: the same on Alpine Linux with only gcc installed. It first checks that `c++`, `g++` and `clang++` are all missing, so it proves a fresh checkout needs no C++ compiler. It builds `tests/consumer` too.
- `host`: `-DRELIC_BUILD_HOST=ON` on all three OSes, all tests, including the cross-checks against the host.
- `c-runtime`: `runtime/c/` on its own with warnings as errors: gcc and clang on Linux with ASan, UBSan and LSan, clang on macOS, MSVC (`/std:c11 /W4`) on Windows.
- `cgen`: the C-only build the same four ways, so `bootstrap/cgen.c`, `relicc` and every compiled golden are built with warnings as errors and run sanitized.
- `sdl`: the host and compiled graphics tests, headless, on Ubuntu 24.04, macOS and Windows (see [Graphics tests](#graphics-tests)).
- Keep main green. A red run gets fixed before new work lands.
- To save Actions minutes, run the tests locally first and push a few commits at once.

## Benchmarks

`tools/bench.sh build/relic` times each `bench/*.relic` on the tree-walker and the VM and checks both print the same. It skips `redraw.relic` without graphics. It is local only, not part of CI. Use a Release build. Numbers for the same programs compiled to C are in [CGEN.md](CGEN.md#speed).
