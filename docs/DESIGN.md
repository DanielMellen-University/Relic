# Design (draft)

Everything here is a proposal. **Default** marks a choice made to keep moving. **Open** marks a question that needs Daniel's call.

## Voice

Relic should not read like C, C++ or Java. The aim is short lines, few symbols, and words where other languages use punctuation.

- **Default:** blocks come from indentation, not braces.
- **Default:** no semicolons and no parentheses around conditions.
- **Default:** logic uses words: `and`, `or`, `not`.
- **Default:** comments start with `#`.

## A taste

Sketch only, not final syntax:

```
# greet everyone on the list
let names = ["Ada", "Linus", "Grace"]

fun greet(who)
    print("hello {who}")

for name in names
    if name == "Linus"
        print("skip")
    else
        greet(name)

let box = {title: "MONOLITH", version: 2}
print(box.title)
```

## File extension

**Default:** `.relic`. Open alternative: `.rl`.

## Blocks

- **Default:** a line that opens a block (`fun`, `if`, `else`, `while`, `for`) has no colon. The next lines indented one level belong to it.
- **Default:** indent with 4 spaces. Tabs are an error, so files look the same everywhere.
- **Default:** no `end` keyword.
- **Open:** whether a trailing colon (Python style) helps readability enough to keep.

## Values

- Numbers: **Default:** one number type (64-bit float) for 1.0. **Open:** separate int and float later.
- Strings: UTF-8, immutable, in double quotes. **Default:** `{name}` inside a string inserts a value. `{{` writes a literal brace.
- Bools: `true`, `false`.
- Nothing: `none`. **Default** over `nil` or `null`, to read as a plain word.
- Lists: `[1, 2, 3]`, growable, any value. Index from 0.
- Maps: `{key: value}`. Keys are names or strings. Read with `box.title` or `box["title"]`. **Default:** maps are in 1.0 since the literal is part of the language's look.

## Typing

**Default:** dynamic typing for 1.0. **Open:** optional type hints later.

## Variables

- **Default:** `let x = 1` declares. `x = 2` reassigns. Assigning to an undeclared name is an error.
- **Open:** a `const` form for names that never change.

## Operators

| Kind | Relic |
|---|---|
| Math | `+ - * / %` |
| Compare | `== != < <= > >=` |
| Logic | `and or not` |
| Join strings | `+` (**Open:** a separate `++` to avoid mixing with math) |
| Ranges | `0..10` for counting loops, end not included (**Default**) |
| Membership | `x in list` |

## Control flow

- `if` / `else if` / `else`
- `while cond`
- `for item in list` and `for i in 0..10`. **Default:** `for` is in 1.0, since it replaces most C-style counting loops.
- `break`, `next` (**Default** over `continue`), `return`

## Functions

- **Default:** declared with `fun name(args)`. Body is the indented block.
- Functions are values and can be stored or passed.
- **Default:** closures are supported.
- **Default:** a short form for one-line functions: `fun double(x) = x * 2`.
- **Default:** a function with no `return` gives `none`.

## Modules

**Default (M7, built).** One file is one module.

```
use lib/mathx          # loads lib/mathx.relic, binds the name mathx
use ../shared/util     # paths can climb with ../
print(mathx.square(4))
```

- `use a/b/name` loads `a/b/name.relic`, relative to the folder of the file that holds the `use`. Inside the REPL it is relative to the current directory.
- The last part of the path becomes the name in the current scope. There is no `as` rename yet (**Open**).
- A module runs once. Using it again, from anywhere, gives the same module.
- Every top-level `let` and `fun` in the module is reachable as `name.thing`. There is no private marker yet (**Open**: maybe a leading `_`).
- Names are read live. Two modules can use each other: the second `use` in a cycle gets the module that is still loading, and its names appear as they are defined. Calling across the cycle inside functions always works. Reading a name at the top level before the other file has defined it is an error: `module 'x' has no 'y' (yet)`.
- A module can use the main file back. It gets the main file's own scope; the main file does not run twice.
- Modules are values of type `module`. They print as `<module name>`. You cannot assign `mod.x = ...` from outside.
- Every module sees the built-ins, but not the main file's globals.
- Errors point at the file they happened in, so a mistake inside `lib/mathx.relic` reports `lib/mathx.relic:line:col`.
- `use` is now a keyword, so it can no longer be a variable name.

## Memory model

**Default (M7).** Reference counting, using the host's `std::shared_ptr` for now.

