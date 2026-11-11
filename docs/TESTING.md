# Testing

How Relic is tested, and how to run it. The default build (C only, no host) has 454 ctest cases: 410 compiled goldens (`cgen_*`), the compiled-only `cgen_only_json_deep` and `cgen_only_long_string`, the 30 compiled example programs `cgen_only_learn_*` (`examples/learn/` and `examples/word_count.relic`, see [EXAMPLES.md](EXAMPLES.md)), `cgen_remove_tree_link` (since 1.0.52: `files.remove_tree` in a folder with links in it removes the links, not what they point at; skipped where links cannot be made), `c_runtime`, `c_demo`, `bootstrap_fresh`, `bootstrap_fixed_point`, `example_compiled`, `tablecheck_goldens`, `depth_limit` and `doc_counts`. A host build (`-DRELIC_BUILD_HOST=ON`) has 902: the host's golden, unit, self-host and VM cases come back, plus the cross-checks, `remove_tree_link` on the host and `guide_examples` (since 1.0.52: every example in [GUIDE.md](GUIDE.md) is run and must print the output block shown after it; `tools/doc_blocks.relic` splits the guide and `tools/check_guide.cmake` runs the blocks) and `stdlib_examples` (since 1.0.52: each `call(args)` → `value` example in a std section of [STDLIB.md](STDLIB.md) must give that value; `tools/check_stdlib_examples.cmake`. It also checks the same kind of line in [docs/stdlib](stdlib/README.md) and runs every program in `examples/learn/` and `examples/ref/` against its `.out`, through `tools/check_example_programs.cmake`). An SDL build with SDL2_image, SDL2_ttf and a TrueType font adds 8 more, and `-DRELIC_C_SDL=ON` adds 8 more compiled ones (`c_gfx_*`, `c_mini_desktop`).

## Running the tests

