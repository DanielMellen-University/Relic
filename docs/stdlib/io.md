# std/io

Small file helpers on top of `std/files`. Paths are relative to where the program runs, the same as `files`. `use std/io`.

## std/io


### is_file

`is_file(path)` is true when `path` is a file. A missing path and a folder are false.

### copy

`copy(from, to)` copies the text of one file onto another. `from` must be a file, not a folder. `to` is created or replaced.
