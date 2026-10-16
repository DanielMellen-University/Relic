# Architecture (host)

How the C++17 host is laid out today. Everything here is **Default** unless marked **Open**.

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
  shell/            MONOLITH 2 desktop in Relic: wm.relic (model), desktop.relic (drawing)
  examples/         small programs, such as hello_window.relic
  bench/            benchmark programs for tools/bench.sh
  tests/
    golden/         .relic programs with expected output (helpers in modules/)
    unit/           C++ tests
  tools/            golden runner, comparison scripts, bench.sh
  docs/
```

## Build targets

| Target | What it is |
|---|---|
| `relic_core` | static library: lexer, parser, AST, tree-walker, VM, built-ins |
| `relic` | the command line program, links `relic_core` |
| `relic_tests` | unit tests, links `relic_core` |

Options: `-DRELIC_SDL=ON` builds the SDL2 graphics built-ins (off by default). `RELIC_STD_DIR` is set to the source tree's `std/` at build time; `$RELIC_STD` overrides it at run time.

## Pipeline

```
source -> lexer -> tokens -> parser -> AST -> compiler -> bytecode -> VM   (default)
                                          \-> tree-walk interpreter         (--tree, REPL)
```

- **File runner:** `relic FILE` compiles the whole file and runs it on the VM. `relic --tree FILE` uses the tree-walker.
- **REPL:** runs each line or block on the tree-walker through the `Session` class (`src/core.cpp`), keeping globals alive between inputs.
- **Shared operations:** math, comparisons, indexing, fields, loops and built-in calls live in `src/interp/ops.hpp`, so both engines give the same results and errors.
- **Modules:** both engines load each file once, cache it by resolved path before running it, and read module names live. See DESIGN.md.
- Errors from any stage go through `diag/` and name the file they happened in.

## Values

One `Value` type: none, bool, int, float, string, list, map, function, built-in, module. Memory is reference counted with host smart pointers, and `src/interp/gc.cpp` collects cycles (see DESIGN.md, Memory model).

## Debug flags

- `--tokens` prints the token stream
- `--ast` prints the tree
- `--bytecode` prints the compiled code
- `--tree` runs on the tree-walker
- `--version`

## Next

- **Open:** a VM written in Relic, so the self-hosted side has a fast engine.
- **Open:** dropping the C++ host for language logic once the Relic side can run itself, including `use`.
