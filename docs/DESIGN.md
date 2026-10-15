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

**Open.** Planned for phase 4. Proposal: one file is one module, loaded with `use path/name`.

## Memory model

**Open.** Options: reference counting, a tracing garbage collector, or arenas. Phase 1 relies on the host. Decide in M7.

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
| `type(x)` | `"none"`, `"bool"`, `"number"`, `"string"`, `"list"`, `"map"` or `"function"` |

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