- Numbers, bools, `none` and strings are plain values and copy on assignment.
- Lists, maps, functions and modules are shared by reference. A value is freed the moment its last reference goes away, so freeing is predictable and there are no pauses.
- Known gap: cycles leak. A list that holds itself, or a closure stored in the scope it captures, is never freed. That is fine for scripts and the toy programs of phase 2.
- Why this and not a tracing GC: it is what the C++ host does for free, it is easy to reason about, and it keeps the runtime small while the language settles. Arenas stay an option for the VM.
- Plan: when Relic hosts itself (M9) and gets a VM (M10), Relic implements its own reference counts instead of borrowing `shared_ptr`, and adds a cycle collector that runs only over containers. That keeps the domestic goal: nothing borrowed in the end.
- **Open:** whether the self-hosted runtime switches to a full tracing collector instead. Decide at M10 with benchmarks.

## Errors

**Default:** runtime errors stop the program with a message and location. **Open:** catchable errors.

## Lexer notes (M2)

- Inside `( )`, `[ ]` and `{ }`, newlines do not end a line, so long calls and literals can wrap.
- An interpolated string becomes `STRING_PART`, `INTERP_START`, the inner tokens, `INTERP_END`, and a final `STRING_END`. A plain string is one `STRING` token.
- `!` on its own is an error that points to `not`.
- A number followed right away by a letter is an error.

## Runtime rules (M4)

These are **Default** behaviors the interpreter now follows.

- Conditions and `and` / `or` / `not` need real bools. `if 1` is an error, so there is no truthiness to learn.
- `and` and `or` stop early: `false and x` never looks at `x`.
- `+` adds numbers, joins strings and joins lists. Mixing types is an error, so use `str(x)` or interpolation.
- `%` takes the sign of the right side: `-7 % 3` is `2`.
- Whole numbers print without a decimal point: `7 / 2` prints `3.5`, `6 / 2` prints `3`.
- `a..b` makes a list from `a` up to `b - 1`. Both ends must be whole numbers.
- Lists and maps are shared, not copied: `let ys = xs` points at the same list.
- `for` loops over a copy of the list, so changing the list inside the loop is safe. It also loops over the characters of a string and the keys of a map, in insertion order.
- Maps keep insertion order. Reading a missing key is an error; `"k" in map` checks first.
- List indexes start at 0 and must be in range. Negative indexes are an error.
- `let` inside a block makes a new name for that block and hides any outer one. Plain `x = ...` changes the nearest existing `x`.
- A function with no `return` gives `none`. `return`, `break` and `next` in the wrong place are errors.
- Calls deeper than 800 stop with "too much recursion".
- Output printed before a runtime error is kept.

### Built-ins

| Name | Does |
|---|---|
| `print(a, b, ...)` | writes values separated by spaces, then a newline |
| `len(x)` | length of a string, list or map |
| `append(list, x)` | adds to the end of a list |
| `str(x)` | turns a value into text |
| `keys(map)` | list of keys in order |
| `type(x)` | `"none"`, `"bool"`, `"number"`, `"string"`, `"list"`, `"map"`, `"function"` or `"module"` |

### Primitives added in M8

| Name | Does |
|---|---|
| `slice(x, a, b)` | part of a string or list from `a` up to `b - 1`; ends are clamped |
| `ord(c)` / `chr(n)` | character code (0 to 255) and back |
| `num(s)` | parses a number from a string; error if it is not one |
| `pop(list)` | removes and gives back the last item |
| `insert(list, i, x)` | puts `x` at index `i` |
| `remove(list, i)` / `remove(map, k)` | drops an item or key and gives back its value |
| `host_*` | files, clocks and float math borrowed from C++. Call them through `std/` instead |

## Standard library (M8)

**Default.** The standard library is Relic code in `std/`, loaded with `use std/name`. Only the pieces Relic cannot do itself come from the host, and those are named `host_*` so the borrowed list stays visible.

