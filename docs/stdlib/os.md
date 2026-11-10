# std/os

Arguments, the environment, the current folder, and standard input. `use std/os`.

In a module, prefer `os.args()` to the built-in `args()`. They are the same list.

## std/os

`args()` is the words after the file name. This test runs the call with no extra words.

`args()` → `[]`

`env(name)` is a string, or `none` when the name is not set. The value of a real variable depends on the machine, so the checked call uses a name that is not set.

`env("RELIC_DOC_EXAMPLE_MISSING")` → `none`

`cwd()` is the current folder, with `/` separators. The text itself changes with where you run. [cwd.relic](../../examples/ref/os/cwd.relic) prints `true` when the result is a non-empty string.

`exit(code)` stops the program after flushing output. `code` must be a whole number. The checked examples do not call it, because a call would not return.

### Lines of input

`read_line()` returns the next line without its line end (`"\n"` or a carriage return followed by `"\n"`), and `none` once the input has run out. An empty line is `""`, so `none` only means the end. [read_line.relic](../../examples/ref/os/read_line.relic) is fed two lines and prints `alpha`, `beta`, then `true`.

`prompt(text)` is `read_line` after showing `text`, and only when stdin is a terminal. A pipe or a file skips the prompt, so the program prints the same answer either way. `text` must be a string. [prompt.relic](../../examples/ref/os/prompt.relic) reads one piped line and prints `Ada`.
