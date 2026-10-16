# Relic

Relic is the programming language for MONOLITH 2.

> Status: **Relic 1.0** is released (tag `v1.0.0`, MIT license). Since then: modules with `use` (M7), a standard library in `std/` (M8), a lexer, parser and interpreter written in Relic (M9), and a bytecode VM that is now the default engine (M10). Graphics on SDL2 have started as an optional build (M11: drawing, keyboard and mouse input, text, BMP and PNG images, TrueType fonts, plus a Relic desktop shell with drag, resize and a native Terminal). The language is now feature complete: catchable errors, int and float, type hints, private names, UTF-8 strings and a cycle collector (see [DESIGN.md](docs/DESIGN.md)).

## A taste

```
fun greet(who)
    print("hello {who}")

for name in ["Ada", "Linus", "Grace"]
    greet(name)
```

## Build

Needs a C++17 compiler and CMake 3.16 or newer.

```
cmake -S . -B build
cmake --build build
./build/relic --version
ctest --test-dir build --output-on-failure
```

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
- The sample above is checked by the `readme_sample` golden test.
- Modules: `use lib/mathx` loads `lib/mathx.relic` next to your file; then call `mathx.square(3)`. `use lib/mathx as m` renames it. Names starting with `_` stay private to their file. Two files can use each other.
- Standard library: `use std/strings`, `std/lists`, `std/maps`, `std/math`, `std/files`, `std/paths`, `std/json`, `std/os`, `std/time`, `std/graphics`. For example `strings.split("a,b", ",")` or `lists.sort(xs)`. Most of it is written in Relic itself. Full tables: [STDLIB.md](docs/STDLIB.md).
- Graphics (optional): build with `cmake -S . -B build-sdl -DRELIC_SDL=ON` (needs SDL2), then `build-sdl/relic examples/hello_window.relic` or `examples/input_demo.relic`. Programs use `use std/graphics`. App-shaped example: `examples/mini_desktop/`. Headless GUI tests: [Running headless](docs/STDLIB.md#running-headless--testing-gui-apps) (`SDL_VIDEODRIVER=dummy`). The first MONOLITH 2 desktop sketch: `build-sdl/relic shell/desktop.relic`.
- Engines: `relic FILE` runs on the bytecode VM. `relic --tree FILE` uses the original tree-walker, and `relic --bytecode FILE` shows the compiled code.
- Self-host: `relic selfhost/tokens.relic FILE` runs the lexer written in Relic; `relic selfhost/ast.relic FILE` runs the Relic-written parser; `relic selfhost/run.relic FILE` runs a program on the interpreter written in Relic.
- Built-ins: `print`, `len`, `append`, `str`, `keys`, `type`, plus `slice`, `ord`, `chr`, `num`, `int`, `float`, `pop`, `insert`, `remove`, `fail`, `gc`, `args`, `eprint`. The full rules are in [DESIGN.md](docs/DESIGN.md).

### Errors and type hints

```
fun half(n as int) as float = n / 2

try
    print(half(4))
    print(half("4"))
catch err
    print("caught: {err.message}")
```

- `try` / `catch err` catches any runtime error; `err` is `{message, line, col}`. `fail("why")` raises one.
- `as` hints on `let`, parameters and returns are checked while the program runs.

## Use

| Command | Does |
|---|---|
| `relic file.relic [args...]` | runs a program on the bytecode VM; `args()` returns the extra words |
| `relic --tree file.relic` | runs it on the tree-walk interpreter instead |
| `relic check file.relic...` | finds mistakes without running: syntax errors, names never defined (with a "did you mean"), top-level use before `fun`/`let`, `break` or `return` in the wrong place. Exit 1 if any |
| `relic` | opens the REPL (runs on the tree-walker) |
| `relic --tokens file.relic` | prints the tokens |
| `relic --ast file.relic` | prints the syntax tree |
| `relic --bytecode file.relic` | prints the compiled bytecode |
| `relic --version` | prints the version |

`--vm` is still accepted and means the same as the default.

### REPL

```
$ ./build/relic
relic 1.0.0. Blocks end with an empty line. Type :quit or press Ctrl-D to leave.
> let x = 6
> x * 7
42
> fun greet(who)
...     print("hello {who}")
...
> greet("Daniel")
hello Daniel
```

- Variables and functions stay defined until you leave.
- A bare expression prints its value. `none` is not printed.
- A line starting with `fun`, `if`, `while`, `for` or `else` opens a block. Type the body indented, then an empty line to run it. The one-line `fun f(x) = ...` form runs right away.
- An open `(`, `[` or `{` keeps reading lines until it closes.
- An error prints `<repl>:line:col: error: ...` and the REPL keeps going. Line numbers count from the start of that input.
- Piped input works too: `relic < script.relic` runs it line by line, without prompts.

## What Relic is

- A small, readable language built from scratch for MONOLITH 2.
- The long-term goal is a stack where nothing is borrowed. Relic, its runtime, its standard library, the desktop shell and the apps are all written in-house.
- A C++17 host runs Relic today. It is a scaffold: the lexer, parser and interpreter already exist in Relic too (`selfhost/`), but they still run on the host. The host goes away once Relic can run itself.

## How it relates to MONOLITH

- **MONOLITH 1.0** is finished and lives in its own repo. It stays C++ and SDL2. Relic and its IDE do not go there.
- **MONOLITH 2** is the next version. Its desktop shell and apps will be rewritten in Relic.
- Relic lives here so the language can grow on its own schedule.

## Docs

- [Vision](docs/VISION.md): the goal and the non-goals
- [Roadmap](docs/ROADMAP.md): phased milestones
- [Design](docs/DESIGN.md): the language, runtime rules, modules, std, self-host, VM, graphics
- [Project plan](docs/PROJECT_PLAN.md): milestones, status, risks, decisions
- [Architecture](docs/ARCHITECTURE.md): host layout, build targets, engines
- [Bootstrap](docs/BOOTSTRAP.md): how the host started, borrowed pieces list
- [Standard library](docs/STDLIB.md): every std/ module (including [headless GUI testing](docs/STDLIB.md#running-headless--testing-gui-apps))
- [MONOLITH 2 readiness](docs/MONOLITH2-READINESS.md)
- [Testing](docs/TESTING.md): golden tests, comparison tests, CI
- [1.0 checklist](docs/1.0-CHECKLIST.md): what 1.0 needed, and the one open check

## Contributing

Items marked **Default** in the docs are choices made to keep moving and can still change. Items marked **Open** need a decision. Run `ctest --test-dir build` before pushing; every change keeps the tests green.

## License

MIT. See [LICENSE](LICENSE).
