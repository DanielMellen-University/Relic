# Relic

Relic is the programming language for MONOLITH 2.

This repo is the language only: the compiler to C and its C runtime, the C++ host, the standard library, the self-hosted lexer, parser and interpreter, tests, tools, examples and docs. The MONOLITH 2 desktop and its apps are written in Relic in their own repo, [M0N0LITH-2](https://github.com/DanielMellen-University/M0N0LITH-2).

> Status: **1.0.15**. Relic 1.0 was tagged `v1.0.0` (MIT). Since then it gained modules (M7), a standard library (M8), a lexer, parser and interpreter written in Relic (M9), a bytecode VM (M10), and optional SDL2 graphics (M11), now for compiled programs too (`-DRELIC_C_SDL=ON` and `relic_add_program(... GRAPHICS)`) with window, cursor and frame control (1.0.11), and sharp on high-DPI screens (1.0.12). The Relic-written interpreter gained `gc()` in 1.0.13 and a frame stack for expressions in 1.0.14 and 1.0.15. Relic now compiles itself: a compiler written in Relic (`selfhost/cgen.relic`, [CGEN.md](docs/CGEN.md)) turns Relic into C for the runtime in `runtime/c/` ([C-RUNTIME.md](docs/C-RUNTIME.md)). Its own C is committed in `bootstrap/`, so the default build needs only a C compiler (C1 to C5). The C++ host (tree-walker, VM, REPL, graphics) is optional. See the [roadmap](docs/ROADMAP.md).

## A taste

```
fun greet(who)
    print("hello {who}")

for name in ["Ada", "Linus", "Grace"]
    greet(name)
```

## Build

Needs a C99 compiler (gcc, clang or MSVC) and CMake 3.16 or newer. No C++ compiler and nothing else.

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

That builds the compiler from the committed C in `bootstrap/`, has it compile `selfhost/cgen.relic` from this checkout into `build/relicc`, and compiles and runs every golden program. To compile your own program:

```
./build/relicc hello.relic -o hello.c
cc -std=c99 -O2 -I runtime/c hello.c build/runtime/c/librelic_rt.a -lm -o hello
./hello
```

On Windows, link `build\runtime\c\Release\relic_rt.lib` and leave out `-lm`.

In a CMake project, add Relic (for example as a `relic/` submodule) and let `relic_add_program` do both steps:

```cmake
add_subdirectory(relic)
relic_add_program(hello src/hello.relic)   # makes the executable target hello
```

It works the same inside this repo (`examples/compiled/` is built that way) and needs only a C compiler. Details are in [CGEN.md](docs/CGEN.md#in-cmake).

### The C++ host

The host runs programs directly, has the REPL and the SDL2 graphics, and is the reference the compiled side is checked against. It needs a C++17 compiler:

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DRELIC_BUILD_HOST=ON
cmake --build build
./build/relic hello.relic
```

Run `./build/relic` alone for the REPL. For graphics add `-DRELIC_SDL=ON` (needs SDL2; it turns the host on); see [examples/](examples/README.md). Compiled programs get the same `std/graphics` with `-DRELIC_C_SDL=ON` and `relic_add_program(name file GRAPHICS)`, which needs SDL2 but not the host ([C-RUNTIME.md](docs/C-RUNTIME.md#graphics)). The host build runs the host's own tests too and checks that the host and the compiled compiler write the same C.

## Relic in five minutes

```
# comments start with #
let count = 3              # declare with let
count = count + 1          # change it without let

let name = "Relic"
print("hello {name}")      # {} puts a value inside a string

# blocks are indented 4 spaces, no braces or colons
if count > 3 and name != ""
    print("big")
else if count == 3
    print("three")
else
    print("small")

let i = 0
while i < 3
    i = i + 1

for n in 1..4              # 1, 2, 3 (the end is left out)
    if n == 2
        next               # skip to the next turn
    print(n)

fun add(a, b)
    return a + b
fun double(x) = x * 2      # one-line form

let xs = [3, 1, 2]
append(xs, 4)
print(xs[0], len(xs))      # lists start at 0

let box = {title: "MONOLITH", version: 2}
box.version = 3
print(box.title, box["version"])
```

- Math: `+ - * / %`. `%` is the remainder: `10 % 3` is `1`.
- Numbers: `3` is an `int`, `3.5` and `1e3` are `float`. `/` always gives a float (`6 / 2` is `3.0`); `int(x)` cuts it back.
- Compare: `== != < <= > >=`. Logic: `and or not` (need real bools; stop early). Membership: `x in list`.
- Values: ints, floats, strings, `true`, `false`, `none`, lists and maps. Strings are UTF-8 and immutable: `len("héllo")` is `5`. Lists and maps are shared by reference; `==` compares them by value.
- Conditions must be `true` or `false`. `if 1` is an error. `if` always takes an indented block (no one-line `if`).
- A `fun` or `let` name exists only after that line has run. Put definitions above the top-level code that calls them (or call them from another function).
- Modules: `use lib/mathx` loads `lib/mathx.relic` next to your file; then call `mathx.square(3)`. Names starting with `_` stay private to their file.
- Standard library: `use std/strings`, then `strings.split("a,b", ",")`. Modules: strings, lists, maps, math, files, paths, json, os, time, vfs, graphics ([STDLIB.md](docs/STDLIB.md)).
- Errors: `try` / `catch err` catches any runtime error (`err.message`); `fail("why")` raises one. `relic check FILE` finds mistakes without running.
- The sample above is checked by the `readme_sample` golden test.

More: the [guide](docs/GUIDE.md) walks through all of it, and the [reference](docs/LANGUAGE.md) has every rule.

## How it relates to MONOLITH

- **MONOLITH 1.0** is finished and lives in its own repo. It stays C++ and SDL2. Relic and the planned Relic IDE do not go there.
- **MONOLITH 2** is the next version. Its desktop shell and apps are written in Relic in [M0N0LITH-2](https://github.com/DanielMellen-University/M0N0LITH-2). That repo pins a Relic commit as a submodule and builds it.
- Language work lands here; desktop and app work lands in M0N0LITH-2.
- The long-term goal is a stack where nothing is borrowed. The C++ host was the scaffold: since 1.0.7 the default build no longer needs it. See [VISION.md](docs/VISION.md).

## Docs

Start at the [docs index](docs/README.md). The short list:

- [Guide](docs/GUIDE.md) and [language reference](docs/LANGUAGE.md)
- [Standard library](docs/STDLIB.md)
- [Command line and REPL](docs/CLI.md)
- [Roadmap](docs/ROADMAP.md) and [changelog](CHANGELOG.md)

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md). In short: run `ctest` before pushing, every bug fix adds a test, and new features need Daniel's approval first.

## License

MIT. See [LICENSE](LICENSE).
