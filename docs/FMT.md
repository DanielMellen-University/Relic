# relic-fmt and relic fmt

`relic-fmt` rewrites Relic source into one canonical form. The language does not change: the same tokens, the same blocks, the same values. It is written in Relic in `selfhost/fmt.relic` and compiled by the default C-only build. The optional C++ host provides the same formatting rules as `relic fmt` ([CLI.md](CLI.md)).

## One file

From the repository root, with only a C compiler and CMake:

```sh
cmake -S . -B build
cmake --build build --target relic-fmt
./build/relic-fmt tests/fmt/in/readme_example.relic
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

Both entry points take the same files and flags. Use `./build/relic-fmt` with
the default build, or `./build/relic fmt` when the host is built. With Visual
Studio the compiled formatter is `build/Release/relic-fmt.exe`.

| Command | Does | Exit |
|---|---|---|
| `relic-fmt FILE...` | writes the canonical text of each file to stdout, one after another | 0, or 1 if a file does not parse |
| `relic-fmt --write FILE...` | updates each file in place. A file that is already canonical is not touched | 0, or 1 if a file does not parse |
| `relic-fmt --check FILE...` | prints each path that would change | 0 if nothing would change, 1 if something would or a file does not parse |

`--check` and `--write` together are a usage error (exit 2), as is `relic-fmt` with no file. A missing file or a folder is `relic: cannot open FILE` and exit 2. A syntax error is printed like any other Relic error, and that file is not written.

`--write` uses LF line endings. A changed file is written through a sibling
temporary file and replaced only after the write and close succeed. A write
or replace failure is `relic: cannot write FILE` and exit 2. Other files are
still processed; exit 2 takes priority over exit 1.

The same Relic driver can also run on the optional host or through the
Relic-written interpreter:

```sh
./build/relic selfhost/fmt.relic FILE...
./build/relic selfhost/run.relic selfhost/fmt.relic FILE...
```

Its syntax diagnostics use the same errors and hints as the other self-hosted
drivers.

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

- A file the lexer or parser rejects is not formatted. Tabs, a bad indent, and other syntax errors stay errors. `relic fmt` does not repair them.
- It does not join or split statements onto different lines, other than the blank-line rule and the indent of a wrapped line. A long line stays one line.
- It does not walk a directory. Pass the files.
- Column-aligned comments are pulled back to two spaces before `#`. The comment text is the same.
- Two negations are written `- -x`, with a space, so they stay two minuses.

`compiled_fmt` checks the six golden pairs in `tests/fmt/in` and `tests/fmt/out`,
idempotence, CLI modes, rejected inputs and write failures. These checks run
without the host. `compiled_fmt_unreadable` checks unreadable input where file
permissions can deny reads. `compiled_fmt_corpus` checks every `.relic` source
outside build directories: invalid files are rejected, accepted files keep
their tokens and still parse, and formatting twice changes nothing. With the
host enabled it also compares the two formatters on that corpus.

The host's `fmt` test uses the same goldens and CLI checks, then runs changed
programs in an isolated copy of the tree and compares stdout, stderr and exit
codes. `selfhost_fmt` and `selfhost_fmt_nested` run the Relic driver on the host
and through `selfhost/run.relic`. Write, close and rename failures must keep
the original file and remove the temporary file; the host also tests an
explicit flush failure.
