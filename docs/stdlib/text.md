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
