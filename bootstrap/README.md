# bootstrap/

`cgen.c` is stage 0 of the Relic compiler: `selfhost/cgen.relic`, with every module it uses (`lexer`, `parser`, `errfmt`, `std/files`, `std/strings`), compiled to C by cgen itself. It is generated. Do not edit it by hand.

It is here so a fresh checkout builds Relic with only a C compiler and `runtime/c/`, no C++ host (milestone C5, Daniel's choice, Oct 21 2026).

## How the build uses it

1. `relicc_stage0` is built from `bootstrap/cgen.c` and `runtime/c/`.
2. `relicc_stage0` compiles `selfhost/cgen.relic` from this checkout to `build/bootstrap/stage1.c`, which is built as `relicc` (stage 1). So a change to `selfhost/` takes effect in the same build, even before `cgen.c` is regenerated.
3. `relicc` compiles programs: `relicc prog.relic -o prog.c`.

## Keeping it fresh

`ctest` case `bootstrap_fresh` fails when `cgen.c` is not what cgen writes from the current `selfhost/` and `std/`. To rewrite it, from a configured build:

```
cmake -DBUILD=build -P tools/regen_bootstrap.cmake
```

It builds `relicc`, has it compile `selfhost/cgen.relic`, writes the result here (with the comment from `tools/bootstrap.cmake` on top) and repeats until nothing changes, usually one round. Commit the new `cgen.c` with the change that caused it.

File names in `cgen.c` are relative to the repo root (cgen's `--root` flag), so the file does not depend on where the checkout is.

If a change to cgen means the old `cgen.c` cannot compile the new source, regenerate with the C++ host instead (`-DRELIC_BUILD_HOST=ON`):

```
./build/relic selfhost/cgen.relic selfhost/cgen.relic --root . -o out.c
```

then put the comment from `tools/bootstrap.cmake` on top of `out.c` and save it as `bootstrap/cgen.c`. With the host built, `bootstrap_matches_host` checks the host writes the same file.
