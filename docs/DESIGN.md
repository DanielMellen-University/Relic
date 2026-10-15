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

## Architecture

1. **Lexer**: source text to tokens. Emits `INDENT` and `DEDENT` tokens from leading spaces, so the parser sees blocks like it would see braces.
2. **Parser**: tokens to an AST.
3. **Interpreter**: walks the AST directly.
4. **Later**: compile the AST to bytecode for a VM.
5. **Much later**: native code generation.

The REPL and the file runner share the same pipeline. In the REPL, a line that opens a block waits for an empty line before running. Layout is in [ARCHITECTURE.md](ARCHITECTURE.md).

## Syntax freeze

The 1.0 syntax must be locked before parser work starts (milestone M3). Until then, everything in this file can change.

## Grammar

No formal grammar yet. One will be written once this syntax settles, marked draft until then.
