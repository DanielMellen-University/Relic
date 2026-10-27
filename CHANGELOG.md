# Changelog

## 1.0.24 (2026-10-26)

Better messages for characters that look like something else, and more tests for behavior that had none. Parsing and running are unchanged.

- `unexpected character` now says what a pasted character is and what to type: a byte order mark at the start of a file (`U+FEFF`; save as UTF-8 without one), a no-break space (`U+00A0`), a zero-width space (`U+200B`), an ideographic space (`U+3000`), and curly single and double quotes. Invisible ones are named by code point, since the quoted character showed nothing. The same words come from the host (`src/lexer/lexer.cpp`) and `selfhost/lexer.relic`, so `bootstrap/cgen.c` is regenerated. New goldens `err_bom` and `err_curly_quote`.
- `unit` checks built-in error messages and edge results (`insert`, `remove`, `pop`, `ord`, `chr`, `int`, `float`, `keys`, `len`, `slice`), the REPL's details from `CLI.md`, and parse error positions and messages for `use`, `fun`, `let`, `try`, loops, lists, maps and calls. They run in the one `unit` case, so they add nothing to the `selfhost_interp` time.
- `c_runtime` checks `rl_type_name`, `rl_a_type`, `rl_repr`, `rl_set_field`, `rl_module_new`, `rl_range_start`, `rl_parse_num`, `rl_fail_str` and `rl_obj_value`; the file is clean under ASan and UBSan.
- Docs: `TESTING.md` says what the unit and C runtime tests cover now, and `LANGUAGE.md` mentions the new messages.
- Speed on this box against 1.0.13: `selfhost_interp` 11.61 s median of 3 (11.46, 11.61, 11.65; was 6.1 s with 136 programs, now 156; 1.90x, budget under 2x), `selfhost_nested` 1.00 s.
- `ctest`: 156 default, 327 with the host, 343 in the `sdl` CI job.

## 1.0.23 (2026-10-26)

A bug fix in `std/math` and more golden tests for behavior that had none. No language change.

- `math.abs(-0.0)` gave `-0.0`; it gives `0.0` now. `abs` of an int and of any other float is as before.
- Six new goldens, each the same on the VM, the tree-walker, compiled C and the self-hosted interpreter: `std_math_edges` (negatives, halves, int against float results, argument errors), `arith_edges` (division and remainder signs, float printing, `int()`, `float()`, `num()`, `str()`), `string_ops` (multi-byte indexing and slicing, comparing, `in`, interpolation, brace escapes), `equality` (`==` on lists, maps, none, functions), `aliasing` (shared lists and maps, `slice` copies) and `loop_forms` (empty ranges, a `for` over a copy, nested `break` and `next`, loop variable scope).
- Docs: `PROJECT_PLAN.md` no longer lists `rt_recursion`, `try_recursion` and `c_demo` as skipped, and `selfhost_nested` is about 1 s.
- Speed on this box against 1.0.13: `selfhost_interp` 11.69 s median of 3 (11.42, 11.69, 11.97; was 6.1 s with 136 programs, now 154; 1.92x, budget under 2x), `selfhost_nested` 0.95 s.
- `ctest`: 154 default, 323 with the host, 339 in the `sdl` CI job.

## 1.0.22 (2026-10-26)

Deep recursion in the self-hosted interpreter (SH3, option C), slice 6 of 6: the old recursive path is gone. SH3 is done.

- `selfhost/interp.relic` loses `eval`, `exec_body`, `call`, `condition`, `assign`, `load_module`, their frame-step constants and the old flow-normal code: about 470 lines nothing called since 1.0.21. `run` and `machine` are the only way a program runs. Nothing in the file recurses on the host for a Relic call, a block or an expression any more. No change in behavior.
- The depth limit is the host's: 800 calls, the same message (`too much recursion (more than 800 calls deep)`), the same position, catchable. New golden `recursion_limit` pins it: the 800th nested call runs and the 801st fails, for a block body, an expression body and two functions calling each other; `e.line` of the error; and the depth is back to zero after a `catch`, so the limit is the same the next time. The same output on the VM, the tree-walker, compiled C and the self-hosted interpreter.
- Speed on this box against 1.0.13: `selfhost_interp` 11.66 s median of 3 (11.66, 11.66, 11.82; was 6.1 s with 136 programs, now 148; 1.91x, budget under 2x), `selfhost_nested` 0.97 s, `bench/fib.relic` on `selfhost/run.relic` about 8.8 s (fib(25)).
- `ctest`: 148 default, 311 with the host, 327 in the `sdl` CI job. `selfhost_interp` compares 148 programs (only `repl_*` skipped).

