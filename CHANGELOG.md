# Changelog

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
