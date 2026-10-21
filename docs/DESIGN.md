# Design

Why Relic looks and behaves the way it does, and what is still undecided. The rules themselves are in the [language reference](LANGUAGE.md). How the host implements them is in [ARCHITECTURE.md](ARCHITECTURE.md).

Two tags are used across the docs:

- **Default**: a choice made to keep moving. It is built and tested, but can still change.
- **Open**: a question that needs Daniel's call before work on it starts.

## Voice

Relic should not read like C, C++ or Java. The aim is short lines, few symbols, and words where other languages use punctuation.

- **Default:** blocks come from indentation, not braces, and there is no `end`.
- **Default:** no semicolons, no colons after block lines, no parentheses around conditions.
- **Default:** logic uses words: `and`, `or`, `not`.
- **Default:** `none` instead of `nil` or `null`, and `next` instead of `continue`, to read as plain words.
- **Default:** comments start with `#`.
- **Default:** 4-space indents, tabs are an error, so files look the same everywhere.

## Choices and the reasons for them

| Choice | Why |
|---|---|
| Conditions must be real bools (no truthiness) | one less thing to learn; `if 1` is almost always a mistake |
| `+` never mixes types | `"a" + 1` hides bugs; use `str(x)` or `{x}` |
| `/` always gives a float, `int` errors on overflow | no silent integer division, no silent wrap |
| Dynamic typing with optional `as` hints checked at run time | hints document intent without a static checker |
| `let` to declare, plain `=` to change | a typo in a name is an error, not a new variable |
| Maps keep insertion order | `keys`, `for` and printing are predictable, and golden tests stay stable |
| One file is one module, `_` names are private | no export lists to keep in sync |
| Module names are read live, cycles allowed | apps whose parts call each other just work |
| Reference counting plus a cycle collector | freeing is immediate and pause-free in the common case; cycles still get cleaned |
| Errors as `file:line:col: error: message` plus the source line | readable by people and by tools |
| Standard library written in Relic, with borrowed pieces named `host_*` | the borrowed list stays visible and small |
| SDL2 for graphics, optional at build time | same backend as MONOLITH 1; the default build needs nothing but a C++ compiler |
| Bytecode VM as the default engine, tree-walker kept | speed, with a second engine to compare against on every test |
| Bootstrap through a Relic compiler that emits C (Daniel, Oct 2026) | C compilers are everywhere, and the result has no C++ left; the C++ host is stage 0 until the compiler compiles itself |

## Syntax freeze

**Frozen for 1.0 (M3, Oct 2026).** Every **Default** for the syntax became the 1.0 syntax, and the parser follows it. Changing a frozen item means updating [LANGUAGE.md](LANGUAGE.md), the parser and the golden tests together.

Added after 1.0, approved by Daniel: `try` / `catch` and `fail`, `as` hints and `use ... as`, separate `int` and `float`, private `_` names, UTF-8 strings with `\u{hex}`, and the cycle collector.

## Open questions

Out of the language unless Daniel picks them:

| Question | Notes |
|---|---|
| File extension `.rl` instead of `.relic` | `.relic` is the default |
| A trailing colon on block lines, Python style | left out for now |
| A `const` form for names that never change | |
| A separate `++` for joining strings | `+` joins today |
| Dates and time zones, random numbers | not in `std/` yet |
| Unicode-aware `upper` and `lower` | ASCII only today |
| More than one graphics window | one at a time today |
| A formal grammar | to be written once the syntax settles; draft until then |