## 1.0.21 (2026-10-26)

Deep recursion in the self-hosted interpreter (SH3, option C), slices 4 and 5 of 6: calls, returns and errors on the frame stack. The self-hosted interpreter now reaches the host's 800-call limit.

- `run` in `selfhost/interp.relic` drives one stack, `machine`, for statements, expressions and calls together, so a Relic call costs no host call. A user function call checks arity, depth and `as` hints as before, then pushes `[k_call, fun, caller_file, call]` (with `rt.file` switched to the function's file) and its body on top. When the body ends, or a `return` unwinds to it, the return hint is checked and the frame goes. A `use` of a new module pushes `[k_module, mod, caller_file, use, env]` and the module's body.
- `return` unwinds to the nearest `k_call`; `break` and `next` to the nearest loop, but not past a function or module edge (same errors as before). An error unwinds to the nearest `try` frame, leaving each call on the way like the host does (std boundary renaming, depth, `rt.file`) and dropping the values it left on the value stack. Slice 5 (errors through calls) came with slice 4 because `c_demo` needs both.
- The depth limit and its message are the host's: `too much recursion (more than 800 calls deep)` at the same call and position, and a `try` catches it. A third nested level of `run.relic` works now too (slow).
- `rt_recursion`, `try_recursion` and `c_demo` are no longer skipped: `selfhost_interp` compares 147 programs (only `repl_*` skipped).
- The old recursive `eval`, `exec_body` and `call` are still in the file, unused; slice 6 removes them.
- New golden `call_flow`: return hints, `break` at a function edge, an error 40 calls deep caught by the caller, a `try` inside a function, calls inside expressions and assignment targets, `return` from loops in a called function. The same output on the VM, the tree-walker, compiled C and the self-hosted interpreter. Shallow (4 levels of blocks); fine for `relicc` on Windows.
- Speed on this box against 1.0.13: `selfhost_interp` 11.58 s median of 3 (11.50, 11.58, 11.59; was 6.1 s with 136 programs, now 147; 1.90x, budget under 2x), `selfhost_nested` 1.51 s, `bench/fib.relic` on `selfhost/run.relic` about 9.0 s (fib(25); was 5.8 s).
- `ctest`: 147 default, 309 with the host, 325 in the `sdl` CI job.

## 1.0.20 (2026-10-26)

The built-in text font draws `_` (Daniel approved Oct 26 2026). Font fix only; no language change.

- `_` in the built-in 8x12 font (`src/builtins/font8x12.inc`, used by `graphics.text` in the host and in compiled programs) was blank: DejaVu Sans Mono draws it below the 12-row cell, so `tools/gen_font.py` picked up nothing. It is now a bar one row under the baseline, as wide as `=` (row 10, `0x7e`). `tools/gen_font.py` writes the same bar, so a rerun keeps it. Typing cursors like the `_` in M0N0LITH-2's Notes and Terminal now show.
- Every other printable glyph from `!` to `~` already drew something; only space is blank, as it should be.
- The `unit` test checks that every printable glyph but space has pixels set, that space has none, and where the `_` bar sits.
- `ctest`: 146 default, 307 with the host (unchanged; the check is in `unit`). `selfhost_interp` 8.89 s median of 3 on this box (1.0.13: 6.1 s; budget under 12.2 s). `selfhost/interp.relic` is not touched.

## 1.0.19 (2026-10-25)

Deep recursion in the self-hosted interpreter (SH3, option C), slice 3 of 6: statements and control flow on a frame stack.

- `exec_body` in `selfhost/interp.relic` runs statements on its own stack: blocks, `let`, assignment, expression statements, `if` / `else if` chains, `while`, `for`, `return`, `break`, `next`, `fun`, `use` and `try`. Frames are small lists: `[s_block, block, env, i]`, `[s_while, node, env]`, `[s_for, node, env, items, i]` and `[s_try, node, env, depth]`. Nested blocks and loops no longer recurse on the host.
- `return` unwinds to the edge of the function body (or file), `break` and `next` unwind to the nearest loop frame, and an error unwinds to the nearest `try` frame, which runs its `catch` part with the depth put back. This replaces the old flow strings (`"return"`, `"break"`, `"next"`, `"error"`) passed up through `exec` and `exec_block`, which are gone. The messages and positions for a `return`, `break` or `next` in the wrong place are unchanged. `try` moved onto the stack here because `break` and `return` have to unwind through it; errors from calls still come back through `call()` on the host until slice 4.
- Calls still go to `call()` and recurse on the host (slice 4). Depth reached: about 262 levels for a function with a block body (was 196) and 395 for one with an expression body (unchanged).
- New golden `stmt_flow`: `return`, `break` and `next` out of nested loops, `if` chains and `try` blocks, errors caught at the nearest `try` (and one raised again from a `catch`), a missing return value with an `as` hint, `break` outside a loop inside a function, and a `for` over a list that grows while it runs. The same output on the VM, the tree-walker, compiled C and the self-hosted interpreter. Shallow (5 levels of blocks); `relicc` compiles it with a 125 KB stack.
- Speed on this box against 1.0.13: `selfhost_interp` 8.79 s median of 3 (was 6.1 s; 1.44x, with one more program; 1.0.18 measures 7.98 s now), `selfhost_nested` 0.88 s (was 0.71 s), `bench/fib.relic` on `selfhost/run.relic` about 8.1 s (was 5.8 s). The budget is under 2x.
- `ctest`: 146 default, 307 with the host, 323 in the `sdl` CI job. `selfhost_interp` compares 143 programs.

## 1.0.18 (2026-10-24)

Long flat chains no longer crash `relicc` (option A, decided Oct 24 2026). Crash fix only; no language change.

- cgen walks a left-leaning chain (`a + b + c ...`, any binary operators down the left side, `and` / `or` too) with a loop instead of recursion. It writes exactly the C it wrote before: the generated C for every golden is byte for byte the same. Before, about 200 terms overflowed Windows' 1 MB stack and about 400 stopped `relicc` with "too much recursion" elsewhere. A 520-term chain now needs about 37 KB of stack on Linux, about what `hello` needs.
- `has_fun` (does a function body hold a nested `fun`) walks the body with a loop too, for the same reason.
- `bootstrap/cgen.c` is regenerated. `selfhost/interp.relic` is not touched.
- New golden `long_chain`: 520 terms of `+`, 300 of `+` and `-` with names, 200 of `*` and `/`, 400 strings, 300 of `and` and of `or`, a 250-term chain inside a function with a nested `fun`, and an error late in a chain with its line and column. The same output on the VM, the tree-walker, compiled C and the self-hosted interpreter. The compiled AST printer needs about 230 KB of stack for it on Linux, under Windows' 1 MB; the host links with a 16 MB stack on Windows.
- This closes the open item about chains from 1.0.16 (CGEN.md).
- `ctest`: 145 default, 305 with the host, 321 in the `sdl` CI job. `selfhost_interp` compares 142 programs, 7.79 s median on this box (1.0.13: 6.1 s).

## 1.0.17 (2026-10-24)

Range evaluation order is the same on every engine (option A, decided Oct 24 2026).

- The host VM checks a range start before it evaluates the end, in `a..b` and in `for i in a..b`, as the tree-walker and the self-hosted interpreter always did. `t(1.5)..t(3)` now fails with `range start must be a whole number` without running `t(3)`. New opcode `RANGE_START`, using the host's shared check (`op_range_start`).
- Compiled code did the same as the VM before, so cgen now emits `rl_range_start` (new in the C runtime, the same check `rl_range` makes) right after the start. `bootstrap/cgen.c` is regenerated.
- A good start and a bad end behave as before: both ends run and the end is reported. Error positions are unchanged.
- New golden `range_order`: good starts, bad starts with an end that prints (and one with an undefined name), and bad ends, in expressions and `for`, with each error's line and column. The range case is back in `expr_order`. Both give the same output on the VM, the tree-walker, compiled C and the self-hosted interpreter.
- `ctest`: 144 default, 303 with the host, 319 in the `sdl` CI job. `selfhost_interp` compares 141 programs, 7.87 s median on this box (1.0.13: 6.1 s).

## 1.0.16 (2026-10-24)

A nesting limit instead of a crash in `relicc` (Daniel's pick, Oct 24 2026).

- Source nesting (brackets, `-`, `not`, blocks) is limited to 32 levels, down from 100. The error is the host's existing one, `too deeply nested (more than 32 levels)`, with file, line and column.
- The limit is now in both parsers: `src/parser/parser.cpp` (the host) and `selfhost/parser.relic`, which had none. So the VM, the tree-walker, the self-hosted interpreter, `relic --ast`, the compiled `selfhost` parser and `relicc` all stop at the same token with the same message. Before, `relicc` crashed with a stack overflow on Windows near 65 levels and stopped with "too much recursion" elsewhere near 75, and the self-hosted interpreter ran out of host calls.
- Why 32: a branch-only CI probe (`probe-nesting-1024`) showed `relicc` built with MSVC overflowing Windows' 1 MB stack between 60 and 70 levels. At 32 the worst case (nested maps) needs about 290 KB on Linux, about half of Windows' stack. The deepest golden or `selfhost/` file nests 12 levels, real code 9, the M0N0LITH-2 shell 8.
- `bootstrap/cgen.c` is regenerated (it contains the parser).
- New error goldens `err_too_deep` (33 levels of brackets) and `err_too_deep_block` (33 nested `if` blocks), the same on every engine and compiled.
- Still open (CGEN.md): a long flat chain like `1 + 1 + ...` is not nesting, but cgen walks it recursively; about 200 terms are too much for `relicc` on Windows. That needs a choice.
- `ctest`: 143 default, 301 with the host, 317 in the `sdl` CI job. `selfhost_interp` compares 140 programs, 7.73 s median on this box (1.0.13: 6.1 s).

## 1.0.15 (2026-10-24)

Deep recursion in the self-hosted interpreter (SH3, option C), slice 2 of 6: expressions on the frame stack.

- `eval` in `selfhost/interp.relic` now runs every expression kind on its own stack: unary, binary (with `and` / `or` short-circuit), interpolation, lists, maps, ranges, fields and indexes. Each has a frame that waits for its parts on the value stack and then combines them, so nested expressions no longer recurse on the host. Assignment targets use the same `eval`. Calls still go to `call()` and recurse on the host (slice 4).
- The old recursive `eval_rec` and `binary` are gone; the operator rules live in `binop`, `unary_of`, `field_of` and `index_of`, with the same messages and positions. The order is unchanged: parts run left to right, the first error stops everything, and a range start is checked before its end runs (as the tree-walker does).
- Depth reached: about 196 levels for a function with a block body (was 131) and 395 for one with an expression body (was 157). A 1,000-term `1 + 1 + ...` chain now runs on the self-hosted interpreter; before it ran out of host calls.
- New golden `expr_order`: evaluation order, short-circuits and the first error in sums, lists, maps, interpolation, indexes, fields, `-` and `not`. Shallow, so `relicc` needs about 48 KB of stack. `selfhost_interp` compares 138 programs.
- Found, not fixed here: the host VM evaluates both ends of a range before checking the start, while the tree-walker checks the start first, so `t(1.5)..t(3)` runs `t(3)` on the VM (compiled code did the same). No golden covered it, so `vm_matches_tree_walker` passed. Fixed in 1.0.17.
- Speed on this box against 1.0.13: `selfhost_interp` 7.60 s median (was 6.1 s; 1.25x, with two more programs), `selfhost_nested` 0.81 s (was 0.71 s), `bench/fib.relic` on `selfhost/run.relic` 7.4 s (was 5.8 s). The budget is under 2x.
- `ctest`: 141 default, 297 with the host, 313 in the `sdl` CI job.

## 1.0.14 (2026-10-24)

Deep recursion in the self-hosted interpreter (SH3), slice 1 of 6. Daniel picked option C on Oct 24 2026: move `eval` in `selfhost/interp.relic` onto an explicit frame stack, so it can reach the host's 800-call limit.

- `eval` is now a driver loop over a frame stack. Leaves (names, strings, numbers, `true`, `false`, `none`) are answered at once with no frame. Any other node gets a small list frame `[step, node, env]` with a whole-number step code. The only step so far, `step_eval`, hands the node to the recursive code (`eval_rec`, `call`, `binary`), so nothing changes for programs. No flag; the next slices move kinds onto the stack.
- Host depth per Relic call is unchanged (about 131 levels for a function with a block body, 157 for one with an expression body), and the depth limit and its error text are still the host's.
- New golden `deep_nesting`: 10 levels of brackets, a 24-term chain, nested lists, maps, calls, `and` / `or` and interpolation. It passes on the VM, the tree-walker, compiled to C and on the self-hosted interpreter. `selfhost_interp` compares 137 programs.
- Found on the way: `relicc` (cgen compiled to C) uses about 8 KB of C stack per level of expression nesting on Linux (gcc, Release) and more with MSVC. A first version of the golden (40 levels, 150 terms, about 500 KB on Linux) overflowed Windows' 1 MB stack. The golden now needs about 110 KB on Linux. Noted in CGEN.md under open choices; nothing else changes.
- Speed on this box against 1.0.13: `selfhost_interp` 7.4 s (was 6.1 s, with one more program), `selfhost_nested` 0.74 s (was 0.71 s), `bench/fib.relic` on `selfhost/run.relic` 6.3 s (was 5.8 s). The budget is under 2x.
- `ctest`: 140 default (was 139), 295 with the host (was 293), 311 in the `sdl` CI job (was 309). Test counts in TESTING.md and CGEN.md that had drifted are corrected.

## 1.0.13 (2026-10-24)

Self-host parity (SH3), one more step:

- `selfhost/interp.relic` has `gc()`. It runs the host's cycle collector, since the interpreter's values live on the host heap, and gives the number freed. The count includes the interpreter's own containers, so it is approximate, like `gc()` in compiled code. Arity errors match the host.
- `gc_cycles` is no longer skipped in `selfhost_interp`, which now compares 136 programs (was 135). Still skipped: `rt_recursion`, `try_recursion` and `c_demo` (deep recursion, the open part of SH3) and the REPL tests.
- `ctest` counts are unchanged: 139 default, 293 with the host.

## 1.0.12 (2026-10-22)

High-DPI drawing, Daniel's pick A (Oct 22 2026). The same in the host and in compiled programs:

- Windows open with `SDL_WINDOW_ALLOW_HIGHDPI` (on Windows also SDL's DPI scaling hint), and the renderer is scaled by `scale()`, so every drawing call, clip rectangle and mouse position stays in window units. Programs need no change. On a high-DPI screen shapes and the built-in font are drawn at full resolution.
- TrueType fonts open at `size * scale()` pixels and are drawn back at window-unit size, so text is sharp. `font_size` gives window units, rounded up. `load_font` sizes are window units.
- Images and canvases are drawn at their normal size in window units (stretched on a high-DPI screen). Drawing into a canvas stays one unit per texel.
- `scale()` follows the window: it is checked on every input call, `present`, `set_window_size`, `set_fullscreen` and `draw_to(none)`, the renderer scale is updated, and fonts are opened again at the new size. While a canvas is the target, `scale()` gives the window's scale (SDL reports the canvas size as the output size then; without this it gave 0.25 in a test).
- The rounding lives in `src/builtins/gfx_scale.h`, shared by both, with unit tests in `c_runtime` for scales other than 1.0, which headless SDL cannot produce.
- Tests: `gfx_window` checks the scale is 1.0 headless, also while a canvas is the target, and `gfx_ttf` checks `font_size` gives whole numbers. Every existing output is unchanged. `ctest` counts as before: 139, 293, 147 with `-DRELIC_C_SDL=ON`, 309 with both.
- The "Windowing and drawing API" roadmap item is closed.

## 1.0.11 (2026-10-22)

The last slice of the windowing and drawing item (Daniel's pick B, Oct 22 2026). In `std/graphics`, with the same names, results and errors in the host and in compiled programs (`relic_rt_gfx`); the stub and the host without SDL fail with the usual rebuild message:

- Window: `title()`, `set_title(s)`, `window_size()` (window units, the ones mouse positions use), `set_window_size(w, h)`, `scale()` (pixels per window unit), `fullscreen()`, `set_fullscreen(on)` (the desktop's own resolution). A size change comes back as the usual `resize` event.
- Cursor: `set_cursor(name)` with 11 system shapes (`arrow`, `text`, `wait`, `crosshair`, `hand`, `move`, `no`, `resize_ew`, `resize_ns`, `resize_nwse`, `resize_nesw`); it gives false when the system cannot show the shape. `show_cursor(on)`, `cursor_visible()`.
- Frames: `set_vsync(on)` (true when the renderer took it), `vsync()`, `ticks()` (whole milliseconds since `open`), `frame_time()` (seconds between the last two presents), `present_capped(fps)` (waits so frames come no faster than `fps`).
- Not done: sharp drawing on high-DPI screens. Windows still open without high-DPI mode, so `scale()` is 1.0. How drawing coordinates should work there is a call for Daniel (PROJECT_PLAN, "Decisions needed"), so the roadmap item stays open.
- `host_names()` lists 77 built-ins (was 62); `selfhost/cgen.relic` knows the new names and `bootstrap/cgen.c` is regenerated.
- New tests `gfx_window` and `gfx_window_errors`, host and compiled (`c_gfx_window`, `c_gfx_window_errors`): every new call headless, and every error message in order, the same on both. What the dummy driver cannot do (real cursors, vsync) is checked by type. The CI `sdl` job now expects 16 graphics tests.
- `ctest`: 139 default and 293 with the host, as before; 147 with `-DRELIC_C_SDL=ON`, 309 with both SDL options.

## 1.0.10 (2026-10-22)

- CI: the `sdl` job runs on macOS (Homebrew `sdl2`, `sdl2_image`, `sdl2_ttf`) and Windows (vcpkg with MSVC, packages cached between runs) as well as Ubuntu 24.04. Each builds with `-DRELIC_SDL=ON -DRELIC_C_SDL=ON` and warnings as errors for the C, runs all 305 tests headless, and fails if any of the 12 graphics tests is missing. 18 jobs.
- Graphics fix (host and compiled): `simulate` puts its event straight into Relic's own event queue, after what SDL already holds, instead of calling `SDL_PushEvent`. Homebrew's `sdl2` is now sdl2-compat (SDL2 over SDL3), which crashed on a pushed text event (`gfx_events`, `mini_desktop` and their compiled twins segfaulted on macOS) and dropped a pushed wheel's distance. Output is the same as before on SDL2.
- Tests: `gfx_ttf` and `c_gfx_ttf` fall back to Arial when DejaVu Sans is not installed (macOS and Windows ship it); `-DRELIC_TEST_FONT=path` picks a font by hand.
- Tests: `mini_desktop` and `c_mini_desktop` share a ctest lock. Both write `examples/mini_desktop/data`, and run side by side with `ctest -j` they failed most of the time.
- Docs: how to get SDL2 on macOS and Windows (TESTING.md); the `std/graphics` header names both ways to build it.
- `ctest` counts are unchanged: 139 default, 293 with the host, 145 with `-DRELIC_C_SDL=ON`, 305 with both SDL options.

## 1.0.9 (2026-10-21)

- Graphics for compiled programs (M11): `runtime/c/rl_gfx_sdl.c` is the optional C module over SDL2 that Daniel picked on Oct 20 2026. It is a C99 port of the host's `src/builtins/graphics.cpp` with the same names, arity, results and error messages, so `std/graphics` works the same compiled. SDL2_image and SDL2_ttf are used when found.
- New CMake option `-DRELIC_C_SDL=ON` builds `relic_rt_gfx`, the runtime with the module in place of `rl_gfx_stub.c`, and `relic_add_program(name file GRAPHICS)` links a program with it (Daniel's pick, option B, Oct 21 2026). `relicc` and programs without `GRAPHICS` keep linking `relic_rt` and never need SDL2. Without the option, `GRAPHICS` falls back to the stub. The option is off by default, so the default and no-C++ builds need no SDL2. It does not need the host. The runtime is compiled once as `relic_rt_base` and packed into both libraries.
- With it, the headless SDL tests and the mini desktop example are compiled with `relic_add_program(... GRAPHICS)` and run with the leak check: `c_gfx_smoke`, `c_gfx_events`, `c_gfx_simulate`, `c_mini_desktop`, `c_gfx_png` and `c_gfx_ttf`, with the same expected output as the host's tests.
- C runtime: a window left open at exit is closed (`rl_gfx_release`), so its queued events are freed before the leak check.
- CI: new job `sdl` on Ubuntu installs SDL2, SDL2_image, SDL2_ttf and DejaVu fonts (apt with retries and timeouts) and builds with `-DRELIC_SDL=ON -DRELIC_C_SDL=ON`, so the host and compiled graphics tests run headless. 16 jobs.
- `ctest` has 139 cases in the default build and 293 with the host, as before; 145 with `-DRELIC_C_SDL=ON` and SDL2_image, SDL2_ttf and DejaVu fonts installed, 305 with both SDL options (the `sdl` CI job).

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
