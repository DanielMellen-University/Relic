# Project plan

This plan turns the [roadmap](ROADMAP.md) into milestones you can work through in order. The table shows where each one stands.

## Milestone map

| # | Milestone | Depends on | Status |
|---|---|---|---|
| M0 | Repo and docs | none | done |
| M1 | Host skeleton: build, CI, test runner | M0 | done |
| M2 | Lexer | M1 | done |
| M3 | Parser and AST | M2 | done |
| M4 | Tree-walk interpreter | M3 | done |
| M5 | REPL and file runner | M4 | done |
| M6 | Relic 1.0 release | M5 | done (`v1.0.0`; human FizzBuzz check still open) |
| M7 | Memory model and modules | M6 | done (cycle collector added later) |
| M8 | Standard library core | M7 | done |
| M9 | Self-host | M8 | done (lexer, parser, interpreter in Relic; limits in DESIGN.md) |
| M10 | Bytecode VM | M9 | host VM done and default; VM in Relic open |
| M11 | Graphics API and desktop shell | M10 | started (SDL2 slices 1 and 2: drawing, input, text, BMP images) |
| M12 | Apps and IDE | M11 | not started |

Phases in the roadmap map to milestones like this: phase 1 is M1, phase 2 is M2 to M6, phase 3 is M9, phase 4 is M7, M8 and M10, phase 5 is M11, phase 6 is M12.

**Default:** the memory model and modules (phase 4 work) come before self-hosting, because a self-hosted compiler needs imports and real memory handling. That pulls part of phase 4 ahead of phase 3.

## Milestones

### M1: Host skeleton
- Work: folder layout from [ARCHITECTURE.md](ARCHITECTURE.md), CMake build, CI, golden test runner from [TESTING.md](TESTING.md).
- Exit: `relic --version` prints a version. CI builds on push. One empty golden test passes.
- Risk: build tool churn. Keep to CMake and the standard library.

### M2: Lexer
- Work: tokens for numbers, strings, names, keywords, operators, `#` comments, and INDENT/DEDENT from leading spaces.
- Exit: a `--tokens` debug flag prints tokens for any file. Lexer tests pass, including bad input.
- Risk: string escapes and UTF-8. Support `\n \t \" \\` and `{}` interpolation only in 1.0. Indentation errors need clear messages.

### M3: Parser and AST
- Work: expressions with precedence, statements, blocks, functions.
- Exit: a `--ast` flag prints the tree. Syntax errors give file, line and column.
- Risk: syntax is not settled. Lock the 1.0 syntax in DESIGN.md before starting M3.

### M4: Interpreter
- Work: walk the AST, environments for scope, function calls, lists, `print`.
- Exit: FizzBuzz, factorial and a sort program pass as golden tests.
- Risk: closures and scope bugs. Add scope tests early.

### M5: REPL and file runner
- Work: `relic` opens a REPL, `relic file.relic` runs a file. Same pipeline for both.
- Exit: REPL keeps state between lines and survives errors.

### M6: Relic 1.0
- Exit: every box in [1.0-CHECKLIST.md](1.0-CHECKLIST.md) ticked, CI green, tag `v1.0.0`.
- Status: tagged `v1.0.0` with CI green on all three OSes. **Open:** a person still needs to write FizzBuzz from the README.

### M7 to M12
Planned at a coarser level than M1 to M6. Status under each exit line.
- M7 exit: memory model chosen and written up. Two files can import each other. **Done:** reference counting (DESIGN.md), `use` with cycles, golden tests `use_*` and `err_use_*`.
- M8 exit: stdlib covers strings, lists, maps, files, time, math, each with tests. **Done:** `std/*.relic`, golden tests `std_*`.
- M9 exit: the Relic lexer, parser and interpreter written in Relic pass the same golden tests as the host.
  - Stage 1, lexer: **done.** `selfhost/lexer.relic` matches `relic --tokens` on every golden, std and selfhost file, errors included (`selfhost_lexer` test).
  - Stage 2, parser: **done.** `selfhost/parser.relic` gives the same tree and the same errors as `relic --ast` on every file (`selfhost_parser` test).
  - Stage 3, interpreter: **done.** `selfhost/interp.relic` runs 91 golden programs with the same stdout and stderr as the host (`selfhost_interp` test). Skipped, see DESIGN.md: REPL tests, modules (`use_*`, `std_*`, `err_use_*`), `rt_recursion`, `try_recursion` and `gc_cycles`.
  - **M9 done** (with those limits). The C++ host still runs the Relic-written stages; dropping the host for good needs `use` in the self-hosted interpreter and a faster runtime (M10).
- M10 exit: VM passes all golden tests and beats the tree-walker on a benchmark set.
  - Host VM: **done.** The VM is the default engine for `relic FILE`, including `use`, cycles and `std/`. It matches the tree-walker (`relic --tree FILE`) on all 109 golden programs (everything except the REPL tests) (`vm_matches_tree_walker` test) and is 1.2x to 2.1x faster on `bench/` (`tools/bench.sh`, local only). The REPL still runs on the tree-walker.
  - Still open for M10: the VM written in Relic (the self-hosted side), and VM speed work.
- M11 exit: a window opens, draws and takes input from a Relic program, then the shell runs on it.
  - Slice 1: **done.** `std/graphics` (open, clear, rect, pixel, present, quit_requested, wait) on SDL2 behind `-DRELIC_SDL=ON`; `examples/hello_window.relic`. 
  - Slice 2: **done.** Keyboard and mouse input, built-in bitmap text, BMP images; `examples/input_demo.relic`; headless `gfx_smoke` ctest in SDL builds. Next: PNG images, desktop shell.
- Language completion (after M11 slice 2): **done.** Catchable errors, int and float, `as` hints, private names, UTF-8 strings and the cycle collector, each with golden tests, on both engines and the self-hosted interpreter.
- M12 exit: terminal, editor, file browser, drawing app, one game and the IDE run in Relic.

## Risks

| Risk | Effect | Plan |
|---|---|---|
| Syntax keeps changing | Rework in parser and tests | Freeze 1.0 syntax before M3 |
| Memory model picked late | Rewrite of runtime | Decided in M7: reference counting |
| Self-host too large | Stall at phase 3 | Ported one stage at a time; all three stages done |
| Self-hosted side too slow or missing `use` | Host never retired | Add `use` to `selfhost/interp.relic`, then a VM in Relic |
| Tree-walker too slow for desktop | Laggy shell | Host VM done and default; more speed work before the shell |
| Borrowed pieces creep in | Breaks the domestic goal | Keep the borrowed list in BOOTSTRAP.md current |

## Decisions needed

Struck-through rows are decided. The rest are **Open** and block the milestone shown.

| Decision | Blocks |
|---|---|
| ~~Final 1.0 syntax~~ frozen in M3; colons, `++` and `const` stay out of 1.0 | done |
| ~~Number types~~ separate 64-bit `int` and `float` | done |
| ~~Catchable errors~~ `try` / `catch err`, `fail(message)` | done |
| ~~Type hints~~ optional `as` hints, checked at run time | done |
| ~~Private names~~ leading `_` in a module | done |
| ~~Strings~~ UTF-8, counted by character | done |
| ~~License~~ MIT, chosen by Daniel | done |
| ~~Memory model~~ reference counting plus a cycle collector | done |
| ~~Module system shape~~ `use path/name`, one file one module | done |
| ~~Graphics backend for phase 5~~ SDL2 by default (matches MONOLITH 1), optional build | done |
