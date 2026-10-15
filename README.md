# Relic

Relic is the programming language for MONOLITH 2.

> Status: **M5 done, REPL**. `relic file.relic` runs Relic programs and `relic` on its own opens a REPL. FizzBuzz, recursion, sorting, closures, lists and maps all work, and errors show `file:line:col`. Next up is the 1.0 release (M6). CI runs on every push. The 1.0 syntax is frozen (see [DESIGN.md](docs/DESIGN.md)).

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
- Compare: `== != < <= > >=`. Logic: `and or not`. Membership: `x in list`.
- Values: numbers, strings, `true`, `false`, `none`, lists and maps.
- Conditions must be `true` or `false`. `if 1` is an error.
- The sample above is checked by the `readme_sample` golden test.
- Built-ins: `print`, `len`, `append`, `str`, `keys`, `type`. The full rules are in [DESIGN.md](docs/DESIGN.md).

## Use

| Command | Does |
|---|---|
| `relic file.relic` | runs a program |
| `relic` | opens the REPL |
| `relic --tokens file.relic` | prints the tokens |
| `relic --ast file.relic` | prints the syntax tree |
| `relic --version` | prints the version |

### REPL

```
$ ./build/relic
relic 0.0.1. Blocks end with an empty line. Type :quit or press Ctrl-D to leave.
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
- Phase 1 uses a temporary C/C++ host to bootstrap the first interpreter. That host is a scaffold and gets replaced once Relic can build itself.

## How it relates to MONOLITH

- **MONOLITH 1.0** is finished and lives in its own repo. It stays C++ and SDL2. Relic and its IDE do not go there.
- **MONOLITH 2** is the next version. Its desktop shell and apps will be rewritten in Relic.
- Relic lives here so the language can grow on its own schedule.

## Docs

- [Vision](docs/VISION.md): the goal and the non-goals
- [Roadmap](docs/ROADMAP.md): phased milestones
- [Design](docs/DESIGN.md): the shape of the language (draft)
- [Project plan](docs/PROJECT_PLAN.md): milestones, exit criteria, risks
- [Architecture](docs/ARCHITECTURE.md): bootstrap host layout
- [Bootstrap](docs/BOOTSTRAP.md): empty repo to `print("hello")`
- [Testing](docs/TESTING.md): golden tests and CI
- [1.0 checklist](docs/1.0-CHECKLIST.md): what a first usable Relic needs

## Contributing

Design decisions are still open. Items marked **Default** or **Open** in the docs are proposals, not final calls.
