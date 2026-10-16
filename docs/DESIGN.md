# Design

The language as it works today, plus open questions. **Default** marks a choice made to keep moving; it is built and tested but can still change. **Open** marks a question that needs Daniel's call.

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

- Numbers: two types (**Default**, since 1.0). `int` is 64-bit, `float` is 64-bit floating point. See [Numbers](#numbers).
- Strings: UTF-8, immutable, in double quotes. **Default:** `{name}` inside a string inserts a value. `{{` writes a literal brace. Length, indexes, slices and loops count characters. See [Strings and UTF-8](#strings-and-utf-8).
- Bools: `true`, `false`.
- Nothing: `none`. **Default** over `nil` or `null`, to read as a plain word.
- Lists: `[1, 2, 3]`, growable, any value. Index from 0.
- Maps: `{key: value}`. Keys are names or strings. Read with `box.title` or `box["title"]`. **Default:** maps are in 1.0 since the literal is part of the language's look.

## Typing

**Default:** dynamic typing, with optional `as` hints checked while the program runs.

```
let count as int = 0
fun area(w as number, h as number) as number
    return w * h
fun greet(name as string) as string = "hello " + name
```

- Hints go on `let`, on parameters and after the `)` of a `fun` for the return value.
- Types: `any`, `none`, `bool`, `int`, `float`, `number` (int or float), `string`, `list`, `map`, `function`, `module`. Any other name is a syntax error.
- `float` means float only. Use `number` to take both.
- Checked when the name gets a value: at `let`, at every later `x = ...`, when a function is called, and when it returns (falling off the end returns `none`).
- Errors: `'count' must be int, got string`, `area() argument 'w' must be number, got string`, `greet() must return string, got int`.
- A new `let` of the same name without a hint drops the hint.
- No checking before the program runs. Hints are notes the runtime enforces, not a static type system.

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
- The last part of the path becomes the name in the current scope. `use lib/mathx as m` binds it as `m` instead.
- A module runs once. Using it again, from anywhere, gives the same module.
- Every top-level `let` and `fun` in the module is reachable as `name.thing`, except private names.
- Private names: a top-level name that starts with `_` stays inside its file. `mathx._cache` from another file is an error: `'_cache' is private to module 'mathx'`. Inside the file, `_cache` works as usual. Map keys that start with `_` are normal keys.
- Names are read live. Two modules can use each other: the second `use` in a cycle gets the module that is still loading, and its names appear as they are defined. Calling across the cycle inside functions always works. Reading a name at the top level before the other file has defined it is an error: `module 'x' has no 'y' (yet)`.
- A module can use the main file back. It gets the main file's own scope; the main file does not run twice.
- Modules are values of type `module`. They print as `<module name>`. You cannot assign `mod.x = ...` from outside.
- Every module sees the built-ins, but not the main file's globals.
- Errors point at the file they happened in, so a mistake inside `lib/mathx.relic` reports `lib/mathx.relic:line:col`.
- `use` is now a keyword, so it can no longer be a variable name. The same goes for `try`, `catch` and `as`.

## Memory model

**Default (final).** Reference counting plus a cycle collector.

- Numbers, bools, `none` and strings are plain values and copy on assignment.
- Lists, maps, functions and modules are shared by reference. A value is freed the moment its last reference goes away, so most freeing is immediate and has no pauses.
- Cycles: a list that holds itself, or a closure stored in the scope it captures, is found by the cycle collector (`src/interp/gc.cpp`) and freed.
- How it works: every list, map, function and scope is on one list. A collection counts the references each one gets from other containers. If its total count is higher, something outside (a local in the host, the VM stack, a module) holds it, so it is a root. Everything reachable from a root lives; the rest gets cleared, and the reference counts free it.
- When: at function calls and loop turns, once more than 100,000 containers exist and the count has doubled since the last collection. `gc()` runs one now and gives the number freed.
- Same collector for the tree-walker and the VM. A loop making millions of cycles stays at a flat memory size.
- Cost: about 10 percent on `bench/`, from tracking each container.
- Still borrowed: the counts themselves come from the host's `std::shared_ptr`. A Relic-written runtime would own them.

## Errors

**Default:** a runtime error stops the program with a message and location, unless a `try` catches it.

```
try
    let n = num(text)
    print(n * 2)
catch err
    print("not a number: {err.message}")
```

- `try` and `catch` are keywords. Both open an indented block, like `if` and `else`. A `try` needs a `catch`.
- `catch err` binds the error as a map `{message, line, col}`. `catch` with no name is allowed.
- `fail("message")` raises an error with that message. Uncaught, it prints like any other error: `file:line:col: error: message`.
- Every runtime error can be caught, including errors from built-ins, `use` and "too much recursion". Syntax errors in the main file cannot, since nothing runs.
- `break`, `next` and `return` inside a `try` leave it normally.
- Changes made before the error stay made.

## Numbers

**Default (since 1.0).** Two number types.

- `3` is an `int`, `3.0` and `0.5` are `float`.
- Exponents: `1e3`, `2.5e-4`, `6E+2` are always `float`. A literal past the float range is an error (`float is too large`).
- `+ - * %` on two ints give an int. Anything with a float gives a float.
- `/` always gives a float: `7 / 2` is `3.5`, `6 / 2` is `3.0`. Use `int(x)` to cut toward zero.
- Ints are 64-bit. Going past the range is an error (`int overflow`), not a wrap.
- `1 == 1.0` is `true`. Comparisons mix freely.
- Floats always print with a point: `3.0`, `0.5`, `1e+20`. Ints never do.
- Indexes, `slice`, `chr` and ranges take ints, or floats with no fraction.
- `len`, `ord`, ranges, `num("12")`, `floor`, `ceil` and `round` give ints. `sqrt`, `pow` and the other float math give floats.
- `type()` says `"int"` or `"float"`. Older code that checked `type(x) == "number"` needs both names, or a `number` hint.

## Strings and UTF-8

**Default (since 1.0).** Strings hold UTF-8 bytes, and every string operation counts characters (code points).

- `len("héllo")` is `5`. `s[1]` is `"é"`. `slice` and `for c in s` work by character.
- `ord("€")` is `8364`, `chr(128512)` is `"😀"`.
- `\u{hex}` writes a character by code point: `"\u{e9}"` is `"é"`. 1 to 6 hex digits, no surrogates.
- Error columns count characters too.
- A byte that is not valid UTF-8 counts as one character, so binary files still load.
- Comparing strings compares code points in order.
- `std/strings` `upper` and `lower` only change ASCII letters.
- Names in code are still ASCII. Other characters go in strings and comments.

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
- `a..b` makes a list of ints from `a` up to `b - 1`. Both ends must be whole numbers.
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
| `type(x)` | `"none"`, `"bool"`, `"int"`, `"float"`, `"string"`, `"list"`, `"map"`, `"function"` or `"module"` |
| `fail(message)` | raises an error that `try` / `catch` can catch |
| `int(x)` / `float(x)` | a number or a numeric string as an int (cut toward zero) or a float |
| `gc()` | runs the cycle collector now; gives how many containers it freed |

### Primitives added in M8

| Name | Does |
|---|---|
| `slice(x, a, b)` | part of a string or list from `a` up to `b - 1`; ends are clamped |
| `ord(c)` / `chr(n)` | code point of a one-character string and back (0 to 1114111, no surrogates) |
| `num(s)` | parses a number from a string: digits give an int, a point or exponent gives a float |
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
- **Open:** directory listing, deleting files, dates and time zones, random numbers, Unicode-aware `upper` and `lower`.

## Self-host (M9)

Relic code that rebuilds the host, one stage at a time, lives in `selfhost/`.

- `selfhost/lexer.relic`: the lexer. `lex(src)` gives back `{tokens, err}`. The first error is stored and lexing stops (it was written before `try` existed).
- `selfhost/tokens.relic`: prints the same dump as `--tokens`. Run it with `relic selfhost/tokens.relic FILE`.
- `selfhost/parser.relic`: the parser. `parse(tokens)` gives back `{tree, err}`; nodes are maps `{kind, text, line, col, kids}`. After the first error every step sees an `ERR` token, so the parse winds down without error checks at each call.
- `selfhost/ast.relic`: prints the same tree as `--ast`. `selfhost/tokens_fmt.relic` holds the shared string quoting.
- `selfhost/interp.relic`: the tree-walk interpreter, with the 1.0 built-ins and the M8 primitives. Functions are maps tagged with the key `chr(1)`. `selfhost/run.relic FILE` runs a program like `relic FILE`.
- Modules are maps tagged `"module"` under the same key, cached by normalized path.
- It knows `try` / `catch`, `fail`, `as` hints, int and float, and UTF-8 strings. Number math and `int()` / `float()` run on the host inside a `try`, so an int overflow becomes the program's own error.
- The `selfhost_interp` test runs 101 golden programs on both and needs identical stdout and stderr.
- Limits of the self-hosted interpreter today:
  - `use` works for your own files (with `as`, cycles, private `_` names), but not `use std/...`: `host_*` built-ins are not exposed.
  - Every Relic call inside it costs several host calls, so the host's 800-call limit stops deep recursion (around 100 levels) before its own 800 limit. `rt_recursion` and `try_recursion` are skipped for that reason.
  - No `gc()`: the host collector already covers its maps. `gc_cycles` is skipped.
  - `args()` gives an empty list. `num()` on strings takes digits, a sign, a fraction and an exponent (no inf, nan or hex).
  - It is far slower than the host. Speed is M10's job.
- The `selfhost_parser` test does the same comparison against `--ast`.
- The `selfhost_lexer` test runs both lexers over every `.relic` file in the repo and needs identical stdout and stderr.
- New built-ins for scripts: `args()` (words after the file name) and `eprint(...)` (print to stderr).
- Watch out: a lone `{` or `}` in a string literal starts or ends interpolation. Write `"{{"` and `"}}"` for literal braces.

## Bytecode VM (M10)

`relic FILE` compiles the tree to bytecode and runs it on a stack machine (`src/vm/`); this is the default engine since M10. `relic --tree FILE` runs the old tree-walker (`--vm` is kept as an alias for the default). `relic --bytecode FILE` prints the compiled code.

- One flat instruction list per function. Jumps for `if`, `while`, `for`, `and`, `or`, `break` and `next`.
- Names resolve at compile time to a scope slot (hops up the chain, then slot) instead of a name lookup at run time. Top-level names are numbered globals; names not found anywhere fall back to the built-ins.
- Only blocks that declare names get a scope at run time. Each `for` turn gets its own scope, so closures made in a loop keep their own loop value, same as the tree-walker.
- Operators, indexing, fields, loops over values and built-in calls share code with the tree-walker (`src/interp/ops.hpp`), so results and error messages match. Number math has a fast path.
- Modules: each file compiles to its own program with its own numbered globals. `use` loads a file once, caches it by resolved path before running it (so cycles work), and binds a module value whose fields are read live from that file's globals. Errors name the file they happened in, like the tree-walker.
- The 800-call limit is the same. Each Relic call is one C++ call of the VM loop, so it is cheap on the host stack.
- Differences from the tree-walker today:
  - The REPL still uses the tree-walker.
  - A function that reads a name its enclosing function declares later in the same block resolves to a global or built-in instead. The tree-walker finds the later name at call time. No golden test does this.
  - Assigning to a built-in's name without `let` (`print = 1`) is an error on the VM.
- Benchmarks live in `bench/`; `tools/bench.sh build/relic` times both engines and checks they print the same.

## Graphics (M11)

**Default:** SDL2 is the graphics backend, the same as MONOLITH 1. It is a borrowed piece, listed like the C++ host, to be replaced once Relic has its own drawing layer.

- Optional at build time: `cmake -S . -B build-sdl -DRELIC_SDL=ON` (needs SDL2 dev files, e.g. `apt install libsdl2-dev`, `brew install sdl2`, or vcpkg `sdl2` on Windows). The default build and CI have no SDL.
- `use std/graphics` gives `available()`, `open(w, h, title)`, `close()`, `clear(r, g, b)`, `rect(x, y, w, h, r, g, b)`, `pixel(x, y, r, g, b)`, `present()`, `quit_requested()` and `wait(ms)`. One window at a time; colors are 0 to 255.
- Input: `key_down(name)`, `next_key()` (next pressed key or `none`), `mouse_x()`, `mouse_y()`, `mouse_down(button)` (1 left, 2 middle, 3 right). Key names are lowercase SDL names (`"a"`, `"space"`, `"left"`, `"escape"`).
- Text: `text(x, y, s, r, g, b)` and `text_scaled(x, y, s, scale, r, g, b)` in a built-in 8x12 ASCII font (generated once from DejaVu Sans Mono by `tools/gen_font.py`; no SDL_ttf).
- Images: `load_image(path)` gives an id; `image(id, x, y)`, `image_width(id)`, `image_height(id)`. BMP only for now (no SDL_image).
- Without SDL, `available()` is `false` and every other call stops with a message saying how to rebuild.
- Host side: `host_gfx_*` built-ins in `src/builtins/graphics.cpp`.
- Try it: `build-sdl/relic examples/hello_window.relic` opens a window for about two seconds.
- **Open:** PNG images, TrueType fonts, more than one window.

## Architecture

1. **Lexer**: source text to tokens. Emits `INDENT` and `DEDENT` tokens from leading spaces, so the parser sees blocks like it would see braces.
2. **Parser**: tokens to an AST.
3. **Tree-walk interpreter**: walks the AST directly. Used by the REPL and `--tree`.
4. **Bytecode VM** (M10, the default; `--tree` for the tree-walker): compiles the AST to bytecode and runs it on a stack machine.
5. **Much later**: native code generation.

The REPL and the file runner share the same pipeline. In the REPL, a line that opens a block waits for an empty line before running. The REPL still runs on the tree-walker. Layout is in [ARCHITECTURE.md](ARCHITECTURE.md).

## Syntax freeze

**Frozen for 1.0 (M3, Oct 2026).** Every **Default** in this file is now the 1.0 syntax, and the parser follows it. Items still marked **Open** stay out of 1.0 unless Daniel picks them. Changing a frozen item means updating this file, the parser and the golden tests together.

Added after 1.0, approved by Daniel: `try` / `catch` and `fail`, `as` hints and `use ... as`, int and float, private `_` names, UTF-8 strings with `\u{hex}`, and the cycle collector. With these the open design list is empty.

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
