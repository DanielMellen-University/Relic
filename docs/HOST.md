# The C++ host: built-ins and embedding

Relic runs on a C++17 host today. This page covers the `host_*` built-ins the standard library is made of, how to add one, and how to run Relic from other C++ code. The host's layout is in [ARCHITECTURE.md](ARCHITECTURE.md).

## Host built-ins

Pieces Relic cannot do itself yet come from C++ and are named `host_*`, so the borrowed list stays visible. Programs should call the `std/` wrappers instead; the names and argument checks there are the stable surface. Every `host_*` call is a normal built-in: errors can be caught, and inside `std/` they are reported under the std name (`files.read(): cannot open x`).

| Built-in | Wrapped by | Does |
|---|---|---|
| `host_read_file(path)` | `files.read`, `files.lines` | whole file as a string |
| `host_write_file(path, text, append)` | `files.write`, `files.append_text` | writes, or appends when `append` is `true` |
| `host_file_exists(path)` | `files.exists` | true for a file or a folder |
| `host_list_dir(path)` | `files.list` | sorted names in a folder |
| `host_is_dir(path)` | `files.is_dir` | |
| `host_make_dir(path)` | `files.make_dir` | parent folders too |
| `host_remove(path)` | `files.remove`, `files.remove_tree` | a file or an empty folder |
| `host_rename(from, to)` | `files.rename` | |
| `host_file_size(path)` | `files.size` | bytes |
| `host_clock()` | `time.clock`, `time.since`, the frame timer | steady clock, seconds |
| `host_now()` | `time.now` | seconds since 1970 UTC |
| `host_sleep(ms)` | `time.sleep`, `time.limit` | |
| `host_floor`, `host_sqrt`, `host_sin`, `host_cos`, `host_exp`, `host_log`, `host_pow` | `std/math` | float math |
| `host_args()` | `os.args` | same list as the `args()` built-in |
| `host_env(name)` | `os.env` | a string or `none` |
| `host_cwd()` | `os.cwd` | `/` separators |
| `host_exit(code)` | `os.exit` | flushes output, then exits |
| `host_gfx_*`, `host_clipboard_get`, `host_clipboard_set` | `std/graphics` | SDL2 graphics, input and clipboard ([STDLIB.md](STDLIB.md#stdgraphics)) |

Used by the self-hosted interpreter:

| Built-in | Does |
|---|---|
| `host_std_root()` | the folder `use std/...` loads from |
| `host_names()` | `[name, arity]` for every `host_*` built-in, sorted |
| `host_call(name, args)` | calls a `host_*` built-in with a list of arguments |
| `host_same(a, b)` | true when two lists or maps are the very same value, not just equal |

`relic` builds without SDL still have every `host_gfx_*` name. Each one then fails with a message saying how to rebuild, and `host_gfx_available()` is `false`.

## Adding a built-in

1. Add it in `Interpreter::install_builtins()` in `src/builtins/builtins.cpp` (graphics ones go in `src/builtins/graphics.cpp`):

   ```cpp
   add("host_thing", 1, [str_arg](std::vector<Value>& a) {
       const std::string& p = str_arg(a[0], "path");
       if (p.empty()) throw std::string("cannot do the thing");
       return Value(true);
   });
   ```

   The number is the arity (`-1` takes any number). Throw a `std::string` to raise a Relic error.
2. Both engines and the self-hosted interpreter pick it up. The tree-walker and the VM share the built-in table, and `host_call` reaches any `host_*` name.
3. Wrap it in a `std/` module with a plain Relic name, and add a row to [STDLIB.md](STDLIB.md).
4. Add a golden test (`tests/golden/std_thing.relic` and `.out`, plus `.err` tests for each failure).
5. Add it to the borrowed list in [VISION.md](VISION.md#borrowed-today) if it is a new kind of borrowed piece.

New built-ins are a new feature, so they need Daniel's approval first (see [CONTRIBUTING.md](../CONTRIBUTING.md)).

## Embedding

The `relic_core` static library holds everything except `main`. Link it and include `core.hpp` (the `src/` folder and the build's `generated/` folder must be on the include path, which the CMake target sets up).

| Call | Does |
|---|---|
| `run_source_vm(path, source)` | runs a whole program on the VM; output comes back in `RunResult::out` |
| `run_source_vm(path, source, out)` | same, printing to the stream `out` as it goes |
| `run_source(path, source)` / `run_source(path, source, out)` | same on the tree-walker |
| `Session(out)` then `session.run(path, source, echo)` | keeps globals between runs, like the REPL; `echo` prints a final bare expression |
| `repl(in, out, err, interactive)` | the whole REPL loop |
| `check_source(path, source)` | `relic check` for one file |
| `dump_tokens`, `dump_tree`, `dump_bytecode` | the debug dumps |
| `version_string()` | `"relic 1.0.6"` |
| `script_args()` | the list `args()` returns; fill it before running |

`RunResult` has `exit_code`, `out` and `err`. `path` is used for error messages and to resolve `use`.

```cpp
#include "core.hpp"
#include <iostream>

int main() {
    relic::RunResult r = relic::run_source_vm("demo.relic", "print(6 * 7)\n");
    std::cout << r.out;   // 42
    std::cerr << r.err;
    return r.exit_code;
}
```

Limits today: `host_exit` ends the whole process, and there is no API yet for adding built-ins from outside `relic_core`.
