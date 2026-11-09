# std/strings

Search, split, case and padding. `use std/strings`, then `strings.find(...)`.

## std/strings

A string argument that is not a string fails with the function's name, as in `strings.upper(): argument must be a string, got int`.

### find, contains, count, find_last

`find(s, sub)` is the index of the first match, or `-1`. An empty `sub` is at `0`. Two lists find a run of items.

`find("hello", "ll")` → `2`

`find("hello", "z")` → `-1`

`find([1, 2, 3], [2])` → `1`

`contains(s, sub)` is `find(s, sub) >= 0`.

`contains("hello", "ell")` → `true`

`contains("hello", "xyz")` → `false`

`count(s, sub)` counts non-overlapping matches from the left. An empty `sub` is an error.

`count("aaa", "aa")` → `1`

`count("hello", "l")` → `2`

`find_last(s, sub)` is where the last match starts, and overlaps count.

`find_last("aaa", "aa")` → `1`

`find_last("hello", "z")` → `-1`

Programs: [find.relic](../../examples/ref/strings/find.relic), [contains.relic](../../examples/ref/strings/contains.relic), [count.relic](../../examples/ref/strings/count.relic), [find_last.relic](../../examples/ref/strings/find_last.relic).

### starts_with, ends_with

Both take two strings, or two lists.

`starts_with("hello", "he")` → `true`

`starts_with("hello", "lo")` → `false`

`starts_with([1, 2, 3], [1, 2])` → `true`

`ends_with("hello", "lo")` → `true`

`ends_with([1, 2, 3], [2, 3])` → `true`

### upper, lower, capitalize

ASCII letters only. Other characters stay as they are. `capitalize` changes the first character and leaves the rest.

`upper("Hello")` → `"HELLO"`

`lower("Hello")` → `"hello"`

`capitalize("hello")` → `"Hello"`

`capitalize("relic lang")` → `"Relic lang"`

### trim, trim_start, trim_end

Space, tab, newline and carriage return. `trim` looks at both ends. The other two look at one end.

`trim("  hi  ")` → `"hi"`

`trim_start("  hi  ")` → `"hi  "`

`trim_end("  hi  ")` → `"  hi"`

### Tests on characters

`is_digits` needs at least one character, each `0` to `9`. No sign and no point.

`is_digits("2048")` → `true`

`is_digits("")` → `false`

`is_digits("-3")` → `false`

`is_alpha` is the same idea for ASCII letters.

`is_alpha("Relic")` → `true`

`is_alpha("R2")` → `false`

`is_upper` is true when there is at least one ASCII letter and none are lower case. Digits do not count against it. `is_lower` is the other way round.

`is_upper("A1")` → `true`

`is_upper("Relic")` → `false`

`is_upper("1")` → `false`

`is_lower("a1")` → `true`

`is_space` is one character: space, tab, newline or carriage return.

`is_space(" ")` → `true`

`is_space("a")` → `false`

### reverse, remove_prefix, remove_suffix

`reverse` flips code points, so a character past ASCII stays one character.

`reverse("relic")` → `"ciler"`

`reverse("ab")` → `"ba"`

`remove_prefix` and `remove_suffix` drop the part once when it is there, and return the text unchanged when it is not.

`remove_prefix("unhappy", "un")` → `"happy"`

`remove_prefix("happy", "un")` → `"happy"`

`remove_suffix("file.txt", ".txt")` → `"file"`

`remove_suffix("file.txt", ".md")` → `"file.txt"`

### words, split, lines, join, replace, repeat

`words` splits on runs of space and never returns an empty piece.

`words("  a  b ")` → `["a", "b"]`

`split` keeps empty pieces. An empty separator splits into characters.

`split("a,,b", ",")` → `["a", "", "b"]`

`split("ab", "")` → `["a", "b"]`

`lines` splits on newlines, drops a carriage return before a newline, and drops an empty last line when the text ends with a newline.

`lines("one\ntwo\n")` → `["one", "two"]`

`join` turns each item into text with `str` and puts the separator between them.

`join(["a", "b", "c"], ", ")` → `"a, b, c"`

`join([1, 2, 3], "-")` → `"1-2-3"`

`replace` swaps every non-overlapping match from the left.

`replace("aaa", "aa", "b")` → `"ba"`

`repeat` copies the text. A count of 0 or less gives `""`.

`repeat("ab", 3)` → `"ababab"`

`repeat("ab", 0)` → `""`

### pad_left, pad_right, center, truncate

`pad_left` and `pad_right` add copies of `fill` until the text is at least `width` long. `center` puts the spare copy on the right. `truncate` cuts to `width` and ends a cut string with `tail`.

`pad_left("42", 5, "0")` → `"00042"`

`pad_right("go", 5, ".")` → `"go..."`

`center("hi", 6, ".")` → `"..hi.."`

`center("odd", 6, ".")` → `".odd.."`

`truncate("abcdef", 4, "...")` → `"a..."`

`truncate("abcdef", 2, "...")` → `".."`

`truncate("hi", 4, "...")` → `"hi"`
