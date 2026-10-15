# Project plan

This plan turns the [roadmap](ROADMAP.md) into milestones you can work through in order. Week counts assume part-time work and are estimates, not deadlines.

## Milestone map

| # | Milestone | Depends on | Estimate |
|---|---|---|---|
| M0 | Repo and docs | none | done (draft) |
| M1 | Host skeleton: build, CI, test runner | M0 | done |
| M2 | Lexer | M1 | done |
| M3 | Parser and AST | M2 | done |
| M4 | Tree-walk interpreter | M3 | done |
| M5 | REPL and file runner | M4 | done |
| M6 | Relic 1.0 release | M5 | done once CI is green and `v1.0.0` is tagged |
| M7 | Memory model and modules | M6 | 2 to 3 weeks |
| M8 | Standard library core | M7 | 3 weeks |
| M9 | Self-host | M8 | 4 to 6 weeks |
| M10 | Bytecode VM | M9 | 3 weeks |
| M11 | Graphics API and desktop shell | M10 | 6+ weeks |
| M12 | Apps and IDE | M11 | ongoing |

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

### M7 to M12
Planned at a coarser level. Each gets its own checklist when M6 is done.
- M7 exit: memory model chosen and written up. Two files can import each other.
- M8 exit: stdlib covers strings, lists, maps, files, time, math, each with tests.
- M9 exit: the Relic lexer, parser and interpreter written in Relic pass the same golden tests as the host.
- M10 exit: VM passes all golden tests and beats the tree-walker on a benchmark set.
- M11 exit: a window opens, draws and takes input from a Relic program, then the shell runs on it.
- M12 exit: terminal, editor, file browser, drawing app, one game and the IDE run in Relic.

## Risks

| Risk | Effect | Plan |
|---|---|---|
| Syntax keeps changing | Rework in parser and tests | Freeze 1.0 syntax before M3 |
| Memory model picked late | Rewrite of runtime | Decide in M7, before stdlib |
| Self-host too large | Stall at phase 3 | Port one stage at a time, lexer first |
| Tree-walker too slow for desktop | Laggy shell | VM (M10) comes before shell (M11) |
| Borrowed pieces creep in | Breaks the domestic goal | Keep the borrowed list in BOOTSTRAP.md current |

## Decisions needed

These are **Open** and block the milestone shown.

| Decision | Blocks |
|---|---|
| ~~Final 1.0 syntax~~ frozen in M3; colons, `++` and `const` stay out of 1.0 | done |
| Number types | M3 |
| ~~License~~ MIT, chosen by Daniel | done |
| Memory model | M7 |
| Module system shape | M7 |
| Graphics backend for phase 5 | M11 |
