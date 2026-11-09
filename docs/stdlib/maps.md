# std/maps

Look up, reshape and merge maps. `use std/maps`. Keys are strings. A value that is not a map fails with the function's name: `maps.values(): m must be a map, got int`.

## std/maps

### Reading

`values` and `items` follow the order the keys were added. `items` is a list of `[key, value]` pairs. `get` returns the fallback when the key is missing. `find_key` is the first key whose value `==` the one you ask about, or `none`. `has_value` is whether any value matches. `1` and `1.0` match.

`values({a: 1, b: 2, c: 3})` → `[1, 2, 3]`

`items({a: 1, b: 2})` → `[["a", 1], ["b", 2]]`

`get({a: 1}, "a", 0)` → `1`

`get({a: 1}, "z", 0)` → `0`

`find_key({a: 1, b: 2, c: 1}, 1)` → `"a"`

`find_key({a: 1}, 9)` → `none`

`has_value({a: 1, b: 2}, 2)` → `true`

`has_value({a: 1}, 1.0)` → `true`

`has_value({a: 1}, 3)` → `false`

### Building a new map

`merge(a, b)` copies both. `b` wins on a shared key. `from_items` is the reverse of `items`, and a later pair with the same key wins. `invert` swaps string values back to keys. Of two keys with one value, the later key wins. A value that is not a string is an error. `pick` copies the keys you name, in that order, and skips a missing key.

`merge({a: 1, b: 2}, {b: 9, c: 3})` → `{a: 1, b: 9, c: 3}`

`from_items([["a", 1], ["b", 2], ["a", 3]])` → `{a: 3, b: 2}`

`invert({a: "x", b: "y", c: "x"})` → `{x: "c", y: "b"}`

`pick({a: 1, b: 2, c: 3}, ["c", "a", "z"])` → `{c: 3, a: 1}`

### In place, and with a function

`drop(m, k)` removes `k` if it is there and returns true or false. The map changes. [drop.relic](../../examples/ref/maps/drop.relic) prints `true`, then `false`, then `{b: 2}`.

`map_values(m, f)` keeps the keys and sets each value to `f(value)`. [map_values.relic](../../examples/ref/maps/map_values.relic) prints `{a: 2, b: 4}`.

`filter(m, keep)` keeps entries where `keep(key, value)` is true. [filter.relic](../../examples/ref/maps/filter.relic) prints `{b: 3, c: 2}`.
