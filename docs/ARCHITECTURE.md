# Architecture

How Relic is built today: the C++17 host, its two engines, memory, and the Relic-written copies in `selfhost/`. Everything here is **Default** unless marked **Open**. Built-ins and embedding are in [HOST.md](HOST.md).

## Folders

```
Relic/
  CMakeLists.txt
  src/
    main.cpp        command line: picks REPL, file runner or a debug dump
    core.*          Session (REPL and tree-walk runner), VM runner, dumps
    lexer/          source text to tokens, INDENT/DEDENT
    parser/         tokens to AST
    ast/            AST node types and the --ast dump
    interp/         values, tree-walk interpreter, modules, ops.hpp (shared operations), gc.cpp (cycle collector)
    vm/             bytecode compiler and stack VM (the default engine)
    builtins/       built-in functions; graphics.cpp holds the optional SDL2 ones (font8x12.inc is its generated font)
    diag/           error messages with file, line, column
  std/              standard library, written in Relic
  selfhost/         lexer, parser and interpreter written in Relic
  examples/         small language examples (see examples/README.md)
  bench/            benchmark programs for tools/bench.sh
  tests/
    golden/         .relic programs with expected output (helpers in modules/)
    sdl/            headless graphics tests, SDL builds only
    unit/           C++ tests
  tools/            golden runner, comparison scripts, bench.sh, gen_font.py
  docs/
```

## Build targets

| Target | What it is |
|---|---|
| `relic_core` | static library: lexer, parser, AST, tree-walker, VM, built-ins |
| `relic` | the command line program, links `relic_core` |
| `relic_tests` | unit tests, links `relic_core` |

Options: `-DRELIC_SDL=ON` builds the SDL2 graphics built-ins (off by default). With it, SDL2_image (PNG, JPG) and SDL2_ttf (TrueType) are used when CMake finds them. `RELIC_STD_DIR` is set to the source tree's `std/` at build time; `$RELIC_STD` overrides it at run time. The version comes from `project(relic VERSION ...)` in `CMakeLists.txt` through `src/version.hpp.in`.

## Pipeline

```
source -> lexer -> tokens -> parser -> AST -> compiler -> bytecode -> VM   (default)
                                          \-> tree-walk interpreter         (--tree, REPL)
```

- **File runner:** `relic FILE` compiles the whole file and runs it on the VM. `relic --tree FILE` uses the tree-walker.
- **REPL:** runs each input on the tree-walker through the `Session` class (`src/core.cpp`), keeping globals alive between inputs.
- **Shared operations:** math, comparisons, indexing, fields, loops and built-in calls live in `src/interp/ops.hpp`, so both engines give the same results and errors.
- **Modules:** both engines load each file once, cache it by resolved path before running it (so cycles work), and read module names live.
- **Errors** from any stage go through `diag/` and name the file they happened in.
- **`relic check`** runs the lexer, the parser and the VM compiler's name resolution without running the program (`check_source` in `src/core.cpp`).

## Lexer

- Leading spaces become `INDENT` and `DEDENT` tokens, so the parser sees blocks as it would see braces.
- Inside `( )`, `[ ]` and `{ }` newlines are dropped.
- An interpolated string becomes `STRING_PART`, `INTERP_START`, the inner tokens, `INTERP_END`, and a final `STRING_END`. A plain string is one `STRING` token.

## Parser

