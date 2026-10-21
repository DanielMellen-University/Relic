# Command line and REPL

Everything the `relic` program does. Build it first ([README](../README.md#build)); the program ends up at `build/relic` (or `build/Release/relic.exe` with Visual Studio).

## Commands

| Command | Does |
|---|---|
| `relic FILE [args...]` | runs the file on the bytecode VM; `args()` gives the extra words |
| `relic --tree FILE [args...]` | runs it on the tree-walk interpreter instead |
| `relic check FILE...` | finds mistakes without running anything (see below) |
| `relic` | starts the REPL |
| `relic --tokens FILE` | prints the tokens, one per line as `line:col KIND text` |
| `relic --ast FILE` | prints the syntax tree |
| `relic --bytecode FILE` | prints the compiled bytecode |
| `relic --version` | prints `relic 1.0.2` |
| `relic --help` | prints the usage lines |

- `--vm` is still accepted and means the same as the default.
- The flag must come first, before the file.
- Anything after the file name goes to the program, including words that start with `--`.

## Exit codes

| Code | When |
|---|---|
| 0 | the program ran to the end, or the REPL was left |
| 1 | a syntax or runtime error nobody caught, or `relic check` found something |
| 2 | bad usage, or the file could not be opened (`relic: cannot open x.relic`) |
| any | the program called `os.exit(code)` |

## Output

- `print` goes to stdout and `eprint` and errors go to stderr.
- On a terminal each `print` shows at once. Into a pipe or a file, output is buffered and written at exit (or at `os.exit`).
- Output printed before an error is kept.

## relic check

`relic check a.relic b.relic` reads each file and reports, without running it:

- syntax errors
- names that are never defined, with a "did you mean" hint
- top-level uses of a name before its `fun` or `let` line
- `break`, `next` or `return` in the wrong place, and other statements that always fail

Each finding prints like an error, with `warning:` in place of `error:`:

```
area.relic:2:20: warning: 'hieght' is not defined (did you mean 'height'?)
        return width * hieght
                       ^
```

The exit code is 1 if anything was found, 0 if every file is clean. Graphics programs can be checked in a build without SDL.

## The REPL

```
$ ./build/relic
relic 1.0.2. Blocks end with an empty line. Type :quit or press Ctrl-D to leave.
> let x = 6
> x * 7
42
> fun greet(who)
...     print("hello {who}")
...
> greet("Daniel")
hello Daniel
```

- Names stay defined until you leave.
- A bare expression prints its value. `none` is not printed.
- A line starting with `fun`, `if`, `else`, `while`, `for`, `try` or `catch` opens a block: type the body indented, then an empty line to run it. The one-line `fun f(x) = ...` runs right away.
- An open `(`, `[` or `{` keeps reading lines until it closes.
- An error prints `<repl>:line:col: error: ...` without the source line, and the REPL keeps going. Line numbers count from the start of that input.
- `use` paths are relative to the current folder.
- Piped input works too: `relic < script.relic` runs it chunk by chunk, without prompts. The exit code is 0 even if a chunk failed.
- The REPL runs on the tree-walker.

## Environment

| Variable | Does |
|---|---|
| `RELIC_STD` | folder that `use std/...` loads from. Without it, `relic` uses the `std/` folder of the source tree it was built from |
| `SDL_VIDEODRIVER=dummy` | in an SDL build, runs graphics programs with no visible window (for tests) |

## Debug dumps

`--tokens`, `--ast` and `--bytecode` take exactly one file and nothing after it. They are what the self-hosted lexer and parser are compared against (see [TESTING.md](TESTING.md#comparison-tests)).

```
$ relic --tokens hello.relic
1:1 NAME print
1:6 LPAREN (
1:7 STRING "hello"
...
```
