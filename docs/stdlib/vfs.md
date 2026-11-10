# std/vfs

An in-memory filesystem. It is not the host disk. Use [std/files](files.md) for that. `use std/vfs`.

`new()` returns a vfs with `/home/welcome.txt` (the text `Welcome to Relic.` plus a newline) and an empty `/tmp`. Paths use `/` and are normalized. Pass the vfs as the first argument of every other call.

## std/vfs

A path that is not a string, or a first argument that is not a vfs, fails with the function's name.

### Asking what is there

`exists` is true for a file or a folder. A path that goes through a file is not there. `is_dir` is false when the path is missing. `list` returns sorted names. `entries` is `[name, is_folder]` in that same order. `size` is the length of the file text in bytes. `read` returns the text.

`exists(vfs.new(), "/tmp")` → `true`

`exists(vfs.new(), "/nope")` → `false`

`exists(vfs.new(), "/home/welcome.txt/x")` → `false`

`is_dir(vfs.new(), "/home")` → `true`

`is_dir(vfs.new(), "/home/welcome.txt")` → `false`

`list(vfs.new(), "/")` → `["home", "tmp"]`

`list(vfs.new(), "/home")` → `["welcome.txt"]`

`size(vfs.new(), "/home/welcome.txt")` → `18`

[new.relic](../../examples/ref/vfs/new.relic) prints the welcome text and the root listing. [entries.relic](../../examples/ref/vfs/entries.relic) prints `[["a.txt", false], ["sub", true]]`.

### Changing it

`write` stores text and replaces what was there. `append` adds text and creates the file if it was missing. `make_dir` creates parents and is fine if the folder already exists. `remove` deletes a file or an empty folder. `remove_tree` deletes a folder and what is in it. A missing path is a no-op, except `/`. `rename` moves a file or folder. It fails when the destination exists or is inside the source.

These need more than one call, so the checked programs are the examples:

| Call | Program | What it prints |
|---|---|---|
| `write` | [write.relic](../../examples/ref/vfs/write.relic) | `two` |
| `append` | [append.relic](../../examples/ref/vfs/append.relic) | `ab` |
| `make_dir` | [make_dir.relic](../../examples/ref/vfs/make_dir.relic) | `true` then `["b"]` |
| `remove` | [remove.relic](../../examples/ref/vfs/remove.relic) | `false` |
| `remove_tree` | [remove_tree.relic](../../examples/ref/vfs/remove_tree.relic) | `false` |
| `rename` | [rename.relic](../../examples/ref/vfs/rename.relic) | `hi` then `false` |
