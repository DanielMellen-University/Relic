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

### title

`title(s)` gives each word an ASCII capital first letter and lower case for the rest. Spaces stay as they are, including runs of them.

`title("hELLO  world")` → `"Hello  World"`

`title("a")` → `"A"`

### indent

`indent(s, prefix)` puts `prefix` at the start of every line. A final newline is kept, and a blank line in the middle gets the prefix too. An empty text stays empty. `indent("a\nb\n", "  ")` is two lines, each starting with two spaces.

`indent("", ">")` → `""`
