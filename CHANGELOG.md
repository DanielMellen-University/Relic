# Changelog

## 1.0.40 (2026-10-30)

Fixes Daniel approved on Oct 30, a new event field, and the std/vfs speedups that were waiting on main. No change to the language.

- `relic check` gives a "did you mean" hint to the first 50 undefined names in a file, in line order; the rest are still reported, without one. Each hint looks at every name in scope, so a file full of typos spent most of its time on them (500 typos: `selfhost/check.relic` 12.5 s, now 1.4 s; the host 0.03 s, now 0.01 s). The host and `check.relic` use the same limit. Golden `check_many_typos`.
- `relic --tree` walks the tree on the native stack, so a big expression inside deep recursion (`f(n) = f(n - 1) + 1 + 1 ...` with 9,990 terms, called 700 deep) ran out of it and crashed (exit 139) before the 800-call limit. It now stops with `too much recursion (calls and expressions nest too deeply for the stack)` once it has used three quarters of the thread's stack, and `try` catches it. The VM, which does not recurse natively, runs the same program as before. `--tree` benches within 2%. Test `tree_stack`.
- `--tree`: a `catch` block runs after the C++ exception is done with, not inside the C++ catch clause (on MSVC that clause runs on top of the frames that threw).
- `std/graphics`: `key_down` and `key_up` events carry `gui` (Cmd on macOS, the Windows key elsewhere) next to `shift`, `ctrl` and `alt`, on the host and in compiled programs; `simulate` takes `"gui"` as a modifier. For Cmd+S in M0N0LITH-2.
- `std/vfs`: path names are cut out with `slice` and added to the list in place, so very long paths and names no longer take time growing with the square of their length (`vfs.exists` on 20,000 names: 0.97 s, now 0.012 s; on a 400,000-character name: 4.2 s, now 0.06 s, compiled); ordinary paths are 10 to 15% faster.
- Fuzzing found nothing new: `fuzz_gen` (150 programs), `fuzz_mutate` (`check` 200, `run` 150), `fuzz_check` (seeds 4000 to 4200), `fuzz_depth` (seed 8, 40 cases).
- Speed: `selfhost_interp` 10.15 s median of 3 (10.15, 10.19, 10.12), under the 12.2 s cap.
- `ctest`: 185 default, 391 with the host.

## 1.0.39 (2026-10-27)

Three more slow spots in the standard library and the Relic-written tools. No change to the language or to any result.