```
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

That is the C-only build. Add `-DRELIC_BUILD_HOST=ON` to the first line for the host and all of its tests. Every section below that runs `relic` needs it: the golden, comparison, unit and graphics tests. The C runtime, compiled golden and bootstrap tests run in both.

- Add `-j8` to `ctest` to run cases in parallel. The cases that write files in `tests/golden` (`std_files` and `std_folders`, compiled and not, and `selfhost_interp` and `vm_matches_tree_walker`, which run every golden) take turns through a `RESOURCE_LOCK`, so a parallel run cannot remove one's folder under another.
- With a multi-config generator (Visual Studio), pass `--config Release` to the build and `-C Release` to `ctest`.
- `ctest --test-dir build -R golden_std_json` runs one case.

## Golden tests

Each test is a set of files in `tests/golden/` with the same name:

| File | Holds |
|---|---|
| `name.relic` | the program |
| `name.out` | the exact stdout |
| `name.err` | for error tests: the exact stderr; the program must exit with code 1 |
| `name.args` | optional flags passed before the file, such as `--tokens` |
| `name.in` | optional standard input for the program (since 1.0.31); without it the program gets empty input, never the terminal |

- Each `tests/golden/*.relic` file is its own ctest case (`golden_name`), run by `tools/run_golden.cmake` from inside `tests/golden/`. Helper modules live in `tests/golden/modules/` and are not run on their own.
- `err_string_tail`, `err_string_tail_nested` and `err_string_later_interp` check that unterminated interpolated strings blame the opening quote.
- `map_value_types` checks maps holding every ordinary value kind; building it also exercises the host value header on GCC 11.
- Paths in `.err` files are written relative to `tests/golden/`.
- Files named `repl_*.relic` are typed into `relic` on stdin. Their stdout must match `.out` and their stderr `.err` (each empty if the file is missing).
- A program that reads standard input (`os.read_line()`) gets `name.in` on stdin, in `golden_name`, `cgen_name` and in the VM and `selfhost_interp` comparisons. `.gitattributes` marks `*.in` as binary (`-text`), so CRLF and odd bytes in them are kept as written, on Windows too.
- Line endings are normalized to `\n` before comparing.
- The README sample is checked by `readme_sample`. Change both together.

Rules:

- One feature per test, named after the feature, like `while_break.relic`.
- Every bug fix adds a test that failed before the fix.
- Every syntax and runtime error message has an error test.
- Adding a golden changes the program counts quoted in this file and in `CGEN.md`; the `doc_counts` test fails until they are updated.

## Comparison tests

These check that different engines agree, byte for byte, on stdout and stderr:

| Test | Compares | Files |
|---|---|---|
| `selfhost_lexer` | `selfhost/tokens.relic` against `relic --tokens` | every golden, helper, `std/`, `selfhost/` and `tests/check_cases/` file (468 today) |
| `selfhost_parser` | `selfhost/ast.relic` against `relic --ast` | same set |
| `selfhost_interp` | `selfhost/run.relic` against `relic` | goldens except `repl_*` (416 today) |
| `vm_matches_tree_walker` | `relic` (VM) against `relic --tree` | goldens except `repl_*` (416 today) |

Three more self-host cases check the interpreter on real use:

| Test | Runs | Checks |
|---|---|---|
| `selfhost_args` | `selfhost/run.relic tests/selfhost/args.relic one --two` | the program gets its args |
| `selfhost_nested` | `selfhost/run.relic selfhost/run.relic tests/selfhost/args.relic deep` | the interpreter runs itself |
| `selfhost_hooks` | `tests/selfhost/hooks.relic` | `rt.out`, `rt.steps`, `rt.denied`, `rt.load`, and `rt.input` with `interp.resume`, the hooks an IDE uses to run a program inside another; prints `hooks ok` |
| `selfhost_check` | `selfhost/check.relic FILE` against `relic check FILE` (`tools/compare_check.cmake`) | stdout, stderr and exit code on the 15 cases in `tests/check_cases` (scopes, late names, use before definition, did-you-mean, outside a loop, too-large literals, syntax errors) and every `.relic` file in the tree, plus runs with several files and missing ones |
| `selfhost_check_nested` | the same on `selfhost/run.relic` (the interpreter written in Relic), for two cases | the interpreter runs `check.relic` right |
| `cgen_selfhost_check` | the compiled `selfhost/check.relic` (host build with C) | the same comparison as `selfhost_check` |
| `selfhost_repl` | `selfhost/repl.relic` against the host's REPL (`tools/compare_repl.cmake`) | stdout, stderr and exit code on every REPL script: the four `tests/golden/repl_*.relic` and the 41 cases in `tests/repl_cases/*.in` (values, state, errors, blocks, multi-line headers, `:quit`, input that ends inside a block, CRLF, long lines, a program reading the same stdin, `use`, `os.exit`) |
| `selfhost_repl_nested` | the same on `selfhost/run.relic` (the interpreter written in Relic), for two scripts | the interpreter runs `repl.relic` right |
| `cgen_selfhost_repl` | the compiled `selfhost/repl.relic` (host build with C) | the same comparison as `selfhost_repl` |
| `relicc_cli` | `relicc` with no arguments, no `-o`, a clean file, a syntax error, a missing file and a folder (`tools/check_relicc_cli.cmake`) | exit codes 0, 1 and 3, and `relicc: cannot open FILE` |
| `depth_limit` | 10,000 and 10,001 levels of `a + a + ...`, `m.a.a...` and `else if`, written by `tools/check_depth_limit.cmake` | 10,000 runs on the VM and `--tree`; 10,001 is the same error at the same place from the VM, `--tree`, `relic check`, `run.relic`, `check.relic` and `relicc` (the default build checks `relicc` only) |
| `tree_stack` | `f(n) = f(n - 1) + 1 + 1 ...` (9,990 terms) called 700 deep, written by `tools/check_tree_stack.cmake` | the VM prints the sum; `--tree`, which recurses on the native stack, stops with `too much recursion (calls and expressions nest too deeply for the stack)` instead of crashing, and `try` catches it (host build only) |
| `selfhost_cli` | `selfhost/run.relic` with no file, a missing file, a folder, an error and a clean run (`tools/check_selfhost_cli.cmake`) | the exit codes (1 for an uncaught error, 2 for usage or a file that cannot be opened) and the "cannot open" message are the host's |

`selfhost_interp` and `vm_matches_tree_walker` compare exit codes as well as output. The `cli` case (`tools/check_cli.cmake`) runs the command line from [CLI.md](CLI.md): exit codes, usage errors, args passed through, `relic check` and the REPL on piped input.

The skips are limits of the Relic-written interpreter, listed in [ARCHITECTURE.md](ARCHITECTURE.md#self-host).

`tools/fuzz_check.py SEED` writes a random program (nested functions, blocks, loops, `try`, `use`, a pool of names with typos). It is not in ctest because it needs Python; after changing `selfhost/check.relic`, compare the two `check`s on a thousand seeds by hand (the command is in the file). Seeds 1 to 1500 agreed in 1.0.27, 2000 to 2400 in 1.0.32, 4000 to 4200 in 1.0.40 (after the 50-hint limit), and 4300 to 4400 in 1.0.41.

`tools/fuzz_repl.py SEED [COUNT]` feeds random scripts (lines picked from the REPL cases and the goldens, LF or CRLF) to the host's REPL and to `selfhost/repl.relic` and reports any difference in stdout, stderr or exit code. Also by hand; about 5000 scripts agreed in 1.0.31.

`tools/fuzz_mutate.py MODE SEED COUNT` edits the golden and check-case programs a little (a quote, a bracket, a keyword, a multi-byte or control character) and compares the Relic-written tools with the host: `tokens`, `ast` and `check` against `--tokens`, `--ast` and `relic check`, and `run` for the VM against the tree-walker, `selfhost/run.relic` and the compiled program. It prints the path of each mismatch. Not in ctest (it needs Python); run it after changing the lexer, parser, `check`, `errfmt` or the interpreter in `selfhost/`.

`tools/fuzz_gen.py SEED COUNT` writes programs that run instead of editing old ones (expressions over ints, floats including tiny and huge literals, text for `num()`, `float()` and `int()` (signs, `nan` / `inf`, hex at the int range's edges, tiny and huge floats, near misses), strings, lists and maps, the built-ins, `std/strings`, `std/lists`, `std/maps`, `std/math`, `std/json`, `std/paths`, interpolation, functions, closures, `else if` chains, loops, `try`, and `os.read_line` / `os.prompt` on a random standard input), runs each on the VM, the tree-walker, `selfhost/run.relic` and compiled, and prints the path of any program (and its input) where stdout, stderr or the exit code differ. By hand as well. About 2300 programs agreed in 1.0.32 and 3600 more since; in 1.0.34 it found `slice(s, -inf, 3)` failing on the Relic-written interpreter. Since 1.0.49 it also calls the std functions that release adds (`count`, `find_last`, `lines`, `trim_start`, `trim_end`, `contains`, `unique`, `min`, `max`, `any`, `all`, `flatten`, `zip`, `from_items`, `sign`); seeds 101 to 108 (400 programs) agreed in 1.0.49, and with the 1.0.50 calls (`reverse`, `is_digits`, `count`, `chunk`, `gcd`) seeds 111 to 114 (200 programs) agreed, and with `lcm`, `invert` and `parts` too, seeds 121 to 124 (200 more), and with `find_index`, `product`, `take`, `drop`, `partition`, `words`, `center`, `remove_prefix`, `remove_suffix`, `is_alpha`, `pick`, `is_even`, `is_odd`, `lerp`, `is_abs` and `with_ext`, seeds 131 to 134 (200 more; one skipped for running past the time limit, which the script allows), and with `last`, `mean`, `windows`, `hypot`, `radians`, `degrees`, `relative` and `capitalize`, seeds 141 to 144 (200 more), and with `truncate`, `is_upper`, `is_lower`, `rotate`, `sum_by` and `trunc`, seeds 151 to 154 (200 more; one skipped for time), and with `count_by`, `has_value` and `dist`, after the std errors started saying what they got, seeds 161 to 164 (200 more). After the 1.0.52 std fixes (`hypot`, `gcd`, `unique` on numbers, the type checks), seeds 171, 172, 181 and 182 (180 more) agreed; one skipped for time. Seed 191 (25) agreed after `sum`, `product`, `mean` and `with_ext` started checking their arguments.

`tools/fuzz_depth.py SEED COUNT` builds expressions and nested `else if` chains within three levels of the 10,000 limit (links, `-`, `not`, lists, maps, `{...}` in strings, call arguments, indexes) and checks that `relic check`, `selfhost/check.relic` and relicc agree and fail exactly past 10,000. By hand too; seeds 1 to 4 (132 cases) agreed in 1.0.37, seed 5 (40 more) in 1.0.38, seed 6 (40 more) in 1.0.39, seed 8 (40 more) in 1.0.40, and seed 9 (40 more) in 1.0.41.

`tools/fuzz_maps.py SEED COUNT [BUILD_DIR [OLD_BUILD_DIR]]` writes programs of random map operations (set, remove, adding a removed key back, removing in a `for` loop, `keys`, `len`, `==`, printing, missing keys under `try`) and compares the VM with `--tree`, `selfhost/run.relic` and the compiled program, and with an older build's VM, `--tree` and compiled program when OLD_BUILD_DIR is given. By hand, after changing maps in `src/interp/value.*` or `runtime/c`; seeds 1 to 10 (560 programs) matched 1.0.40 exactly in 1.0.41, and seed 11 (60) matched 1.0.41 in 1.0.42. Seed 41 (40 programs) matched 1.0.48 exactly in 1.0.49, after the std type checks. Seed 51 (30 programs) matched 1.0.49 exactly in 1.0.50. Seed 71 (20 programs) matched 1.0.51 exactly before 1.0.52, after `vfs.write` and the paths took only strings.

`tools/fuzz_lists.py SEED COUNT [BUILD_DIR [OLD_BUILD_DIR]]` does the same for lists: appends, removes and inserts at the front, the back and the middle, emptying a list from the front, `pop`, indexing and setting (out of range under `try`), `for`, `==`, `+` and printing, and lists that hold each other. By hand, after changing lists in `src/interp/value.*` or `runtime/c`; seeds 1 to 9 (540 programs) matched 1.0.41 exactly in 1.0.42. Seed 41 (40 programs) matched 1.0.48 exactly in 1.0.49, after the std type checks. Seeds 51 and 52 (60 programs) matched 1.0.49 exactly in 1.0.50. Seed 71 (20 programs) matched 1.0.51 exactly before 1.0.52.

`tools/fuzz_vfs.py SEED COUNT [BUILD_DIR [OLD_BUILD_DIR]]` does the same for `std/vfs`: folders, writes (some a hundred files at once), `remove`, `remove_tree` and `rename` that work or fail, removes while going through a listing, and printing listings and the whole tree. By hand, after changing `std/vfs` or `std/maps`; seeds 1 to 8 (320 programs) matched 1.0.41 exactly in 1.0.42, and seeds 11 to 18 (320) again after the sorted-names cache. Seed 41 (40 programs) matched 1.0.48 exactly in 1.0.49, after the std type checks. Seed 51 (30 programs) matched 1.0.49 exactly in 1.0.50. Seed 71 (20 programs) matched 1.0.51 exactly before 1.0.52, after `vfs.write` and the paths took only strings.

`tools/fuzz_find_sort.py SEED COUNT [BUILD_DIR [OLD_BUILD_DIR]]` does the same for `strings.find`, `contains`, `split` and `replace` (printing whole results since 1.0.46), `upper`, `lower` and `trim` on texts with characters past ASCII (some over 1,024 characters, some like `aaa...ab`), and `lists.sort` on ints, floats with NaN and inf, ints past 2^53 next to floats, `1` next to `1.0`, strings, and lists it cannot sort. By hand, after changing `host_find`, `host_sort` or their std wrappers; seeds 1 to 8 (320 programs) matched 1.0.42 exactly in 1.0.43. Seeds 11 and 12 (80 programs) matched 1.0.45 in 1.0.46. Seed 41 (40 programs) matched 1.0.48 in 1.0.49 apart from the new `lists.sort(): xs must be a list, got none` message where 1.0.48 said `len(): cannot get the length of a none`. Seeds 51 and 52 (60 programs) matched 1.0.49 exactly in 1.0.50. Seed 71 (20 programs) matched 1.0.51 before 1.0.52 apart from the intended change: `lists.sort` of a string or a map sorts its characters or keys, where 1.0.51 failed (7 programs). Seed 91 (20 programs), after `reverse` and `find_last` stopped copying characters to a list, had every engine agree; against 1.0.51 it differed in 11 programs, all in the intended 1.0.52 changes (`strings.find` naming itself for a text that is not one, and `lists.sort` of a map).

`tools/fuzz_concat.py SEED COUNT [BUILD_DIR [OLD_BUILD_DIR]]` does the same for `x = x + t`: strings in globals, locals, parameters and closures, copies kept in other names, lists and maps, a string added to itself, `x + (slice(x, 0, 9) + t)`, numbers and lists mixed in, and `len`, indexes and slices past ASCII along the way. By hand, after changing `str_append_in_place`, `rl_add_to` or how an assignment is compiled; seeds 1 to 8 (320 programs) matched 1.0.42 exactly in 1.0.43.

`tools/fuzz_json.py SEED COUNT [BUILD_DIR [OLD_BUILD_DIR]]` decodes JSON texts that Python's `json` module wrote (nested objects and arrays, strings with quotes, backslashes, control characters and characters past the BMP, ints, floats from a list, from random bits and as random decimals, `true`, `false`, `null`, random spacing and `\u` escapes), some with one character inserted, removed or changed, and prints `json.encode` of each value or the error. It compares every engine as the others do, and also checks the VM against Python: a text is accepted exactly when Python's strict `json.loads` accepts it, `encode` writes JSON Python reads back as exactly the same value, and `decode` of `encode` and of `pretty` gives the value back. An OLD_BUILD_DIR before 1.0.47 may differ only in that release's two changes (it reads raw control characters in strings, and writes floats with 15 digits); anything else is reported. By hand, after changing `std/json` or what it uses. In 1.0.46 (with those two rules allowed for), seeds 1 to 6 (220 programs) had no mismatch and seeds 7 to 9 (120) matched 7a7b9b3 exactly; in 1.0.47, seeds 21 to 28 (320 programs) matched 1.0.46 apart from those two changes, with no other mismatch. Seed 41 (30 programs) and seed 71 (20, against 1.0.51) had no mismatch after `encode` started stopping on a value that holds itself. Seed 91 (20) had no mismatch after `decode` started reading a high surrogate escape with no low one as U+FFFD and saying `expected a value` where none can start.

## Unit tests

`tests/unit/main.cpp` (`relic_tests`, ctest case `unit`) checks the lexer, parser, interpreter and REPL from C++, where token lists or tree shape are easier to check than output. It uses a small hand-written harness, no test library. It also holds the checks that would only add programs to `selfhost_interp` for little gain: lexer, parse and built-in error messages with their positions, the REPL's details, and the built-in font.

## C runtime tests

Built with the C runtime (`runtime/c/`, on by default). Details in [C-RUNTIME.md](C-RUNTIME.md#tests).

| Test | Checks |
|---|---|
| `c_runtime` | unit tests for values, operators, messages, built-ins, files, the cycle collector, the high-DPI rounding (`src/builtins/gfx_scale.h`), the small public calls (type names, `repr`, fields, range starts, the number reader), and that nothing leaks |
| `c_runtime_hardening` | runtime allocation and value safety checks |
| `c_runtime_input` | standard input, prompts and line endings |
| `c_demo` | the hand-compiled `tests/demo.c` prints exactly `tests/golden/c_demo.out` |

`golden_c_demo` runs the same program on the C++ host, so a change to either side shows up. `selfhost_interp` compares it too (since 1.0.21; it recurses 800 calls deep).

## Compiled golden tests

Every golden except `repl_*` and those with a `.args` file also runs compiled to C: `cgen_name` (410 cases). At build time `build/relicc` writes `build/cgen/name.c` (in a host build, `selfhost/cgen.relic` on the host does), CMake builds it against `relic_rt` with the runtime's strict flags, and `tools/run_cgen.cmake` runs it in `tests/golden/` with `RELIC_LEAK_CHECK=1` (exit 3 if anything is left alive) and checks it the same way as `golden_name`. A program the generator does not cover yet would print `cgen-skip: <reason>` and ctest would report it as skipped. Today all 410 pass. `tests/cgen/*.relic`, if any, are cases for the compiled side only, with `.out` files from `relic --tree` (`cgen_only_name`); there are 32: `json_deep`, nested JSON at the recursion limit, which the Relic-written interpreter would need seconds for, `long_string`, a string constant of about 75 KB, longer than MSVC takes in one C literal, and 30 `learn_*` wrappers that `use` an example program and expect the same bytes as its `.out`. Details in [CGEN.md](CGEN.md#tests).

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

The CI job `sdl` runs a build with `-DRELIC_SDL=ON -DRELIC_C_SDL=ON` on Ubuntu 24.04 (apt), macOS (Homebrew) and Windows (vcpkg, MSVC): 906 cases each (the host build's 890 plus the 16 graphics ones), headless with `SDL_VIDEODRIVER=dummy`. It fails if any of the 16 graphics tests is missing. 406 and 422 were the host total and that total plus graphics at 1.0.52 (see the changelog). They are not the current counts.

## tablecheck application

[`examples/tablecheck/README.md`](../examples/tablecheck/README.md) describes the
standalone CSV/TSV validator. `tablecheck_goldens` is one ctest case in a
top-level C-runtime build, or in its standalone consumer build. It checks exact
stdout, stderr and exit status for valid input, input and argument errors,
compile-time syntax errors and runtime type errors, with the leak check enabled.
Its fixtures and goldens stay outside `tests/golden`, so the language-corpus
counts below are unchanged. This application check does not verify the human
README-only FizzBuzz acceptance gate.

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

## Acceptance record

One finite pass on main `ed34bba` (Relic 1.0.53), 2026-11-07. No language or library change. The version and the changelog are unchanged. This record does not say the portfolio is done.

### Frozen scope

In this tree, and exercised below:

- The language and the standard library.
- The Relic-written compiler to C (`selfhost/cgen.relic`), the C runtime (`runtime/c/`), and the committed bootstrap (`bootstrap/cgen.c`). The default build needs only a C compiler.
- The optional C++ host, REPL, and SDL graphics.

Out of this pass. These are not release blockers for that frozen scope:

- Native code generation. It stays an open stretch item on the roadmap. The C backend is the shipped compiler.
- A VM written in Relic. Dropped (M10). The host bytecode VM remains the default engine.
- New language or std features, a website, installers, release tags, or publishing.
- MONOLITH 2 apps (M12). They live in M0N0LITH-2. The IDE there has started; see [PROJECT_PLAN.md](PROJECT_PLAN.md).

### Accepted limits

Decided behavior. A program that hits one of these is inside the language as shipped:

- Self-host interpretation is slow. On this machine `selfhost_interp` took 12.29 s (one run).
- Recursion stops at 800 calls.
- Graphics libraries are optional. SDL2 for the window, SDL2_image and SDL2_ttf for PNG and TrueType.
- `gc()` in compiled code is an approximate count.
- A compiled error names the file, line and column. The source text is not embedded. The runtime prints the source line only when it can read the file at the time of the error.

### What blocks a release

A crash, a leak, a wrong result on supported behavior, a stale `bootstrap/cgen.c`, or a parity miss (VM against the tree-walker, `relicc` against the host, self-host against the host). None of those showed up in the checks below.

### Checklist

| Check | Result |
|---|---|
| README C-only configure, build, `ctest` from a clean tree | passed here: 262, 0 failed, 6.27 s |
| README compile and link of a small program | passed here. See FizzBuzz below |
| Consuming project `tests/consumer` | passed here: 6, 0 failed. `hello` printed the two expected lines |
| Host build and `ctest` | passed here: 549, 0 failed, 51.23 s |
| Bootstrap fresh, fixed point, host match | passed inside those suites |
| Parity and `doc_counts` | passed inside the host suite |
| Leak check on compiled goldens and `c_runtime` | passed (`RELIC_LEAK_CHECK` and `c_runtime`; nothing failed) |
| Host REPL and `relic --version` | passed here. Version line `relic 1.0.53`. Piped REPL printed `3` |
| Graphics example and SDL `ctest` on this Linux machine | passed here. `hello_window` printed `drew 123 frames`. SDL `ctest` 565, 0 failed, including all 16 graphics tests |
| Human FizzBuzz, README only, by a person who has not seen the code | **unverified.** The box in [1.0-CHECKLIST.md](1.0-CHECKLIST.md) stays open |
| Windows, macOS, Alpine with no C++ compiler, sanitizers | **not run on this machine.** Remote CI on the same commit is below |
| Windows and macOS graphics | **not run on this machine.** The Linux graphics run above is the local one. Remote CI is below |

The FizzBuzz row and the human row are different checks. The program for this pass was written by an agent from the README "Relic in five minutes" section (`for n in 1..21`, because `1..4` is 1, 2, 3) and compiled with the README `relicc` and `cc` lines. Stdout was 1 through 20 with Fizz, Buzz and FizzBuzz in the usual places, exit 0. The same file on the host printed the same stdout. That does not tick the human box.

### Commands on this machine

Linux. CMake 3.28.3. `/usr/bin/cc` and `/usr/bin/c++` are Clang 18.1.3. The C-only README build linked. The host configure did not, until `libstdc++-14-dev` was installed: this VM's clang looks for `libstdc++` in the gcc 14 directory, and only gcc 13's `libstdc++.so` was present. `g++` 13 linked before that package. That is this machine, not a Relic defect.

C-only, from a clean checkout (no `build/` yet), as the README writes it:

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

262 passed, 0 failed, 6.27 s. That includes `doc_counts`, `example_compiled`, `bootstrap_fresh`, `bootstrap_fixed_point`, `depth_limit`, `c_runtime`, `c_demo` and the compiled goldens. `./build/example_compiled one two` printed the three greetings, `Ada, Linus, Grace`, and `args: ["one", "two"]`.

Consumer, as CI and [the section above](#relic_add_program) write it:

```
cmake -S tests/consumer -B build-consumer -DCMAKE_BUILD_TYPE=Release
cmake --build build-consumer
ctest --test-dir build-consumer --output-on-failure
```

6 passed, 0 failed (`hello`, `doc_counts`, `bootstrap_fresh`, `bootstrap_fixed_point`, `c_runtime`, `c_demo`). `build-consumer/hello` printed:

```
hello from a consuming project
HELLO
```

Host, in `build-host` so the README tree stayed C-only. Same flags as the README host section:

```
cmake -S . -B build-host -DCMAKE_BUILD_TYPE=Release -DRELIC_BUILD_HOST=ON
cmake --build build-host
ctest --test-dir build-host --output-on-failure
```

549 passed, 0 failed, 51.23 s. Among them: `vm_matches_tree_walker`, `bootstrap_matches_host`, `bootstrap_fresh`, `bootstrap_fixed_point`, `cgen_stage1_matches_host`, `cgen_fixed_point`, `doc_counts`, `selfhost_lexer`, `selfhost_parser`, `selfhost_interp`, `selfhost_check`. `build-host/relic --version` printed `relic 1.0.53`. A pipe of `print(1 + 2)` and `:quit` into `build-host/relic` printed `3` and exited 0. There was no banner, because stdin was not a terminal.

Graphics, after `libsdl2-dev` 2.30.0, SDL2_image 2.8.2, SDL2_ttf 2.22.0 and `fonts-dejavu-core`. The example README asks for `-DRELIC_SDL=ON`. This build also passed `-DRELIC_C_SDL=ON`, which is the CI graphics build, so the compiled graphics tests ran too.

```
cmake -S . -B build-sdl -DCMAKE_BUILD_TYPE=Release -DRELIC_SDL=ON -DRELIC_C_SDL=ON
cmake --build build-sdl
SDL_VIDEODRIVER=dummy SDL_VIDEO_DRIVER=dummy ./build-sdl/relic examples/hello_window.relic
SDL_VIDEODRIVER=dummy SDL_VIDEO_DRIVER=dummy ctest --test-dir build-sdl --output-on-failure
```

`hello_window` printed `drew 123 frames` and exited 0. `ctest` was 565 passed, 0 failed, 65.95 s. The 16 graphics tests all passed: `gfx_smoke`, `gfx_events`, `gfx_simulate`, `gfx_window`, `gfx_window_errors`, `mini_desktop`, `gfx_png`, `gfx_ttf`, and the eight `c_gfx_*` / `c_mini_desktop` twins.

### Remote CI on the same commit

Not run here. Read from GitHub Actions run [42268705993](https://github.com/DanielMellen-University/Relic/actions/runs/42268705993) on `ed34bba`. All 18 jobs succeeded. Do not read this table as a local Windows or macOS run.

| Job | What the log said |
|---|---|
| `build` on Ubuntu, macOS, Windows | 262 passed, then the consumer project 6 passed |
| `no-cxx` (Alpine, no C++ compiler) | 262 passed, then consumer 6 passed |
| `host` on Ubuntu, macOS, Windows | 549 passed |
| `sdl` on Ubuntu 24.04, macOS, Windows | 16 graphics tests present, 565 passed |
| `cgen` (Linux gcc sanitize, Linux clang sanitize, macOS clang, Windows MSVC) | 262 passed on each |
| `c-runtime` (the same four) | 2 passed on each |

Installers, new demos and more goldens were not part of this pass.

## Benchmarks

`tools/bench.sh build/relic` times each `bench/*.relic` on the tree-walker and the VM and checks both print the same. It skips `redraw.relic` without graphics. It is local only, not part of CI. Use a Release build. Numbers for the same programs compiled to C are in [CGEN.md](CGEN.md#speed).