- Recursive descent with the precedence table in [LANGUAGE.md](LANGUAGE.md#operators).
- `else if` chains are stored as an `If` inside the `else` slot.
- `return` is accepted anywhere by the parser; using it outside a function is a runtime error (and a `relic check` warning).
- Nesting is capped at 100 levels so hostile input cannot overflow the C++ stack.

## Bytecode VM

The default engine since M10 (`src/vm/`). `relic --bytecode FILE` prints the compiled code.

- One flat instruction list per function. Jumps for `if`, `while`, `for`, `and`, `or`, `break` and `next`.
- Names resolve at compile time to a scope slot (hops up the chain, then slot). Top-level names are numbered globals; names found nowhere fall back to the built-ins.
- Only blocks that declare names get a scope at run time. Each `for` turn gets its own scope.
- Each file compiles to its own program with its own numbered globals. A module value reads its fields live from that file's globals.
- Each Relic call is one C++ call of the VM loop, so it is cheap on the host stack. The 800-call limit is the same as the tree-walker's.

- Late names: a nested function can read or set a name that its enclosing function declares further down. The compiler reserves two slots for it, the value and a mark (`LOAD_LATE` / `STORE_LATE`), and the `let` sets the mark (`MARK`). Until the mark is set, the access falls back to the next instruction, which reaches the outer name. This gives the same result as the tree-walker's run-time lookup (golden `vm_late_local`).

The only difference left between the engines is that the REPL uses the tree-walker.

### VM speed

`tools/bench.sh build/relic` on a Release build times both engines and checks they print the same. What makes the VM fast:

- `BINARY` carries its operator as a number. int and float math and compares skip the general path unless something unusual happens (overflow, division, mixed types).
- `for i in a..b` counts on the stack (`FOR_RANGE_PREP`, `FOR_RANGE_NEXT`) instead of building the list.
- Scopes that no closure kept, and value stacks, are reused instead of allocated per call or loop turn.

Fastest of 7 runs, Release, VM, before and after the speed pass (commit 76cfc43):

| bench | before | after |
|---|---|---|
| fib | 39 ms | 32 ms |
| lists | 50 ms | 41 ms |
| loops | 264 ms | 157 ms |
| strings | 46 ms | 32 ms |

## Values

One `Value` type: none, bool, int, float, string, list, map, function, built-in, module.

## Memory

- Lists, maps, functions and scopes are reference counted with the host's `std::shared_ptr`. Strings and numbers are plain values.
- Cycles: `src/interp/gc.cpp` keeps every list, map, function and scope on one list. A collection counts the references each one gets from other containers. If an object's total count is higher, something outside the heap (a C++ local, the VM stack, a module) holds it, so it is a root. Everything reachable from a root lives; the rest is cleared, and the reference counts free it.
- When: at calls and loop turns, once more than 100,000 containers exist and the count has doubled since the last collection, or when the program calls `gc()`. The same collector serves both engines.
- Cost: about 10 percent on `bench/`, from tracking each container.
- Deep nesting is freed with a work queue, not one C++ call per level.
- Still borrowed: the counts come from `std::shared_ptr`. A Relic-written runtime would own them.

## Self-host

`selfhost/` holds the lexer, parser and interpreter again, written in Relic. They still run on the C++ host.

| File | What it is |
|---|---|
| `lexer.relic` | `lex(src)` gives `{tokens, err}`; stops at the first error |
| `tokens.relic` | `relic selfhost/tokens.relic FILE` prints the same dump as `--tokens` |
| `tokens_fmt.relic` | string quoting shared by the dumps |
| `parser.relic` | `parse(tokens)` gives `{tree, err}`; nodes are maps `{kind, text, line, col, kids}` |
| `ast.relic` | `relic selfhost/ast.relic FILE` prints the same tree as `--ast` |
| `interp.relic` | tree-walk interpreter with every built-in, `try` / `catch`, `as` hints, int and float, UTF-8, `use` (own files and `std/`) |
| `errfmt.relic` | prints errors the way the host does |
| `run.relic` | `relic selfhost/run.relic FILE` runs a program like `relic FILE` |

How it works:

- After the first error, every step sees an `ERR` token (parser) or a stored `rt.err` (interpreter), so the work winds down without checks at each call.
- Functions and modules are maps tagged with the key `chr(1)`, which Relic source cannot write by accident. Modules are cached by normalized path.
- Number math and `int()` / `float()` run on the host inside a `try`, so an int overflow becomes the program's own error.
- `host_*` built-ins are reached through `host_call`, which is how `use std/...` works.

What works:

- `relic selfhost/run.relic FILE args...` passes the args to the program (`selfhost_args`).
- It can run itself: `relic selfhost/run.relic selfhost/run.relic FILE` works (`selfhost_nested`, about 0.7 s). To make that possible, tagged maps also carry a list under `chr(2)` that is checked by identity (`host_same`), so the inner copy's functions look like plain maps to the outer copy.
- `num()` hands the string to the host's `num()`, so the answers and errors are the same (golden `num_edges`).

Limits today:

- Every Relic call inside it costs several host calls, so recursion stops at about 130 levels instead of 800, and the error comes from the interpreter itself, so the program cannot catch it. `rt_recursion` and `try_recursion` are skipped in the comparison for that reason. A third nested level of `run.relic` hits the same limit.
- No `gc()`; the host collector already covers its maps. `gc_cycles` is skipped.
- No REPL, no VM, no `check`.
- It is slow: `bench/fib.relic` takes about 5.4 s on it against about 35 ms on the host VM (Release).

What still comes from the C++ host: running all of it, reference counting and the cycle collector, number parsing, files, clocks, float math, process access and graphics (`host_*`), and the command line.

## Where this is going

Daniel picked the bootstrap: a Relic compiler written in Relic that emits C, built by a normal C compiler. The C++ host is stage 0 until that compiler compiles itself, and then leaves the default build. Milestones C1 to C5 are in [PROJECT_PLAN.md](PROJECT_PLAN.md#self-host-and-bootstrap). The Relic-written interpreter above stays as a testing aid.