- `use std/x` looks in `$RELIC_STD` if set, otherwise the `std/` folder of the source tree the host was built from.
- `std/strings`: `find`, `contains`, `starts_with`, `ends_with`, `upper`, `lower`, `trim`, `split`, `join`, `replace`, `repeat`. ASCII only for now.
- `std/lists`: `map`, `filter`, `reduce`, `sum`, `index_of`, `reverse`, `copy`, `sort` (stable merge sort), `sort_by(xs, less)`.
- `std/maps`: `values`, `items`, `get(m, k, fallback)`, `merge(a, b)`.
- `std/files`: `read`, `write`, `append_text`, `exists`, `lines`. Paths are relative to where `relic` runs. Host: `host_read_file`, `host_write_file`, `host_file_exists`.
- `std/time`: `now()` (Unix seconds), `clock()` (steady seconds), `since(start)`. Host: `host_now`, `host_clock`.
- `std/math`: `pi`, `e`, `abs`, `min`, `max`, `clamp`, `floor`, `ceil`, `round`, `sqrt`, `pow`, `sin`, `cos`, `exp`, `log`, `is_whole`. Host: floor, sqrt, sin, cos, exp, log, pow.
- **Open:** UTF-8 aware strings, directory listing, deleting files, dates and time zones, random numbers.

## Self-host (M9)

Relic code that rebuilds the host, one stage at a time, lives in `selfhost/`.

- `selfhost/lexer.relic`: the lexer. `lex(src)` gives back `{tokens, err}`. Relic has no catchable errors yet, so the first error is stored and lexing stops.
- `selfhost/tokens.relic`: prints the same dump as `--tokens`. Run it with `relic selfhost/tokens.relic FILE`.
- `selfhost/parser.relic`: the parser. `parse(tokens)` gives back `{tree, err}`; nodes are maps `{kind, text, line, col, kids}`. After the first error every step sees an `ERR` token, so the parse winds down without error checks at each call.
- `selfhost/ast.relic`: prints the same tree as `--ast`. `selfhost/tokens_fmt.relic` holds the shared string quoting.
- `selfhost/interp.relic`: the tree-walk interpreter, with the 1.0 built-ins and the M8 primitives. Functions are maps tagged with the key `chr(1)`. `selfhost/run.relic FILE` runs a program like `relic FILE`.
- The `selfhost_interp` test runs 75 golden programs on both and needs identical stdout and stderr.
- Limits of the self-hosted interpreter today:
  - No `use`, so no modules and no `std/`. `host_*` built-ins are not exposed either.
  - Every Relic call inside it costs several host calls, so the host's 800-call limit stops deep recursion (around 100 levels) before its own 800 limit. `rt_recursion` is skipped for that reason.
  - `args()` gives an empty list. `num()` on strings takes digits, a sign and a fraction only (no exponents).
  - It is far slower than the host. Speed is M10's job.
- The `selfhost_parser` test does the same comparison against `--ast`.
- The `selfhost_lexer` test runs both lexers over every `.relic` file in the repo and needs identical stdout and stderr.
- New built-ins for scripts: `args()` (words after the file name) and `eprint(...)` (print to stderr).
- Watch out: a lone `{` or `}` in a string literal starts or ends interpolation. Write `"{{"` and `"}}"` for literal braces.

## Architecture

1. **Lexer**: source text to tokens. Emits `INDENT` and `DEDENT` tokens from leading spaces, so the parser sees blocks like it would see braces.
2. **Parser**: tokens to an AST.
3. **Interpreter**: walks the AST directly.
4. **Later**: compile the AST to bytecode for a VM.
5. **Much later**: native code generation.

The REPL and the file runner share the same pipeline. In the REPL, a line that opens a block waits for an empty line before running. Layout is in [ARCHITECTURE.md](ARCHITECTURE.md).

## Syntax freeze

**Frozen for 1.0 (M3, Oct 2026).** Every **Default** in this file is now the 1.0 syntax, and the parser follows it. Items still marked **Open** stay out of 1.0 unless Daniel picks them. Changing a frozen item means updating this file, the parser and the golden tests together.

## Parser notes (M3)

Precedence, lowest first:

| Level | Operators |
|---|---|
| 1 | `or` |
| 2 | `and` |
| 3 | `not` |
| 4 | `== != < <= > >= in` (no chaining: `a < b < c` is an error) |
| 5 | `..` |
| 6 | `+ -` |
| 7 | `* / %` |
| 8 | unary `-` |
| 9 | calls `f(x)`, index `a[i]`, field `a.b` |

- Math operators group left: `1 - 2 - 3` is `(1 - 2) - 3`.
- `else if` chains are stored as an `If` inside the `else` slot.
- Lists, maps and call arguments allow a trailing comma.
- Map keys are names or strings, and a key can appear only once per literal.
- Only a name, an index or a field can be assigned to.
- `return` is accepted anywhere by the parser. Using it outside a function becomes a runtime error in M4.
- `relic --ast file.relic` prints the tree.

## Grammar

No formal grammar yet. One will be written once this syntax settles, marked draft until then.
