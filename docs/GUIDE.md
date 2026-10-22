# Relic guide

A walk through the language, from the first line to modules and errors. Every example here runs as shown on `relic` 1.0.9. For the exact rules, see the [language reference](LANGUAGE.md).

You need a built `relic` first: the C++ host, built with `-DRELIC_BUILD_HOST=ON`. See [The C++ host](../README.md#the-c-host). The default build makes `relicc` instead, which compiles the same programs to C.

## 1. Hello

Save this as `hello.relic` and run `relic hello.relic`:

```
print("hello")
let name = "Relic"
let year = 2026
print("hello {name}, it is {year}")
print(1 + 2, 7 / 2, 7 % 3, int(7 / 2))
```

```
hello
hello Relic, it is 2026
3 3.5 1 3
```

- `print` takes any number of values and puts a space between them.
- `let` makes a new name. Later you change it with plain `name = ...`.
- `{...}` inside a string puts a value there. Write `{{` and `}}` for real braces.
- `/` always gives a float. `int(x)` cuts it back toward zero.
- Comments start with `#`.

## 2. Blocks and loops

Blocks are lines indented 4 spaces under the line that opens them. There are no braces, colons or `end`. Tabs are an error.

```
let n = 15
if n % 15 == 0
    print("FizzBuzz")
else if n % 3 == 0
    print("Fizz")
else
    print(n)

let i = 0
while i < 3
    i = i + 1
print(i)

for k in 0..3
    print(k)
```

```
FizzBuzz
3
0
1
2
```

- `0..3` is `0, 1, 2`. The end is left out.
- `break` leaves a loop, `next` skips to the next turn.
- Conditions must be `true` or `false`. `if 1` is an error, not "truthy".
- Logic is written in words: `and`, `or`, `not`.

## 3. Lists and maps

```
let xs = [3, 1, 2]
append(xs, 4)
print(xs, len(xs), xs[0])
let ys = xs
append(ys, 5)
print(xs)

let box = {title: "MONOLITH", version: 2}
box.version = 3
box["owner"] = "Daniel"
print(box)
print(keys(box), "title" in box)
for k in box
    print(k, box[k])
```

```
[3, 1, 2, 4] 4 3
[3, 1, 2, 4, 5]
{title: "MONOLITH", version: 3, owner: "Daniel"}
["title", "version", "owner"] true
title MONOLITH
version 3
owner Daniel
```

- Lists start at 0. Reading past the end is an error.
- Lists and maps are shared, not copied: `ys` and `xs` above are the same list. Use `lists.copy(xs)` for a copy.
- Maps keep the order keys were added. Reading a missing key is an error, so check with `"k" in m` first.
- `==` compares lists and maps by their contents.

## 4. Functions

```
fun add(a, b)
    return a + b
fun double(x) = x * 2

fun counter()
    let n = 0
    fun next_one()
        n = n + 1
        return n
    return next_one

let c = counter()
c()
print(add(2, 3), double(4), c())
```

```
5 8 2
```

- `fun name(args) = expr` is the one-line form.
- A function with no `return` gives `none`.
- Functions are values. They can be stored, passed and returned, and they keep the names around them (closures).
- A name exists only after its `fun` or `let` line has run. Put definitions above the top-level code that calls them.

## 5. Text and numbers

```
let s = "héllo"
print(len(s), s[1], slice(s, 1, 3), ord("€"), chr(65))
print(type(1), type(1.0), type("x"), type([]), type({}), type(none))
print(1 == 1.0, 1e3, num("12"), num("1.5"), str(42) + "!")
```

```
5 é él 8364 A
int float string list map none
true 1000.0 12 1.5 42!
```

- Strings are UTF-8 and count characters, not bytes.
- `int` is 64-bit and errors on overflow instead of wrapping. `float` is 64-bit floating point.
- `+` never mixes types: `"a" + 1` is an error. Use `str(x)` or `{x}` in a string.

## 6. Errors

```
fun half(n as int) as float = n / 2

try
    print(half(4))
    print(half("4"))
catch err
    print("caught: {err.message}")

try
    fail("disk is full")
catch err
    print(err.message, err.line)
```

```
2.0
caught: half() argument 'n' must be int, got string
disk is full 10
```

- `try` / `catch err` catches any runtime error. `err` is a map `{message, line, col}`.
- `fail("why")` raises your own error. It takes a string only.
- `as` hints are checked while the program runs. They are optional.

An error nobody catches stops the program and shows where it happened:

```
fun area(width, height)
    return width * hieght
print(area(2, 3))
```

```
area.relic:2:20: error: 'hieght' is not defined (did you mean 'height'?)
        return width * hieght
                       ^
```

`relic check area.relic` finds the same mistake without running the file.

## 7. Modules and the standard library

One file is one module. `use lib/mathx` loads `lib/mathx.relic`, found next to the file that has the `use`.

```
# lib/mathx.relic
let _calls = 0
fun square(x)
    _calls = _calls + 1
    return x * x
```

```
use lib/mathx
use lib/mathx as m
use std/strings
use std/lists

print(mathx.square(4), m.square(5))
print(strings.split("a,b,c", ","))
print(lists.sort([3, 1, 2]))
```

```
16 25
["a", "b", "c"]
[1, 2, 3]
```

- Names starting with `_` stay private to their file. `mathx._calls` from outside is an error.
- A module runs once, however many times it is used.
- `std/...` is the standard library: strings, lists, maps, math, files, paths, json, os, time, vfs and graphics. See [STDLIB.md](STDLIB.md).

## 8. Scripts and graphics

- `relic tool.relic a b` passes `a` and `b` to the program. `os.args()` (or the built-in `args()`) gives `["a", "b"]`.
- `std/files` reads and writes text files, `std/json` reads and writes JSON, `std/os` has `env`, `cwd` and `exit`.
- Windows, drawing and input come from `std/graphics` in a build made with `-DRELIC_SDL=ON` (the host) or `-DRELIC_C_SDL=ON` (compiled programs). Start with `examples/hello_window.relic`. See [examples/](../examples/README.md).

## Where next

- [LANGUAGE.md](LANGUAGE.md): every rule in one place
- [STDLIB.md](STDLIB.md): every std function
- [CLI.md](CLI.md): command line, `relic check` and the REPL
