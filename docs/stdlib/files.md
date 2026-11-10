# std/files

Text files and folders on the host disk. Paths are relative to where `relic` was started. `use std/files`.

Caught errors use the std name (`files.list(): cannot list x`), not the host name underneath.

These calls write in the current folder, so the checked examples are programs rather than one-line arrows. Each one runs in an empty directory.

## std/files

| Call | Returns | On failure | Program |
|---|---|---|---|
| `read(path)` | the whole text | `cannot open {path}`, also for a folder | [read.relic](../../examples/ref/files/read.relic) prints `true` when the text matches |
| `write(path, text)` | none | `cannot write {path}` | [write.relic](../../examples/ref/files/write.relic) writes `hello` and prints it back |
| `append_text(path, text)` | none | `cannot write {path}` | [append_text.relic](../../examples/ref/files/append_text.relic) prints `one` then `two` |
| `lines(path)` | list of strings | same as `read` | [lines.relic](../../examples/ref/files/lines.relic) prints `["one", "two"]`. A file that ends in a newline has no empty last line |
| `exists(path)` | bool | never fails. True for a file or a folder | [exists.relic](../../examples/ref/files/exists.relic) prints `true` then `false` |
| `is_dir(path)` | bool | never fails. False if missing | [is_dir.relic](../../examples/ref/files/is_dir.relic) prints `true`, `false`, `false` |
| `list(path)` | sorted names | `cannot list {path}` | [list.relic](../../examples/ref/files/list.relic) prints `["a.txt", "b.txt"]` |
| `make_dir(path)` | none | parents too. Fine if it already exists. `cannot make folder {path}` | [make_dir.relic](../../examples/ref/files/make_dir.relic) prints `true` |
| `remove(path)` | none | a file or an empty folder. Fails if missing or if the folder still has entries | [remove.relic](../../examples/ref/files/remove.relic) prints `false` |
| `remove_tree(path)` | none | folder and contents, or one file. A missing path is a no-op. A link is removed as the link | [remove_tree.relic](../../examples/ref/files/remove_tree.relic) prints `false` |
| `rename(from, to)` | none | `cannot rename {from}`. A folder is not renamed | [rename.relic](../../examples/ref/files/rename.relic) prints `hi` then `false` |
| `size(path)` | int, bytes | `cannot get the size of {path}`, also for a folder | [size.relic](../../examples/ref/files/size.relic) prints `2` |

`{path}` in the failure column is the path you passed, not a second argument.