- `strings.find` on long text and `strings.split` use Knuth-Morris-Pratt, so text like `"aaa...a"` searched for `"aa...ab"` no longer takes time growing with both lengths (100,000 a's and a 501-character separator: `find` 9.3 s, now 0.09 s; `split` 9.5 s, now 0.1 s). Ordinary text runs at the same speed. Checked against the old module on 600 random long texts on all four engines. Golden `std_strings_edges`.
- `selfhost/check.relic` read the file and split it into lines again for each warning; it splits it once (`errfmt.snippet_in`). 2,000 warnings: 41.7 s, now 1.1 s. The same output as `relic check`.
- `selfhost/repl.relic` lexed the whole input so far again for each line inside a block, where only an empty line can end it. A 1,000-line function typed in: 27 s, now 0.1 s.
- `bootstrap/` regenerated.
- Fuzzing found nothing new: `fuzz_gen` (300 programs), `fuzz_mutate` (`run`, `tokens`, `check`, 700 cases), `fuzz_repl` (400 scripts), `fuzz_check` (seeds 3500 to 3700), `fuzz_depth` (seed 6, 40 cases).
- Speed: `selfhost_interp` 11.09 s median of 3 (11.09, 11.59, 11.04), under the 12.2 s cap. The new `std_strings_edges` case was cut to one `find` on 1,031 characters (the first version added 1.1 s on the Relic-written interpreter).
- `ctest`: 185 default, 389 with the host.

## 1.0.38 (2026-10-27)

More code whose time grew with the square of its input. No change to the language or to any result.

- Parser: a duplicate map key or parameter was found by comparing each new one with every one before it, in the host and in `selfhost/parser.relic`. It uses a set now (a map literal with 40,000 keys: host 2.9 s, now 0.05 s; 8,000 keys in `check.relic` 8.9 s, now 0.5 s; 4,000 parameters 1.7 s, now 0.25 s). The same errors at the same places. `bootstrap/` regenerated.
- `std/paths`: `normalize`, `base`, `dir`, `join`, `ext` and `stem` read the path as a list of characters and join pieces in halves (a 40,000-character path: `normalize` 0.30 s, now 0.10 s; `ext` 0.21 s, now 0.04 s). `strings.trim` looks at a list of characters (0.25 s, now 0.06 s). Golden `std_paths_edges`.
- `std/vfs`: `remove_tree` takes a folder from its parent at once. It emptied the folder one name at a time, copying the folder's names for each (4,000 files: 3.0 s, now 3 ms).
- Each change was checked against the old code on random input on the VM, `--tree`, `selfhost/run.relic` and compiled: 1,300 paths, 6,700 `vfs` operations, and maps and parameter lists with repeats.
- LANGUAGE.md says what long strings cost and how to read and build long text in time that grows with its length.
- Fuzzing found nothing new: `fuzz_gen` (450 programs), `fuzz_mutate` (`ast`, `check`, `run`, 1,100 cases), `fuzz_repl` (500 scripts), `fuzz_check` (seeds 3000 to 3400), `fuzz_depth` (seed 5, 40 cases).
- Speed: `selfhost_interp` 10.54 s median of 3 (10.76, 10.44, 10.54), under the 12.2 s cap.
- `ctest`: 185 default, 389 with the host.

## 1.0.37 (2026-10-27)

Speed and fuzzing: code that built a long string one piece at a time, or indexed one, took time growing with the square of its size. No change to the language or to any result.

- `std/json`: `decode` read its text by indexing the string; it reads a list of the characters now (200 KB: 3.7 s, now 0.2 s). `encode`, `pretty` and the strings `decode` builds join their pieces in halves (200,000 characters: 4.4 s, now 0.34 s; a list of 100,000 ints: 3.9 s, now 0.22 s). `decode` of a value that is not a string says `json.decode(): text must be a string, got int` (it said `len(): cannot get the length of an int`). Golden `std_json_edges`.
- `std/strings`: `find` on long text, `upper`, `lower`, `repeat`, `pad_left` and `pad_right` (80,000 characters: `find` 0.9 s, now 0.03 s; `repeat` of 200,000 characters 0.86 s, now under 1 ms). Golden `std_strings_edges`.
- `std/vfs`: every call ran `paths.normalize` twice and a split on its path; it reads the path once. `list` sorts with an in-place merge. A folder of 3,000 names: `list` 20 ms, now 12 ms; `is_dir` on each name 62 ms, now 16 ms. Same results and messages. Golden `std_vfs` covers `\`, `..`, `.` and doubled slashes.
- `selfhost/lexer.relic` built long names and strings one character at a time (a 400,000-character string: 4.3 s in `check.relic`, now 0.8 s), and `errfmt.relic` reads a long line's window in one pass. Same tokens and errors. `bootstrap/` regenerated.
- Each module change was checked against the old module on random input on the VM, `--tree`, `selfhost/run.relic` and compiled.
- `tools/fuzz_gen.py` writes text for `num()`, `float()` and `int()`: signs, `nan` / `inf` / `infinity` in mixed case, hex at the int range's edges, tiny and huge floats, near misses. 450 programs and 3,565 strings agreed on all four engines and with a model of the rules.
- New `tools/fuzz_depth.py`: expressions and nested `else if` chains within three levels of the 10,000 limit, read by `relic check`, `check.relic` and `relicc`. 132 cases agreed and failed exactly past 10,000.
- Speed: `selfhost_interp` 10.29 s median of 3 (10.29, 10.10, 10.60), under the 12.2 s cap.
- `ctest`: 185 default, 389 with the host.

## 1.0.36 (2026-10-27)

Two changes Daniel approved: `num()` reads more numbers, and a limit of 10,000 on how deep code goes, where the host used to crash.

- `num()`, `float()` and `int()` read `nan`, `inf` and `infinity` in any case, with an optional sign, and hex like `0x1F` or `-0x1f`, which gives an int when it fits (it gave a float before; past the int range it is still a float). A float too small for a normal float rounds like a literal: `num("1e-400")` is `0.0` and `num("4.9e-324")` the smallest float above zero (both were `not a number`). One too large, `nan(1)`, `infin`, spaces and the rest are still `not a number`. The host's `parse_num` and the C runtime's `rl_parse_num` take the same steps, and the Relic-written interpreter hands the text to the host's `num()`. Goldens `num_edges`, `float_tiny`.
- `json.decode` reads back a subnormal float that `json.encode` wrote (it said `bad number`). JSON stays strict: `NaN`, `Infinity` and hex are still errors. Golden `std_json_edges`.
- `selfhost/floatlit.relic` (1.0.34) is gone: `interp.relic` and `check.relic` read every float literal with `num()` now, and `relicc` writes a tiny one from `num()`'s value as a whole number of steps of 2^-1074. The C it writes is the same as before.
- An expression more than 10,000 levels deep is a parse error, `expression is too deeply nested (limit 10000)`, and so is an `else if` chain of more than 10,000 links, `'else if' chain is too long (limit 10000)`. Each operator, call, index, field, `-`, `not`, list, map and string with `{...}` adds a level, so 10,000 `+` in a row run and 10,001 do not. Before, about 55,000 links crashed the host (C stack). The host's parser and `selfhost/parser.relic` count the same way, so the VM, `--tree`, `relic check`, `run.relic`, `check.relic` and `relicc` all give the error at the same place. New test `depth_limit` (`tools/check_depth_limit.cmake` writes the 10,000 and 10,001 cases; they are not goldens, since a C compiler takes seconds on the first and the Relic-written tools about 2 s on the `else if` ones). The 32-level limit on brackets, `-`, `not` and blocks is unchanged.
- Speed: `selfhost/lexer.relic` indexed the source string, and indexing a string walks its UTF-8 from the start, so lexing took time growing with the square of the file (a 10,000-link `else if` file: 35 s in `check.relic`, now about 2 s). It reads a list of the characters now. `strings.split` copied the rest of the text after every piece; for text over 4096 characters it now walks it once (a 20,000-line file: 6.9 s, now 0.3 s). Same results.
- Speed: `selfhost_interp` 10.27 s median of 3 (10.10, 10.28, 10.27), under the 12.2 s cap.
- `ctest`: 185 default, 389 with the host.

## 1.0.35 (2026-10-27)

Standard library edge cases: wrong answers, a hang and error messages that named the wrong function. No change to the language.

- `strings.pad_left` and `strings.pad_right` with a `width` of +infinity padded forever. They now check `text`, `fill` and `width` (`strings.pad_left(): width must not be infinite`), and the empty-fill message starts with `strings.` like the rest: `strings.pad_left(): fill must not be empty` (was `pad_left(): ...`). `strings.repeat` wants a whole-number count. Goldens `std_strings_edges`, `inf_args`.
- `math.floor`, `math.ceil` and `math.round` went through a float, so an int past 2^53 lost digits (`math.floor(9007199254740993)` gave `9007199254740992`) and `math.floor` of the largest int failed. An int now comes back as it is. `math.round(0.49999999999999994)` gave 1 and `math.round(4503599627370497.0)` gave `4503599627370498`, because `x + 0.5` rounded first; it now looks at what is over the floor. Nan, an infinity or a float past the int range says `math.floor(): number is out of the int range` (was `int(): ...`), and `math.floor("a")` gets the same argument check as `ceil` and `round`. Golden `std_math_edges`.
- `paths.ext("..")` was `"."`. Leading dots are part of the name, as for `".hidden"`, so `".."` and `"...a"` have no extension. Golden `std_paths_edges`.
- `tools/fuzz_gen.py`, `fuzz_mutate.py` and `fuzz_check.py` print their usage on bad arguments (they showed a Python traceback) and find the source tree from their own path, so they run from any folder.
- Docs: STDLIB.md for all of the above, and what `os.exit` does with its code.
- All four engines agreed on every case above before and after; the fixes are in `std/`, which they share. `fuzz_gen`, `fuzz_mutate` and `fuzz_repl` with new seeds found nothing.
- Speed: `selfhost_interp` 10.31 s median of 3 (10.70, 10.13, 10.31) on a quiet box, under the 12.2 s cap.
- `ctest`: 184 default, 388 with the host.

## 1.0.34 (2026-10-27)

Float edge cases where the Relic-written tools and compiled programs differed from the host. No change to the language.

- Tiny float literals: the host reads `1e-400` as `0.0` and `4.9e-324` as the smallest float above zero (it uses `strtod`). The Relic-written interpreter, `check.relic` and `relicc` all said `float is too large`, because `num()` refuses numbers out of range in both directions. New `selfhost/floatlit.relic` works out the same float from the digits, exactly, half to even. `relicc` writes it as a whole number of the smallest step, since a C compiler warns about a literal it rounds to zero. Golden `float_tiny`, green on MSVC. `num("1e-400")` still fails on every engine, as before.
- An infinity counts as a whole number for `slice`, `chr`, `insert` and `remove` on the Relic-written interpreter, as on the host (`floor(x) == x`), which then takes the nearest int. `slice(s, -inf, 3)` failed there and worked everywhere else. Found by `fuzz_gen`. Golden `inf_args`.
- `tools/fuzz_gen.py` feeds each program a random standard input (CRLF, NUL, bad UTF-8, long lines) and calls `os.read_line` and `os.prompt`, and uses tiny and huge float literals. About 1500 more programs agree.
- Docs: C-RUNTIME.md's "Not in the runtime yet" list had been empty since 1.0.10; the float rule in LANGUAGE.md; `floatlit.relic` in ARCHITECTURE.md; what `fuzz_gen` covers in TESTING.md.
- Speed: `selfhost_interp` 10.21 s median of 3 (10.30, 10.21, 10.12) on a quiet box, under the 12.2 s cap. Up from 9.79 s: the two new goldens add about 0.1 s, and the rest looks like box noise (three readings taken right after a full ctest were 10.32 to 10.75).
- `ctest`: 184 default, 388 with the host.

## 1.0.33 (2026-10-27)

Long chains and long strings in compiled programs and the Relic-written tools. No change to the language.

- `else if` chains: `relicc` wrote one more nested C block per link. MSVC stops at 128 nested blocks and clang at 256 brackets, so a chain of a couple of hundred links would not compile there. The branches are now side by side behind a flag (5 levels of braces for 200 links, was 203). The Relic-written parser and `relicc` also walked the chain with recursion and stopped near 770 links with `too much recursion`; both use a loop now. Golden `else_if_long`.
- `relicc` walks calls, fields and indexes on one value (`f()()`, `m.a.b[0]`) with a loop, like `a + b + c`. It stopped near 264 calls or 396 fields in a row. Every golden compiles to the same C as before. Golden `long_postfix`.
- `selfhost/ast.relic` and `selfhost/check.relic` walk expressions and `else if` chains with a loop. About 790 links in one chain stopped them, while `relic --ast` and `relic check` went on. Same output as the host on 2000-link chains.
- A string constant whose C literal would pass 4000 characters is written as an array of bytes. MSVC takes 16380 characters in one literal and 65535 joined. New compiled-side case `tests/cgen/long_string` (about 75 KB), green on MSVC.
- `tools/fuzz_gen.py` covers `std/json`, `std/paths`, interpolation, more `std` calls, `else if` chains and closures. 1400 more programs agree on all four engines. `fuzz_repl` (1500 scripts), `fuzz_mutate` and `fuzz_check` (seeds 1 to 300 again after the `check.relic` change) found nothing.
- Docs: the chain limits as measured, the `else if` and long-string layouts in CGEN.md, the ctest counts.
- Speed: `selfhost_interp` 9.79 s median of 3 (9.79, 9.94, 9.76) on a quiet box, under the 12.2 s cap. Two tries at speeding up `selfhost/interp.relic` (scopes without a hints map until needed, literal caching earlier) gained under 2%, so they were left out.
- `ctest`: 182 default, 384 with the host.

## 1.0.32 (2026-10-27)

Fixes found by fuzzing, a new fuzzer, and one more test for Windows. No change to the language.

- `math.pow(a, b)` with both arguments wrong named the power first on the host (C++ ran the argument reads right to left on GCC) and the base first in compiled programs. The base comes first everywhere now. Golden `std_math_edges` covers it.
- A compiled program's error report stopped printing the source line at a NUL byte in that line; the host prints it whole. The C runtime cuts the line by length now (`c_runtime` has a case).
- NUL bytes in error messages: the host cut a message at the first NUL (`RuntimeError::what()`), and compiled programs cut the text put into `not a number: "..."` and `map has no key ...`. Both keep the whole message. Golden `nul_in_messages`.
- New `tools/fuzz_gen.py SEED COUNT`: random well-formed programs run on the VM, the tree-walker, `selfhost/run.relic` and compiled, then compared. About 2300 programs, no mismatches left. `fuzz_mutate` (150 cases per mode) and `fuzz_check` (seeds 2000 to 2400) found nothing new.
- New compiled-side case `tests/cgen/json_deep` (JSON nested to the recursion limit). A Windows program gets 1 MB of stack and MSVC frames are bigger than GCC's, so `relic_rt` now passes `/STACK:8388608` to the programs that link it on MSVC.
- Docs: the limit on very long operator chains (the host runs out of C stack at about 55,000 terms on Linux), `fuzz_gen` in TESTING.md, the ctest counts.
- Speed: `selfhost_interp` 9.71 s median of 3 (9.99, 9.71, 9.59) on a quiet box, under the 12.2 s cap. The deep JSON case is compiled-side only because it took 8.4 s on the Relic-written interpreter.
- `ctest`: 179 default, 379 with the host.

## 1.0.31 (2026-10-27)

Standard input for Relic programs, and with it the REPL written in Relic, which finishes SH4. Daniel approved the new built-in on Oct 27 2026. One new `os` pair of calls; no change to the language itself.

- New built-in `host_read_line(prompt)`, wrapped as `os.read_line()` and `os.prompt(text)` in `std/os`: the next line of standard input without its line end (`"\n"` or `"\r\n"`), or `none` at the end of the input, every time after that too. An empty line is `""`, so `none` always means the end. Only the line end is cut; a last line with no newline comes back as it is. Any length, any bytes (NUL, bytes that are not UTF-8, Ctrl-Z); stdin is read in binary mode on Windows. `os.prompt(text)` shows `text` first, but only when stdin is a terminal (so a program prints the same into a pipe), and writes a newline when the input ends there; a non-string prompt is `os.prompt(): prompt must be a string, got int`. Same behavior and messages in the tree-walker, the VM, compiled programs (`runtime/c/rl_host.c`, and the name in `selfhost/cgen.relic`, `bootstrap/cgen.c` regenerated) and `selfhost/run.relic` (it reaches the primitive through `host_call`). `host_names()` lists 78 primitives now (77 before).
- New `selfhost/repl.relic`: `relic selfhost/repl.relic` is the host's REPL written in Relic (the second half of SH4), on the Relic-written lexer, parser and interpreter. Same output, errors and exit code as the host's on every script tried: values, state, errors, blocks, multi-line headers (1.0.29), `:quit`, input that ends inside a block, CRLF, long lines, a program reading the same stdin, `use`, `os.exit`. Two differences for a person at a terminal: the banner has no version number and `:quit` prints no extra empty line. SH4 is ticked in the roadmap and the project plan.
- Test runners: a golden can have a `name.in` file, fed on stdin by `golden_name`, `cgen_name`, `vm_matches_tree_walker` and `selfhost_interp`; no file means empty input, not the terminal. `*.in` is `-text` in `.gitattributes`, so CRLF and odd bytes in them are kept.
- Tests: goldens `stdin_lines`, `stdin_edge`, `stdin_empty`, `stdin_only_newline`, `stdin_prompt` and `err_stdin_prompt_type` (lines, empty lines, CRLF, a lone CR, UTF-8, a last line with no newline, 5000- and 70000-byte lines, NUL, non-UTF-8, Ctrl-Z, end of input read again and again, an empty input) on all engines; `selfhost_repl` (the host REPL against `repl.relic` on the 4 `repl_*` goldens and 41 new scripts in `tests/repl_cases/`), `selfhost_repl_nested` (on `selfhost/run.relic`, 2 scripts) and `cgen_selfhost_repl` (compiled); `c_runtime` checks the new primitive's argument error and the 78 names. `tools/fuzz_repl.py` (by hand, needs Python) runs random scripts through both REPLs; about 5000 agreed. A terminal session (banner, prompts, Ctrl-D) was checked by hand through a pty on both REPLs.
- Speed: `selfhost_interp` 10.17 s median of 3 on a quiet box (1.67x the 6.1 s baseline; 9.84 s in 1.0.29, which had 6 fewer goldens). A first version of `stdin_edge` summed a 70000-byte line one character at a time and added 3 s to it, so long lines are sampled instead.
- `ctest`: 177 default, 376 with the host.

## 1.0.30 (2026-10-27)

One error message fix and a fuzz tool fix. No language change.

- The lexer's `unknown escape` error copied the character after the backslash into the message. A NUL there cut the host's message short (`unknown escape '\` and nothing more), and other control characters printed as nothing. A control character now gets its code, like `unexpected character` does: `unknown escape: a backslash then U+0001 (a control character; delete it)`. Host and `selfhost/` agree (golden `err_escape_control`). Found with `tools/fuzz_mutate.py`.
- `tools/fuzz_mutate.py` writes to `/tmp/relic_fuzz/MODE_SEED/`, so several runs can go at once. Before, parallel runs shared one scratch file and reported false mismatches.
- `ctest`: 171 default.

## 1.0.29 (2026-10-27)

Four places where the engines or the self-hosted tools disagreed with the host, a REPL fix, and a fuzz tool that finds this kind of thing. No language change.

- Compiled programs: a built-in (`print`, `len`, `str`, ...) called from the 800th frame failed with `too much recursion`; the VM, the tree-walker and the self-hosted interpreter allow it, since only Relic functions count toward the limit. Now they agree (golden `recursion_builtins`).
- `selfhost/run.relic`: `==` on lists and maps and printing them recursed on the host, so a list nested a few hundred levels deep stopped with `too much recursion` (and a list holding itself looped there). Both now run on their own stacks like the host: `==` gives up past 10000 levels, printing shows `[...]` past 1000 (golden `deep_values`).
- `selfhost/tokens.relic` and `selfhost/ast.relic` exited 0 after a lex or parse error. They exit 1 like `--tokens` and `--ast`, and 2 for no file, a missing file or a folder (`relic: cannot open FILE`). The lexer and parser comparisons check exit codes and those cases.
- The lexer's `unknown escape` error printed only the first byte of a multi-byte character after the backslash, a broken character; it names the whole character now, on the host (the Relic lexer already did). Golden `err_escape_utf8`.
- REPL: a block header spread over several lines (`fun add(` with the parameters on the next lines) ran the header alone and failed with `expected an indented block`; it reads the indented body once the bracket closes (golden `repl_multiline_header`).
- New `tools/fuzz_mutate.py` (by hand, needs Python): edits the test programs a little and compares `--tokens`, `--ast`, `relic check` and `run` (VM, tree-walker, `selfhost/run.relic`, compiled) with the Relic-written ones. It found the escape and exit-code bugs above; thousands of other cases agreed. A second scan of random statement programs (about 120 seeds), random expressions with 150 values each (about 160 seeds) and random JSON (80 seeds) found only the recursion-limit difference.
- Not fixed: when the source line an error points at holds a NUL byte, a compiled program stops printing that line at the NUL (the host prints it whole).
- C runtime: `==` on two cyclic lists (or maps nested 10000 levels) recursed 10000 calls deep and overflowed the 1 MB stack of a compiled program on Windows (the new `deep_values` golden found it in CI). It now uses an explicit stack; the 10000-level limit and the results are the same.
- Tests: goldens `recursion_builtins`, `deep_values`, `err_escape_utf8`, `repl_multiline_header`; `std_lists_edges` is smaller.
- Speed: `selfhost_interp` 9.84 s median of 3 on a quiet box (1.61x the 6.1 s baseline; 9.29 s in 1.0.28). Two new goldens cost about 0.5 s, and `==` and printing are a little slower on their own stacks.
- `ctest`: 170 default, 359 with the host.

## 1.0.28 (2026-10-27)

Clearer errors for wrong arguments, a folder given as a file, and control characters; a platform-dependent `slice` fixed; the self-hosted tools match the host in two more places. No language change.

- `std/math`: `abs`, `ceil` and `round` on a non-number fail with `math.abs(): argument must be a number, got string` (they said `cannot negate a string` or `cannot compare ...`). `std/strings`: `upper`, `lower`, `trim`, `split` and `repeat` say `strings.upper(): argument must be a string, got int` (they said `cannot loop over an int`).
- `slice`, `insert` and `remove` with a whole float past the int range (`slice("abc", 0, 1e300)`) use the nearest int on every engine. The cast gave the lowest int on x86, so that slice was empty and `insert([1], 1e30, 0)` named `-9223372036854775808` as the index; the selfhost interpreter also printed these indexes as floats and rejected huge ints used as indexes.
- A folder given as a file (`relic DIR`, `relic check DIR`, `files.read("DIR")`, `host_read_file` in the C runtime, and the selfhost `run` and `check`) is `cannot open`, exit 2. It used to run as an empty program and exit 0. `relicc` with a missing file or a folder says `relicc: cannot open FILE` and exits 3.
- The lexer names a control character in the source: `unexpected character U+0001 (a control character; delete it)`, on the host and in `selfhost/lexer.relic` (the message used to hold the raw character).
- `selfhost/errfmt.relic` cuts very long source lines to the host's 160-byte window around the column, so errors from `selfhost/run.relic` and `check.relic` on such lines read the same.
- Tests: goldens `std_paths_edges`, `std_lists_edges`, `index_big_floats`, `err_long_line`, `err_control_char`; `std_math_edges` and `std_strings_edges` cover the new messages; new `relicc_cli`; `cli`, `selfhost_cli` and `selfhost_check` try a folder; `c_runtime` covers float indexes and a folder (786 checks).
- Speed: `selfhost_interp` 9.29 s median of 3 on a quiet box (1.52x the 6.1 s baseline; an earlier set read 9.44 s; five new goldens).
- `ctest`: 167 default, 352 with the host.

## 1.0.27 (2026-10-27)

`relic check` written in Relic (the first half of SH4), and one bug fix in the self-hosted interpreter. No language change, and the host's `relic check` is untouched.

- New `selfhost/check.relic`: `relic selfhost/check.relic FILE...` prints the same warnings as `relic check FILE...`, in the same order, with the same exit codes (scopes and late names, use before definition, "did you mean", `return`, `break` and `next` in the wrong place, number literals that do not fit, syntax errors, files that cannot be opened). It uses the Relic-written lexer, parser and interpreter and needs no new built-in.
- `selfhost/interp.relic`: an integer literal past 64 bits ran as a float. It is now the host's error, `int is too large (the limit is 9223372036854775807)`. New goldens `err_int_literal_large` and `err_float_literal_large`.
- Tests: `selfhost_check` compares `check.relic` with `relic check` on 15 files in `tests/check_cases` and on every `.relic` file in the tree, and on runs with several files and missing ones; `cgen_selfhost_check` does the same with `check.relic` compiled to C; `selfhost_check_nested` runs it on the Relic-written interpreter. The lexer and parser tests also read `tests/check_cases`. `tools/fuzz_check.py` makes random programs for a bigger comparison by hand (seeds 1 to 1500 agree).
- The REPL half of SH4 is not done: it needs a built-in that reads standard input, which Relic does not have. SH4 stays open on the roadmap.
- Speed: `selfhost_interp` 8.78 s median of 3 on a quiet box (1.44x the 6.1 s baseline; the new goldens add little).
- `ctest`: 161 default, 341 with the host.

## 1.0.26 (2026-10-27)

Bug fixes in `std/strings`, `std/json`, `relic check` and the self-hosted interpreter's exit codes, a faster `selfhost_interp`, and tests that check what the docs say. No language change.

- `std/strings`: `pad_left` and `pad_right` with an empty `fill` never ended. They fail with `pad_left(): fill must not be empty` when padding is needed (an empty `fill` that is not needed is fine).
- `std/json`: `decode` follows JSON's number grammar. `01`, `1.e3`, `1e` and `+1` were accepted or reported at the wrong place; a number now ends where JSON says, and the error points at the character that cannot continue it. `encode` fails on inf and nan (`json: cannot encode inf or nan`) instead of writing `inf` and `nan`, which no JSON reader takes.
- `relic check` with no file used to say `relic: cannot open check`; it prints the usage and exits 2.
- `selfhost/run.relic` exited 0 after an error. It exits 1 like `relic`, 2 with no file or a file that cannot be opened (with the host's `relic: cannot open x` line), and still passes a program's own `os.exit` code through. `selfhost_interp` and `vm_matches_tree_walker` now compare exit codes, so this cannot come back unseen.
- Tests: new goldens `std_strings_edges` and `std_json_edges`; `c_runtime` covers operator and condition errors (`not`, `-`, `*`, negation, `%`, comparisons), `equal`, `cond`, `loop_items`, `check_type`, `map_remove` and the built-in table; `cli` (exit codes, usage errors, args, `relic check`, the REPL on piped input) and `selfhost_cli` are new; golden and compiled error tests must exit with code 1, not just any nonzero code; `doc_counts` fails when the program counts in `TESTING.md` and `CGEN.md` stop matching `tests/golden`.
- Speed: golden `gc_cycles` makes 100000 pairs of cycles instead of 200000 (still past the collector's 100000-container threshold, so the automatic collection is still tested). `selfhost_interp` takes 8.49 s median of 3 on a quiet box (8.49, 8.49, 8.75; 1.0.25 measured 11.37 s the same way, 1.0.13 was 6.1 s with 136 programs, now 158; 1.39x, budget under 2x).
- `ctest`: 159 default, 334 with the host.

## 1.0.25 (2026-10-26)

A bug fix in `std/vfs`. No language change.

- `std/vfs`: a path that goes through a file, like `/home/welcome.txt/x`, stopped the program with `map has no key 'kids'` in `exists`, `is_dir`, `read`, `size`, `append` and `remove`. `STDLIB.md` says `exists` and `is_dir` never fail, and now they do not: such a path is missing, and the others fail with their usual message. The `std_vfs` golden covers it (no new program, so `selfhost_interp` is unchanged).
- Speed: `selfhost_interp` 12.0 s median of 3 on a busy box (the same box measured 12.2 s without the change, 11.61 s earlier today; 1.90x was the quiet figure), `selfhost_nested` unchanged.
- `ctest`: 156 default, 327 with the host, 343 in the `sdl` CI job.

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
