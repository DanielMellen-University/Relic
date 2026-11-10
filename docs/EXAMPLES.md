# Example programs

Two kinds of example live under `examples/`.

## Programs

[examples/learn](../examples/learn/README.md) holds programs meant to be read and run: a word-ish counter lives one level up as `examples/word_count.relic`, and the folder itself has a JSON pretty printer, a todo list, matrix helpers, a text adventure, and short algorithms.

```
build/relic examples/learn/adventure.relic
```

Each program has a sibling `.out`. The host test `stdlib_examples` runs the program in an empty directory and compares stdout to that file. Stdin is empty unless a sibling `.in` is present (the reference examples that read a line use one).

The same programs are compiled. `tests/cgen/learn_<name>.relic` loads the example with `use`, and ctest runs the result as `cgen_only_learn_<name>` with the leak check on. The `.out` next to that wrapper is the same bytes as the example's `.out`. The example checker fails if the two files drift.

## One call at a time

`examples/ref/<module>/<call>.relic` shows a single standard-library call and prints a small result. [docs/stdlib](stdlib/README.md) is the page for that module. A line there of the form call, arrow, value is executed by `stdlib_examples` as well. Graphics calls that open a window are not in that set: `available()` depends on the build. The clipboard example does not open a window, so it runs everywhere.

## What is not in this set

`examples/compiled/`, `examples/tablecheck/`, `examples/mini_desktop/`, `examples/hello_window.relic` and `examples/input_demo.relic` already have their own tests. This page does not change them.
