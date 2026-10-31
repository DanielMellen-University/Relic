# Relic

Relic is the programming language for MONOLITH 2.

This repo is the language only: the compiler to C and its C runtime, the C++ host, the standard library, the self-hosted lexer, parser and interpreter, tests, tools, examples and docs. The MONOLITH 2 desktop and its apps are written in Relic in their own repo, [M0N0LITH-2](https://github.com/DanielMellen-University/M0N0LITH-2).

> Status: **1.0.46**. Relic 1.0 was tagged `v1.0.0` (MIT). What it has now:
>
> - The language with modules, a standard library (strings, lists, maps, math, files, paths, json, os, time, an in-memory filesystem `std/vfs`, and graphics) and UTF-8 strings.
> - A compiler written in Relic (`selfhost/cgen.relic`, [CGEN.md](docs/CGEN.md)) that turns Relic into C for the runtime in `runtime/c/` ([C-RUNTIME.md](docs/C-RUNTIME.md)). It compiles itself, and its own C is committed in `bootstrap/`, so the default build needs only a C compiler.
> - An optional C++ host with a tree-walker, a bytecode VM (the default engine), the REPL and SDL2 graphics. Compiled programs get the same graphics with `-DRELIC_C_SDL=ON`.
> - A lexer, parser, interpreter, `relic check` and REPL written in Relic (`selfhost/`). The interpreter can run a program inside another, with its output captured, a step limit, refused built-ins, modules read from elsewhere and input fed line by line; MONOLITH 2's IDE runs programs that way (1.0.44 and 1.0.45).
> - Searching, building a string with `x = x + t` and taking items off the front of a list take time in line with their size, and sorting n log n, not the square of it (1.0.42 and 1.0.43).
>
> The [changelog](CHANGELOG.md) has every version and the [roadmap](docs/ROADMAP.md) what is next.

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

## Docs

Start at the [docs index](docs/README.md). The short list:

- [Guide](docs/GUIDE.md) and [language reference](docs/LANGUAGE.md)
- [Standard library](docs/STDLIB.md)
- [Command line and REPL](docs/CLI.md)
- [Roadmap](docs/ROADMAP.md) and [changelog](CHANGELOG.md)

[M0N0LITH-2](https://github.com/DanielMellen-University/M0N0LITH-2) is written in Relic.

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md). In short: run `ctest` before pushing, every bug fix adds a test, and new features need Daniel's approval first.

## License

MIT. See [LICENSE](LICENSE).
