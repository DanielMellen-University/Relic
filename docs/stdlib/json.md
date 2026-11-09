# std/json

JSON in and out. `use std/json`. Objects become maps and keep key order. Arrays become lists. `null` is `none`. `true` and `false` stay bools.

## std/json

`encode(v)` writes one line, with no extra spaces. A float is written so that `decode` reads the same value back. `pretty(v)` is the same text with a 2-space indent.

`encode` fails on a function or a module, on inf or nan, and on a list or map that holds itself.

`encode([1, 2])` → `"[1,2]"`

`encode(none)` → `"null"`

`encode(true)` → `"true"`

[encode.relic](../../examples/ref/json/encode.relic) prints `{"a":1,"b":"x"}` and `[true,false,null]`.

[pretty.relic](../../examples/ref/json/pretty.relic) prints a map with a nested list, indented:

```
{
  "name": "Ada",
  "scores": [
    1,
    2
  ]
}
```

`decode(text)` reads JSON. A bad text fails with `json: ... at N`, and `N` is a character index. The text must be a string.

`decode("[1, 2]")` → `[1, 2]`

`decode("true")` → `true`

`decode("null")` → `none`

`decode(encode(v)) == v` for every value `encode` accepts. [decode.relic](../../examples/ref/json/decode.relic) decodes `[1, 2, true, null]` as `[1, 2, true, none]` and checks the round trip.
