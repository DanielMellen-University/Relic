# Architecture (bootstrap host)

How the phase 1 C++17 host is laid out. Everything here is **Default** unless marked **Open**.

## Folders

```
Relic/
  CMakeLists.txt
  src/
    main.cpp        entry point: picks REPL or file runner
    lexer/          source text to tokens, INDENT/DEDENT
    parser/         tokens to AST
    ast/            AST node types
    interp/         tree-walk interpreter, values, environments
    builtins/       print and other built-in functions
    diag/           error messages with file, line, column
  tests/
    golden/         .relic programs with expected output
    unit/           C++ tests for lexer and parser
  tools/
    run_golden.*    golden test runner
  docs/
```

## Build targets

| Target | What it is |
|---|---|
| `relic_core` | static library: lexer, parser, AST, interpreter |
| `relic` | the command line program, links `relic_core` |
| `relic_tests` | unit tests, links `relic_core` |

Keeping the core in a library means the REPL, the file runner, tests and the future IDE all use the same code.

## Pipeline

```
source text -> lexer -> tokens -> parser -> AST -> interpreter -> output
```

- **File runner:** reads the whole file, runs the pipeline once.
- **REPL:** runs the pipeline on each line or block. It keeps one global environment alive between inputs, so variables and functions persist.
- Errors from any stage go through `diag/` so both modes report them the same way.

## Values

One `Value` type covering number, string, bool, none, list, map and function. **Open:** how values are freed. Phase 1 uses host smart pointers. The real model is chosen in M7.

## Debug flags

- `--tokens` prints the token stream
- `--ast` prints the tree
- `--version`

## Later

- Phase 3: each stage is rewritten in Relic, one at a time, and must pass the same golden tests.
- Phase 4: a bytecode compiler and VM slot in after the AST, so the pipeline becomes `AST -> bytecode -> VM`.
