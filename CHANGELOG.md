# Changelog

## 1.0.8 (2026-10-21)

- New CMake helper `relic_add_program(name file [DEPENDS files...])` in `cmake/Relic.cmake` (Daniel's pick, Oct 21 2026; no `relicc run` mode). It compiles a `.relic` program with `relicc`, builds the C with `runtime/c`, and makes the executable target `name`, in this tree or in a project that adds Relic with `add_subdirectory`. It reruns relicc when the file, any `.relic` under its folder, `std/` or relicc changes; `DEPENDS` adds modules from elsewhere.
- The top-level `CMakeLists.txt` uses its own folder for paths, so it works as a subproject. There it leaves out the 134 compiled goldens by default.
- New example `examples/compiled/` (ctest `example_compiled`) and a consuming project `tests/consumer/`, built in CI by the `build` jobs (Linux, macOS, MSVC) and `no-cxx`.
- `ctest` has 139 cases in the default build and 293 with the host.

## 1.0.7 (2026-10-21)

Bootstrap without the C++ host (milestone C5, Daniel's choice, Oct 21 2026):

- `bootstrap/cgen.c` is committed: `selfhost/cgen.relic` and the modules it uses, compiled to C by cgen itself. The default build now needs only a C compiler and CMake. It builds `relicc_stage0` from that file and `runtime/c/`, then `relicc` from `selfhost/cgen.relic` in the checkout, and compiles and runs every golden with `relicc`. No C++ compiler is looked for.
- The C++ host (`relic`: tree-walker, VM, REPL, graphics) is optional: `-DRELIC_BUILD_HOST=ON` builds it with all its tests, as before. `-DRELIC_SDL=ON` turns it on.
- New tests: `bootstrap_fresh` (the committed C matches what cgen writes from the current sources), `bootstrap_fixed_point` (`relicc` writes the C it was built from), and with the host, `bootstrap_matches_host` and `cgen_stage1_matches_host` (`relicc` and the host write the same C for all 134 compiled goldens).
- `cmake -DBUILD=build -P tools/regen_bootstrap.cmake` rewrites `bootstrap/cgen.c` after a change to `selfhost/` or `std/`.
- cgen: new `--root DIR` flag writes file names relative to `DIR`, so the committed C does not depend on where the checkout is. No other change to the C it writes.
- CI: 15 jobs. `build` is now the C-only build on Linux, macOS and MSVC; new `no-cxx` builds and tests on Alpine with no C++ compiler installed; new `host` runs the host build with the cross-checks on all three; `cgen` runs the C-only build strict and sanitized.
- From scratch on an 8-core Linux box (gcc, Release): about 27 s with `-j8` (24 s to get `relicc`, most of it compiling the 1 MB C file twice) and 46 s with `-j1`.
- PROJECT_PLAN and CGEN record Daniel's decisions (Oct 21 2026): C5 as above, one C file per program for modules, and an error position mark at each failing call site. The last two were interim.

Also in this release:

- VM: a nested function that reads or assigns a name which more than one enclosing function declares after it now finds the nearest `let` that has run. Before, a read could skip past it to a "not defined" error, an assignment could fail with "not declared", and a function that had already declared the name itself could read the outer one instead. The tree-walker and compiled C were already right. New golden `late_nested`.
- Host: indexing and slicing a long string (256 bytes or more) with multi-byte characters uses the same table of every 64th character offset as the C runtime, kept for the last four strings indexed. The new `bench/utf8.relic` (40,000 mixed characters, every one indexed) went from 1.36 s to 0.19 s on the VM. No output change.
- `ctest` has 138 cases in the default build and 292 with the host.

## 1.0.6 (2026-10-20)

- VM: a name whose `as` hint changes between `let`s is now checked against the hint of the latest `let` that ran, also when a nested function assigns to it, as on the tree-walker. Before, the VM used the hint where the function was defined, and for top-level names the last hinted `let` in the file, even for assignments above it. Stores from a nested function to a name its enclosing function declares later are now checked too. New golden `hint_relet` (tree-walker, VM and compiled C agree; it replaces the cgen-only case).
- cgen: a hinted parameter that a later `let` re-hints now starts with its hint (found by the new golden).
- C runtime: indexing and slicing a long string with multi-byte characters no longer walks from the start each time; it keeps a table of the byte offset of every 64th character. A loop over 40,000 mixed characters went from 1.3 s to 17 ms. No output change; new golden `utf8_long` and runtime tests (678 checks).
- PROJECT_PLAN records the C3 choices (decided by Engineer, Oct 20 2026) and the C5 bootstrap question, pending Daniel.
- `ctest` has 286 cases.

## 1.0.5 (2026-10-20)

Code generator, full language (milestones C3 and C4):

- `selfhost/cgen.relic` now compiles `use`: the program's own modules and `std/`, compiled into the same C file (interim decision, pending Daniel's confirmation). First-use order, caching, cycles, a module using the main file back, modules that fail partway, missing modules, lex and parse errors in modules, private and "(yet)" names, and the std names at the boundary (`files.list(): ...`) match the host.
- The last refused case compiles too: a name whose `as` hint changes between `let`s, assigned from a nested function. It follows the tree-walker; the VM differs there (listed as an open choice).
- All 131 compiled goldens pass (106 before), clean under ASan, UBSan and LSan and the leak check, on Linux, macOS and Windows.
- cgen compiles itself: stage 1 (cgen compiled by cgen) writes the same C as stage 0 (`cgen_fixed_point`). The lexer and parser compiled to C match `--tokens` and `--ast` on all 168 corpus files (`cgen_selfhost_lexer`, `cgen_selfhost_parser`).
- Runtime: compiled modules (`rl_module_scoped`), the std boundary (`rl_std_boundary`, `rl_fun_std`); fixed `host_list_dir` sorting (crashed with more than one entry); direct string indexing when every character is one byte.
- `std/strings.join` joins halves instead of growing one string, so long joins are no longer quadratic (same results).
- Speed: cgen compiling itself takes about 0.2 s compiled against about 1.1 s on the host; the compiled lexer is about 12 times faster than running it on the host.
- New goldens `use_cache` and `err_use_parse`; `ctest` has 283 cases.
- PROJECT_PLAN records the C2 choices (decided by Engineer, Oct 20 2026, internal, no output change) and the interim module decision.

## 1.0.4 (2026-10-20)

Code generator (milestone C2):

- New `selfhost/cgen.relic`: a Relic-to-C compiler written in Relic. It reads a program with the Relic-written lexer and parser and writes C99 for `runtime/c/`: `relic selfhost/cgen.relic prog.relic -o prog.c`. It covers the whole language except `use`: literals, operators, strings and interpolation, lists, maps, `let` and assignment, `if` / `while` / `for`, functions, recursion, closures, `as` hints, `try` / `catch` and `fail`, and every built-in. The C program prints what `relic` prints and fails with the same message and position. See [CGEN.md](docs/CGEN.md).
- Error positions: each call site that can fail marks its position, behind one helper so a line table can replace it later. Recorded in PROJECT_PLAN as an interim decision (Oct 20 2026), pending Daniel's confirmation.
- Runtime additions for generated code: unset slots, `rl_at3` (index and range errors point at the operand, as on the host), `rl_fail_undefined` with the host's "did you mean" hint, `rl_interp`. Fixed a NULL compare in the runtime's map-key hint.
- Tests: every golden except REPL and `.args` tests is also compiled and run as `cgen_name` with a leak check at exit: 106 of 129 pass, 23 skip (they use modules), none fail. `ctest` now has 275 cases.
- CI: a `cgen` job builds the generated C with warnings as errors on gcc and clang (Linux, with ASan, UBSan and LSan), clang on macOS and MSVC on Windows.
- Speed: `fib(30)` runs in 141 ms compiled, against 363 ms on the host VM and 947 ms on the tree-walker.

## 1.0.3 (2026-10-20)

C runtime (milestone C1):

- New `runtime/c/`: a plain C99 runtime for compiled Relic, built next to the C++ host (which is unchanged and still the default). It has the values, reference counts and the host's cycle collector, UTF-8 strings, ordered maps, scopes and closures, modules, the error-flag calling convention for `try` / `catch`, the language built-ins and the `host_*` primitives, with the host's messages. Graphics is a stub that answers like a non-SDL host; the SDL2 module comes later.
- Tests: `c_runtime` (unit tests, cycle collection, leak checks) and `c_demo` (a program compiled by hand the way the code generator will, matching the host on the new golden `c_demo`).
- CI: a `c-runtime` job builds the runtime on its own with warnings as errors on gcc and clang (Linux, with ASan, UBSan and LSan), clang on macOS and MSVC on Windows.
- Docs: [C-RUNTIME.md](docs/C-RUNTIME.md) describes the ABI. PROJECT_PLAN records Daniel's C decisions (Oct 20 2026): reference counts plus a cycle collector, C99 on gcc, clang and MSVC, an error flag checked after every call, graphics as an optional SDL2 module.

## 1.0.2 (2026-10-20)

Fixes:

- The REPL opens a block on `try` and `catch`, like `if` and `fun` (golden `repl_try`).
- The VM now matches the tree-walker when a nested function reads or sets a name its enclosing function declares further down: the outer name until that `let` runs, the local after (golden `vm_late_local`).
- Assigning to a built-in without `let` (`print = 1`) is an error on both engines and in the self-hosted interpreter (golden `err_assign_builtin`). It used to be allowed on the tree-walker.

Self-host:

- `selfhost/run.relic FILE args...` passes the args to the program (`selfhost_args`).
- `run.relic` can run itself on a program (`selfhost_nested`).
- `num()` in the self-hosted interpreter gives the host's answers, including `inf`, `nan` and hex (golden `num_edges`).
- New roadmap: a Relic compiler in Relic that emits C (milestones SH1 to SH4 and C1 to C5 in PROJECT_PLAN.md).

Docs:

- New: [guide](docs/GUIDE.md), [language reference](docs/LANGUAGE.md), [command line and REPL](docs/CLI.md), [host built-ins and embedding](docs/HOST.md), [CONTRIBUTING.md](CONTRIBUTING.md), [examples/README.md](examples/README.md).
- The [docs index](docs/README.md) has a reading order for users and for contributors.
- DESIGN.md now holds the reasons and open questions; the rules moved to LANGUAGE.md and the engine internals to ARCHITECTURE.md.
- Fixed stale facts: self-host limits (`use std/...` works there), test counts, the std module list, the borrowed pieces list (now one list, in VISION.md).
- Version 1.0.2 in CMake, the README, and the REPL banner shown in the docs.

## 1.0.1 (2026-10-20)

- Split the repo. The MONOLITH 2 desktop shell (`shell/wm.relic`, `shell/desktop.relic`) and its `shell_wm` golden test moved to [M0N0LITH-2](https://github.com/DanielMellen-University/M0N0LITH-2), history included. Relic is now the language only.
- The planned Relic IDE moved from Relic's roadmap to M0N0LITH-2's, as a planned app.
- Docs call `examples/mini_desktop/` what it is: a small language example, not the real environment.
- No language, std or engine changes.

## 1.0.0

- First release, tag `v1.0.0`. Work after it (modules, std, self-host, VM, graphics, M7 to M11) landed on main without a version bump. See [PROJECT_PLAN.md](docs/PROJECT_PLAN.md).
