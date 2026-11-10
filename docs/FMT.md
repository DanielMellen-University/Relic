# relic fmt

`relic fmt` rewrites Relic source into one canonical form. The language does not change: the same tokens, the same blocks, the same values. It is part of the C++ host, so build with `-DRELIC_BUILD_HOST=ON` first ([CLI.md](CLI.md)).

## One file

From the repository root:

```
./build/relic fmt tests/fmt/in/readme_example.relic
```

The input is `tests/fmt/in/readme_example.relic`. Stdout from that command (it ends with a newline):

```
# a messy program
let names = ["Ada", "Linus"]
fun greet(who) = print("hello {who}")
for name in names
    if (name == "Ada")
        greet(name)  # first
    else
        greet(name)
```

Running either file prints:

```
hello Ada
hello Linus
```

## Commands

| Command | Does | Exit |
|---|---|---|
| `relic fmt FILE...` | writes the canonical text of each file to stdout, one after another | 0, or 1 if a file does not parse |
| `relic fmt --write FILE...` | updates each file in place. A file that is already canonical is not touched | 0, or 1 if a file does not parse |
| `relic fmt --check FILE...` | prints each path that would change | 0 if nothing would change, 1 if something would or a file does not parse |

`--check` and `--write` together are a usage error (exit 2), as is `relic fmt` with no file. A missing file or a folder is `relic: cannot open FILE` and exit 2. A syntax error is printed like any other Relic error, and that file is not written.

`--write` uses LF line endings.

## What it changes

- Indent is 4 spaces. A block the language already accepted is already at that indent. A comment line that is not on a 4-space column, or that sits to the left of the next line, lines up with that line. A wrapped line inside `( )`, `[ ]` or `{ }` keeps its indent when that indent is a multiple of 4. Otherwise it is indented 4 spaces past the statement.
- One space around `+ - * / % == != < <= > >= =` and around `and or not in as`. No space around `.` or `..`, and no space around `/` in a `use` path (`use std/strings`, `1..4`, `a.b`).
- No space before `,` or `:`, one space after. No space before `(` of a call (`print(1, 2)`). A space after `if`, `while`, `for`, `return` and `not` when a parenthesis follows (`if (x)`).
- A trailing comma stays (`print(1, 2,)`).
- A line break the author wrote stays. Blank lines at the start and the end go away, and two or more blank lines in a row become one.
- Trailing whitespace goes away. The file ends with one newline.
- A comment at the end of a line is two spaces, then `#`, then the comment text as it was written. Spaces after `#` stay. Trailing spaces on the comment go away.

Formatting the result again changes nothing.

## What it keeps

Comments stay, including comments on their own line and comments inside a call or a literal. The parser drops those; the formatter reads them from the source, next to the tokens.

String escapes stay as written (`"a\nb"`, `"\u{e9}"`, `"{{"`). Spaces inside `{ }` of an interpolation are expression spaces, so `"hi { name }"` becomes `"hi {name}"`. A space is kept between `{` and `{`, and between `}` and `}`, so a map inside an interpolation does not turn into an escaped brace.

## Limits

- `relic fmt` is the host command. The default build, which makes only `relicc`, does not include it.
- A file the lexer or parser rejects is not formatted. Tabs, a bad indent, and other syntax errors stay errors. `relic fmt` does not repair them.
- It does not join or split statements onto different lines, other than the blank-line rule and the indent of a wrapped line. A long line stays one line.
- It does not walk a directory. Pass the files.
- Column-aligned comments are pulled back to two spaces before `#`. The comment text is the same.
- Two negations are written `- -x`, with a space, so they stay two minuses.

The `fmt` test checks the golden pairs in `tests/fmt/in` and `tests/fmt/out`, checks that formatting twice is a no-op, and runs it on every `.relic` file in the tree. A file that does not parse must be rejected. A file that changes must still run with the same stdout, stderr and exit code. A file that does not change is already the canonical program.
