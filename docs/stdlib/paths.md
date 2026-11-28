# std/paths

Paths always use `/`. A backslash in the input is read as `/`. `use std/paths`. A path that is not a string fails with the function's name.

## std/paths

### Join and split

`join(a, b)` puts a slash between the parts. An absolute `b` replaces `a`. `base` is the last name. A path that ends in `/` has an empty base. `dir` is everything before that name. `ext` keeps the dot, and a leading dot belongs to the name, so `.hidden` has no extension. `stem` is the name without its last extension. A dotfile keeps its whole name.

`join("a", "b")` → `"a/b"`

`join("a/", "/b")` → `"/b"`

`join("a", "")` → `"a/"`

`base("a/b.txt")` → `"b.txt"`

`base("a/")` → `""`

`dir("a/b.txt")` → `"a"`

`dir("b.txt")` → `""`

`dir("/a/b")` → `"/a"`

`ext("a/b.txt")` → `".txt"`

`ext("a.tar.gz")` → `".gz"`

`ext(".hidden")` → `""`

`ext("Makefile")` → `""`

`stem("b.txt")` → `"b"`

`stem("a.tar.gz")` → `"a.tar"`

`stem(".bashrc")` → `".bashrc"`

### Normalize, parts, extension, relative

`normalize` collapses `.` and `..`. `parts` is those names. An absolute path starts with `"/"`. `.` has no parts. `is_abs` is whether the path starts with `/` or a backslash. `with_ext` swaps the extension. The new one is `""` or it starts with a dot and holds no slash. `relative(base, target)` is the path from the folder `base` to `target`. Both must be absolute, or both relative. The same folder is `"."`.

`normalize("a/./b/../c")` → `"a/c"`

`normalize("a\\b")` → `"a/b"`

`parts("a/./b/../c")` → `["a", "c"]`

`parts("/a/b")` → `["/", "a", "b"]`

`parts(".")` → `[]`

`is_abs("/a")` → `true`

`is_abs("a/b")` → `false`

`is_abs("")` → `false`

`with_ext("a/b.txt", ".md")` → `"a/b.md"`

`with_ext("a/b.txt", "")` → `"a/b"`

`with_ext(".hidden", ".txt")` → `".hidden.txt"`

`relative("/a/b", "/a/c/d")` → `"../c/d"`

`relative("/a/b", "/a/b")` → `"."`

`relative("a/b", "a/c")` → `"../c"`

### has_ext

`has_ext(p, e)` is whether `p` ends with the extension `e`. `e` may be `".txt"` or `"txt"`. `""` means `p` has no extension.

`has_ext("a/b.txt", ".txt")` → `true`

`has_ext("a/b.txt", "txt")` → `true`

`has_ext("Makefile", "")` → `true`

### with_stem

`with_stem(p, stem)` replaces the file name and keeps the directory and the extension. `stem` must not hold a slash. A dotfile has no extension, so the whole name is replaced.

`with_stem("a/b.txt", "c")` → `"a/c.txt"`

`with_stem("/b.txt", "c")` → `"/c.txt"`

`with_stem("a/.bashrc", "c")` → `"a/c"`

### join_all

`join_all(parts)` joins every part, left to right, with the same rule as `join`: an absolute part replaces what came before it. An empty list is `""`.

`join_all(["a", "b", "c"])` → `"a/b/c"`

`join_all(["a", "/b", "c"])` → `"/b/c"`

`join_all([])` → `""`

### split_ext

`split_ext(p)` returns `[p without its extension, the extension]`. The extension keeps the dot, or is `""` when there is none. A dotfile stays in one piece.

`split_ext("a/b.tar.gz")` → `["a/b.tar", ".gz"]`

`split_ext(".bashrc")` → `[".bashrc", ""]`
