# std/text

More text helpers on top of `std/strings`. Case that looks at words, splitting once, and indent. `use std/text`.

## std/text


### is_blank

`is_blank(s)` is true when `s` is empty or every character is space, tab, newline or CR.

`is_blank("  ")` → `true`

`is_blank("")` → `true`

`is_blank(" a")` → `false`

### partition

`partition(s, sep)` is `[the text before sep, sep, the text after]` for the first `sep`. When `sep` is not there, the result is `[s, "", ""]`.

`partition("a=b=c", "=")` → `["a", "=", "b=c"]`

`partition("abc", "=")` → `["abc", "", ""]`

### replace_first

`replace_first(s, old, new)` replaces the first `old` with `new`. No match leaves `s` as it is. An empty `old` is an error.

`replace_first("aaa", "a", "b")` → `"baa"`

`replace_first("abc", "z", "q")` → `"abc"`
