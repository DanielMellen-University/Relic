# Relic

Relic is the programming language for MONOLITH 2.

This repo is the language only: the C++ host, the standard library, the self-hosted lexer, parser and interpreter, tests, tools, examples and docs. The MONOLITH 2 desktop and its apps are written in Relic in their own repo, [M0N0LITH-2](https://github.com/DanielMellen-University/M0N0LITH-2).

> Status: **1.0.1**. Relic 1.0 was tagged `v1.0.0` (MIT). Since then it gained modules (M7), a standard library (M8), a lexer, parser and interpreter written in Relic (M9), a bytecode VM that is the default engine (M10), and optional SDL2 graphics with input, text, images and fonts (M11). The language itself is feature complete: catchable errors, int and float, type hints, private names, UTF-8 strings and a cycle collector. Next up is self-hosting. See the [roadmap](docs/ROADMAP.md).

## A taste

```
fun greet(who)
    print("hello {who}")

for name in ["Ada", "Linus", "Grace"]
    greet(name)
```

## Build

Needs a C++17 compiler and CMake 3.16 or newer. Nothing else.

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/relic --version
ctest --test-dir build --output-on-failure
```

Then run a file with `./build/relic hello.relic`, or start the REPL with `./build/relic`. For graphics, build again with `-DRELIC_SDL=ON` (needs SDL2); see [examples/](examples/README.md).

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
- The long-term goal is a stack where nothing is borrowed. The C++ host is a scaffold: it goes away once Relic can run itself. See [VISION.md](docs/VISION.md).

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
