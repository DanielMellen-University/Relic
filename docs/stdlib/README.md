# Standard library reference

One page per module. Load a module with `use std/NAME` and call `NAME.fun(...)`.

A checked line shows a call, then an arrow, then the returned value. Strings are shown in quotes. `stdlib_examples` runs each of those lines. A longer program for the same call lives in `examples/ref/` and has its own expected output, checked by the same test.

[STDLIB.md](../STDLIB.md) is the full note, including the history of each error message. These pages are the short version for reading one module at a time.

| Module | For |
|---|---|
| [strings](strings.md) | search, split, case, padding |
| [lists](lists.md) | map, filter, sort, windows |
| [maps](maps.md) | lookup, merge, pick |
| [math](math.md) | rounding, roots, gcd, angles |
| [paths](paths.md) | `/` paths |
| [json](json.md) | JSON in and out |
| [files](files.md) | the host disk |
| [vfs](vfs.md) | an in-memory disk |
| [os](os.md) | arguments, environment, input |
| [time](time.md) | clocks and a frame timer |
| [graphics](graphics.md) | a window, drawing, input |

Runnable programs that use several modules together are in [examples/learn](../../examples/learn/README.md).
