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

### upper, lower, capitalize, swapcase, title, slug

ASCII letters only. Other characters stay as they are. `capitalize` changes the first character and leaves the rest.

`upper("Hello")` → `"HELLO"`

`lower("Hello")` → `"hello"`

`capitalize("hello")` → `"Hello"`

`capitalize("relic lang")` → `"Relic lang"`

`swapcase` flips ASCII upper case to lower and lower to upper.

`swapcase("Relic 2!")` → `"rELIC 2!"`

`title` puts the first letter of each word in upper case and the rest in lower case. A word starts at a letter that does not follow a letter, a digit or a character past ASCII.

`title("hello world")` → `"Hello World"`

`title("o'neil 2nd")` → `"O'Neil 2nd"`

Program: [title.relic](../../examples/ref/strings/title.relic).

`slug` lowers the text and keeps ASCII letters and digits; each other run of characters becomes one `-`, with none at either end.

`slug("Hello, World!")` → `"hello-world"`

`slug("  Relic 1.0  ")` → `"relic-1-0"`

Program: [slug.relic](../../examples/ref/strings/slug.relic).

### trim, trim_start, trim_end, trim_chars, squeeze

Space, tab, newline and carriage return. `trim` looks at both ends. The other two look at one end.

`trim("  hi  ")` → `"hi"`

`trim_start("  hi  ")` → `"hi  "`

`trim_end("  hi  ")` → `"  hi"`

`trim_chars` removes any character in `chars` from both ends.

`trim_chars("..hi..", ".")` → `"hi"`

`trim_chars("/a/b/", "/")` → `"a/b"`

`squeeze(s, c)` cuts each run of the one character `c` down to a single copy.

`squeeze("a  b   c", " ")` → `"a b c"`

`squeeze("mississippi", "s")` → `"misisippi"`

Program: [squeeze.relic](../../examples/ref/strings/squeeze.relic).

### Tests on characters

`is_digits` needs at least one character, each `0` to `9`. No sign and no point.

`is_digits("2048")` → `true`

`is_digits("")` → `false`

`is_digits("-3")` → `false`

`is_alpha` is the same idea for ASCII letters.

`is_alpha("Relic")` → `true`

`is_alpha("R2")` → `false`

`is_alnum` is true for ASCII letters and digits.

`is_alnum("Relic2")` → `true`

`is_alnum("R-2")` → `false`

`is_alnum("")` → `false`

`is_upper` is true when there is at least one ASCII letter and none are lower case. Digits do not count against it. `is_lower` is the other way round.

`is_upper("A1")` → `true`

`is_upper("Relic")` → `false`

`is_upper("1")` → `false`

`is_lower("a1")` → `true`

`is_space` is one character: space, tab, newline or carriage return.

`is_space(" ")` → `true`

`is_space("a")` → `false`

`is_blank` gives `true` for empty text, or text that holds only spaces, tabs, line feeds and carriage returns.

`is_blank("")` → `true`

`is_blank(" \t\n")` → `true`

`is_blank(" x ")` → `false`

Program: [is_blank.relic](../../examples/ref/strings/is_blank.relic).

### reverse, remove_prefix, remove_suffix

`reverse` flips code points, so a character past ASCII stays one character.

`reverse("relic")` → `"ciler"`

`reverse("ab")` → `"ba"`

`remove_prefix` and `remove_suffix` drop the part once when it is there, and return the text unchanged when it is not.

`remove_prefix("unhappy", "un")` → `"happy"`

`remove_prefix("happy", "un")` → `"happy"`

`remove_suffix("file.txt", ".txt")` → `"file"`

`remove_suffix("file.txt", ".md")` → `"file.txt"`

### words, split, partition, rpartition, lines, join, replace, repeat

`words` splits on runs of space and never returns an empty piece.

`words("  a  b ")` → `["a", "b"]`

`split` keeps empty pieces. An empty separator splits into characters.

`split("a,,b", ",")` → `["a", "", "b"]`

`split("ab", "")` → `["a", "b"]`

`partition` splits around the first match of `sep` into `[before, sep, after]`, or `[s, "", ""]` when `sep` is not there. `rpartition` splits around the last match, or returns `["", "", s]`. An empty `sep` is an error.

`partition("a=b=c", "=")` → `["a", "=", "b=c"]`

`partition("abc", "=")` → `["abc", "", ""]`

`rpartition("a=b=c", "=")` → `["a=b", "=", "c"]`

`rpartition("abc", "=")` → `["", "", "abc"]`

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

### pad_left, pad_right, center, truncate, indent, dedent, zfill, expand_tabs

`pad_left` and `pad_right` add copies of `fill` until the text is at least `width` long. `center` puts the spare copy on the right. `truncate` cuts to `width` and ends a cut string with `tail`.

`pad_left("42", 5, "0")` → `"00042"`

`pad_right("go", 5, ".")` → `"go..."`

`center("hi", 6, ".")` → `"..hi.."`

`center("odd", 6, ".")` → `".odd.."`

`truncate("abcdef", 4, "...")` → `"a..."`

`truncate("abcdef", 2, "...")` → `".."`

`truncate("hi", 4, "...")` → `"hi"`

`indent` adds `prefix` to every non-blank line and leaves blank lines as they are. `dedent` removes common leading spaces or tabs from every non-blank line.

`indent("hi", "  ")` → `"  hi"`

`indent("", "  ")` → `""`

`dedent("  hi")` → `"hi"`

`zfill` pads with zeros on the left, after a leading `+` or `-`.

`zfill("42", 5)` → `"00042"`

`zfill("-42", 5)` → `"-0042"`

`zfill("12345", 3)` → `"12345"`

Program: [zfill.relic](../../examples/ref/strings/zfill.relic).

`expand_tabs(s, size)` turns each tab into the spaces that reach the next stop, one every `size` columns. Columns count again from 0 after a line feed.

`expand_tabs("a\tb", 4)` → `"a   b"`

`expand_tabs("abcd\te", 4)` → `"abcd    e"`

Program: [expand_tabs.relic](../../examples/ref/strings/expand_tabs.relic).

### common_prefix, common_suffix

`common_prefix` gives the longest text that every string in the list starts with, and `common_suffix` does the same at the end. An empty list gives `""`.

`common_prefix(["flower", "flow", "flight"])` → `"fl"`

`common_prefix(["a", "b"])` → `""`

`common_suffix(["running", "jumping", "sing"])` → `"ing"`

`common_suffix([])` → `""`

Program: [common_prefix.relic](../../examples/ref/strings/common_prefix.relic).
