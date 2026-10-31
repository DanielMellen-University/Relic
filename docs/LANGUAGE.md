# Language reference

The rules of Relic as the host runs them today (version 1.0.45). The [guide](GUIDE.md) teaches the same things with examples. Why things are this way, and what is still undecided, is in [DESIGN.md](DESIGN.md).

## Contents

- [Source files](#source-files)
- [Values and types](#values-and-types)
- [Names and scope](#names-and-scope)
- [Operators](#operators)
- [Control flow](#control-flow)
- [Functions](#functions)
- [Type hints](#type-hints)
- [Modules](#modules)
- [Errors](#errors)
- [Numbers](#numbers)
- [Strings](#strings)
- [Memory](#memory)
- [Built-in functions](#built-in-functions)
- [Limits](#limits)

## Source files

- Files end in `.relic` and are UTF-8.
- Comments start with `#` and run to the end of the line.
- A line that opens a block (`fun`, `if`, `else`, `while`, `for`, `try`, `catch`) has no colon. The lines under it, indented one more level, are the block. There is no `end`.
- Indent with 4 spaces. Tabs are an error.
- No semicolons. Inside `( )`, `[ ]` and `{ }` a newline does not end the line, so long calls and literals can wrap.
- Lists, maps and call arguments allow a trailing comma.
- Names are ASCII letters, digits and `_`, not starting with a digit. A number followed right away by a letter is an error.
- `!` on its own is an error that points you to `not`.
- Keywords: `let fun return if else while for in break next and or not true false none use as try catch`.

## Values and types

| Type | Written as | Notes |
|---|---|---|
| `none` | `none` | the "nothing" value |
| `bool` | `true`, `false` | |
| `int` | `3`, `-12` | 64-bit; overflow is an error |
| `float` | `3.5`, `1e3`, `2.5e-4` | 64-bit floating point |
| `string` | `"hi {name}"` | UTF-8, immutable |
| `list` | `[1, "a", none]` | growable, any values, index from 0 |
| `map` | `{title: "x", "two words": 2}` | keys are names or strings; keeps insertion order |
| `function` | `fun ...` | includes built-ins |
| `module` | `use lib/x` | prints as `<module x>` |

- `type(x)` gives the type name as a string.
- `none`, bools, numbers and strings are copied on assignment. Lists, maps, functions and modules are shared: `let ys = xs` then `append(ys, 1)` changes `xs` too. `lists.copy(xs)` makes a shallow copy.
- A map key can appear only once in a literal.

## Names and scope

- `let x = 1` declares a name in the current block. `x = 2` changes the nearest existing `x`. Assigning to a name that was never declared is an error.
- Built-ins are not declared names: `print = 1` is an error (`'print' is not declared; use 'let print = ...'`). `let print = 1` makes a new name that hides the built-in in that scope.
- A `let` inside a block makes a new name for that block and hides an outer one.
- A name exists only after its `let` or `fun` line has run. Calling a function at the top level above its `fun` is an error (`'greet' is not defined`). Inside another function it is fine, because the later `fun` has run by the time the call happens. `relic check` warns about the top-level case.
- Closures capture their scope. A function made inside a block still sees that block's names after it ends. A nested function that uses a name its enclosing function declares further down sees the outer name until that `let` has run, and the local after. Each `for` turn gets its own scope, so closures made in a loop keep their own loop value.
- Only a name, an index (`a[i]`) or a field (`a.b`) can be assigned to. Field writes reach the map through any path: `wins[i].x = 1` changes the map inside the list.

## Operators

Lowest precedence first:

| Level | Operators | Notes |
|---|---|---|
| 1 | `or` | stops early; both sides must be bools |
| 2 | `and` | stops early; both sides must be bools |
| 3 | `not` | needs a bool |
| 4 | `== != < <= > >= in` | no chaining: `a < b < c` is an error |
| 5 | `..` | `a..b` is the ints from `a` up to `b - 1` |
| 6 | `+ -` | |
| 7 | `* / %` | |
| 8 | unary `-` | |
| 9 | `f(x)`, `a[i]`, `a.b` | call, index, field |

- Math groups left: `1 - 2 - 3` is `(1 - 2) - 3`.
- `+` adds numbers, joins two strings, or joins two lists. Mixing types is an error (`cannot add a string and an int`).
- `/` always gives a float. `%` takes the sign of the right side: `-7 % 3` is `2`.
- `==` and `!=` compare lists and maps by contents, strings by contents, and functions by identity. `none` equals only `none`. `1 == 1.0` is `true`.
- `<` and friends compare numbers with numbers and strings with strings (by code point).
- `x in list` checks for an item, `"k" in map` for a key, `"ell" in "hello"` for a substring.
- Range ends must be whole numbers.

## Control flow

```
if a
    ...
else if b
    ...
else
    ...

while cond
    ...

for item in xs        # a list, a string (by character), a map (by key) or a range
    ...
```

- Conditions must be real bools: `if 1` is an error.
- There is no one-line `if` and no `x if c else y`.
- `for` loops over a copy of the list, so changing the list inside the loop is safe.
- `break` leaves the loop, `next` goes to the next turn. `break`, `next` or `return` in the wrong place is an error.

## Functions

```
fun area(w, h)
    return w * h

fun double(x) = x * 2
```

- Functions are values. They can be stored in lists and maps, passed and returned.
- A function with no `return`, or a bare `return`, gives `none`.
- Calling with the wrong number of arguments is an error.
- Calls more than 800 deep stop with `too much recursion (more than 800 calls deep)`. `relic --tree` walks the tree on the native stack, so a very big expression inside deep recursion can stop it sooner, with `too much recursion (calls and expressions nest too deeply for the stack)`; it uses up to three quarters of the stack (6 MB of the usual 8 MB). Both errors can be caught with `try`.

## Type hints

Optional, and checked while the program runs. There is no static type check.

```
let count as int = 0
fun area(w as number, h as number) as number
    return w * h
fun greet(name as string) as string = "hello " + name
```

- Hints go on `let`, on parameters, and after the `)` of a `fun` for the return value.
- Hint types: `any`, `none`, `bool`, `int`, `float`, `number` (int or float), `string`, `list`, `map`, `function`, `module`. Any other name is a syntax error.
- `float` means float only. Use `number` to take both.
- A hinted name is checked at `let`, at every later `x = ...`, at each call, and at return (falling off the end returns `none`).
- Messages: `'count' must be int, got string`, `area() argument 'w' must be number, got string`, `greet() must return string, got int`.
- A new `let` of the same name without a hint drops the hint.
- The hint that counts is the one from the latest `let` that ran, also for an assignment from a nested function that runs after the name was declared again.

## Modules

```
use lib/mathx          # loads lib/mathx.relic, binds the name mathx
use lib/mathx as m     # same module, bound as m
use ../shared/util     # paths can climb with ../
use std/strings        # standard library
print(mathx.square(4))
```

- One file is one module. `use a/b/name` loads `a/b/name.relic`, relative to the folder of the file that holds the `use`. In the REPL it is relative to the current folder.
- `use std/x` loads from the standard library folder (see [CLI.md](CLI.md#environment)).
- `use` binds the name in the current scope only. A `use` inside a function is not visible outside it.
- A module runs once. Every later `use` of the same file, from anywhere, gives the same module.
- Every top-level `let` and `fun` in the module is reachable as `mod.name`, except names starting with `_`, which are private to the file (`'_cache' is private to module 'mathx'`). Map keys starting with `_` are ordinary keys.
- Module names are read live. Two modules can use each other: the second `use` in a cycle gets the module that is still loading. Calls across the cycle inside functions always work. Reading a name at the top level before the other file defines it is an error (`module 'x' has no 'y' (yet)`).
- A module can use the main file back. It gets the main file's own scope, and the main file does not run twice.
- Modules see the built-ins, but not the main file's globals.
- `mod.x = ...` from outside is an error. Give the module a function that does it.
- Modules are values, so you can pick one and call through it:

```
let mod = notes
if kind == "terminal"
    mod = terminal
mod.on_key(app, key)
```

## Errors

```
try
    let n = num(text)
    print(n * 2)
catch err
    print("not a number: {err.message}")
```

- Every runtime error can be caught: from your code, from built-ins, from `use`, and "too much recursion". Syntax errors in the main file cannot, since nothing runs.
- `catch err` binds a map `{message, line, col}`. `catch` with no name is allowed. A `try` needs a `catch`.
- `fail("message")` raises an error. The message must be a string.
- `break`, `next` and `return` inside a `try` leave it normally. Changes made before the error stay made.
- An error inside a std module is reported at your call, under the std name: `files.list(): cannot list x`.

An uncaught error prints like this and exits with code 1:

```
area.relic:2:20: error: 'hieght' is not defined (did you mean 'height'?)
        return width * hieght
                       ^
```

- The first line is always `file:line:col: error: message`, so tools can match it. The file is the one the error happened in, which may be a module.
- Under it come the source line and a caret under the column. Long lines are cut around the caret. The REPL leaves these two lines out.
- Unknown names and missing map keys get a "did you mean" hint when a name one or two typos away exists.
- Output printed before the error is kept.

## Numbers

- `3` is an `int`. `3.0`, `0.5` and anything with an exponent (`1e3`, `6E+2`) is a `float`.
- `+ - * %` on two ints give an int. Anything with a float gives a float. `/` always gives a float: `6 / 2` is `3.0`.
- Ints are 64-bit. Going past the range is `int overflow`, not a wrap. A float literal past the float range is `float is too large`. One too small for a float rounds to the nearest one, down to `0.0` (`1e-400` is `0.0`, `4.9e-324` the smallest float above zero); since 1.0.36 `num()` reads them the same way.
- Floats always print with a point or an exponent: `3.0`, `0.3`, `1e+20`. Ints never do.
- Indexes, `slice`, `chr` and range ends take ints, or floats with no fraction.
- `len`, `ord`, ranges and `num("12")` give ints. `std/math` `floor`, `ceil` and `round` give ints; `sqrt`, `pow` and the rest give floats.

## Strings

- Double quotes only. Escapes: `\n`, `\t`, `\"`, `\\`, and `\u{hex}` for a code point (1 to 6 hex digits, no surrogates).
- `{expr}` inserts a value. `{{` and `}}` write literal braces. A lone `{` starts an interpolation, so escape it when you mean a brace.
- Strings are UTF-8 and count characters (code points): `len("héllo")` is `5`, `s[1]` is `"é"`, and `slice` and `for c in s` go by character. Error columns count characters too.
- A byte that is not valid UTF-8 counts as one character, so binary files still load.
- A curly quote or a no-break space pasted from a document, a byte order mark at the start of a file, and a control character (shown as `U+0001`, since it prints as nothing), are `unexpected character` errors that say what the character is and what to type instead.
- Strings are immutable: `s[0] = "x"` is an error.
- `std/strings` `upper` and `lower` change ASCII letters only.
- Cost: `s + t` makes a new string, copying both. `x = x + t`, with the same name on both sides and nothing else holding the text in `x` (no other variable, list, map or closure got it since it was made), adds `t` to the end of that text instead, so a loop that builds a string one piece at a time this way takes time growing with its length, not its square (200,000 one-character pieces: 0.69 s on the VM in 1.0.42, 0.013 s since 1.0.43; the same in `--tree` and compiled programs). Any other way of adding, like `y = x + t` or `x = t + x`, copies; build such text from a list of pieces with `strings.join` (the standard library does since 1.0.37). Reading a string variable, passing a string or putting it in a list does not copy it, and `len(s)`, `s[i]` and `slice` on a long string are quick: the first `len` or index of a string counts its characters once (a 200,000-character string: about 0.02 ms in ASCII, 0.3 ms in é), and after that `len` is immediate and an index goes straight to its character, or near it when the string has characters past ASCII (since 1.0.41 on the host; compiled programs before that).

## Memory

- Values are reference counted and freed as soon as the last reference goes. Cycles (a list that holds itself, a closure stored in the scope it captured) are found by a cycle collector.
- The collector runs on its own at calls and loop turns once more than 100,000 containers exist and the count has doubled since the last run. `gc()` runs it now and gives the number freed.
- Very deep nesting is safe to free. A value that holds itself prints `[...]` or `{...}` where it repeats, and anything nested more than 1000 deep prints the same way.
- Details: [ARCHITECTURE.md](ARCHITECTURE.md#memory).

## Built-in functions

Always available, no `use` needed.

| Name | Does |
|---|---|
| `print(a, b, ...)` | writes the values with spaces between, then a newline |
| `eprint(a, b, ...)` | same, to stderr |
| `len(x)` | length of a string, list or map |
| `str(x)` | the value as text, the way `print` shows it |
| `type(x)` | `"none"`, `"bool"`, `"int"`, `"float"`, `"string"`, `"list"`, `"map"`, `"function"` or `"module"` |
| `int(x)` / `float(x)` | a number or numeric string as an int (cut toward zero) or a float |
| `num(s)` | a number from a string: digits give an int (a float when they do not fit), a point or exponent gives a float; also `nan`, `inf` and `infinity` in any case, and hex like `0x1F` (an int when it fits). A sign may come first. A float too small rounds to a subnormal or `0.0` like a literal; one too large, spaces or anything else is `not a number` (since 1.0.36; before, hex always gave a float and a tiny float was an error) |
| `ord(c)` / `chr(n)` | code point of a one-character string, and back (0 to 1114111, no surrogates) |
| `slice(x, a, b)` | part of a string or list from `a` up to `b - 1`; the ends are clamped |
| `append(list, x)` | adds `x` to the end |
| `insert(list, i, x)` | puts `x` at index `i`; the items on the shorter side of `i` move, so the front and the back are quick and the middle of a long list is not |
| `pop(list)` | removes and gives the last item |
| `remove(list, i)` / `remove(map, k)` | drops an item or key and gives its value. On a list the items on the shorter side of `i` move, so taking items off the front, as a queue does, is as quick as off the back (20,000 off a 100,000-item list: 0.002 s on the VM since 1.0.42, 3.4 s before). On a map it is quick, and the other keys keep their order |
| `keys(map)` | the keys, in order |
| `fail(message)` | raises an error; `message` must be a string |
| `args()` | the words after the file name on the command line |
| `gc()` | runs the cycle collector; gives how many containers it freed |

Names starting with `host_` are the host's own pieces (files, clocks, float math, graphics). The standard library wraps them, so call `std/` instead. They are listed in [HOST.md](HOST.md).

## Limits

- Calls: 800 deep.
- Source nesting (brackets, `-`, `not`, blocks): 32 levels (`too deeply nested (more than 32 levels)`), the same in every engine and in `relicc`. It was 100 before 1.0.16; see [CGEN.md](CGEN.md#decided-choices) for why.
- Expression depth: 10,000 levels (`expression is too deeply nested (limit 10000)`, since 1.0.36). Each operator, call, index, field, `-`, `not`, list, map and string with `{...}` is one level more than its deepest part, so `a + a + ...` with 10,000 `+` is fine and 10,001 is the error, pointing at the `+` that went over; `print(...)` around it counts one more. `f()()...` and `m.a.a...` count one per link. Before 1.0.36 there was no limit, and about 55,000 links in a row crashed the host (C stack). The same in every engine and in `relicc`.
- `else if` chains: 10,000 links (`'else if' chain is too long (limit 10000)`, pointing at the `if` that went over). A chain inside a branch of another chain counts that chain's links before it too. Nothing real comes near either limit; split the expression or use a map or a list.
- One graphics window at a time.
