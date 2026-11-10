# std/lists

Build, search and reorder lists. `use std/lists`, then `lists.sum(...)`.

A string passed as `xs` is read as its characters. A map is read as its keys. Anything else fails with the function's name: `lists.sum(): xs must be a list, got int`.

## std/lists

### Adding and counting

`sum` adds the numbers, from 0. `product` multiplies them, and an empty list is 1. `mean` is the sum divided by the count. `count` is how many items are `==` the one you ask about. `1` and `1.0` match.

`sum([1, 2, 3, 4])` → `10`

`sum([])` → `0`

`product([2, 3, 4])` → `24`

`product([])` → `1`

`mean([1, 2, 3, 4])` → `2.5`

`count([1, 1.0, 2], 1)` → `2`

`index_of` is the first index, or `-1`. `contains` is that index being at least 0.

`index_of([1, 2, 3], 1.0)` → `0`

`index_of([1, 2], 9)` → `-1`

`contains([1, 2], 1.0)` → `true`

`min` and `max` use `<`. `last` is the final item. An empty list is an error for `min`, `max`, `mean` and `last`.

`min([3, 1, 2])` → `1`

`max(["b", "a", "c"])` → `"c"`

`last([1, 2, 3])` → `3`

Programs: [sum.relic](../../examples/ref/lists/sum.relic), [mean.relic](../../examples/ref/lists/mean.relic), [product.relic](../../examples/ref/lists/product.relic), [min.relic](../../examples/ref/lists/min.relic), [max.relic](../../examples/ref/lists/max.relic), [last.relic](../../examples/ref/lists/last.relic).

### Cuts and order

`take` keeps the first n items. `drop` skips them. `chunk` cuts into pieces of n, and the last piece may be short. `windows` is every run of n items. `rotate` moves the first n items to the end. A negative n goes the other way. `reverse` returns a new list. `unique` keeps the first of each item. `sort` returns a new sorted list. `flatten` joins one level. `zip` pairs items until the shorter list ends. `copy` is a shallow copy.

`take([1, 2, 3, 4], 2)` → `[1, 2]`

`drop([1, 2, 3, 4], 2)` → `[3, 4]`

`chunk([1, 2, 3, 4, 5], 2)` → `[[1, 2], [3, 4], [5]]`

`windows([1, 2, 3, 4], 2)` → `[[1, 2], [2, 3], [3, 4]]`

`windows([1], 2)` → `[]`

`rotate([1, 2, 3, 4], 1)` → `[2, 3, 4, 1]`

`rotate([1, 2, 3, 4], -1)` → `[4, 1, 2, 3]`

`reverse([1, 2, 3])` → `[3, 2, 1]`

`unique([1, 2, 1, 3, 2])` → `[1, 2, 3]`

`sort([3, 1, 2, 1])` → `[1, 1, 2, 3]`

`sort(["b", "a", "c"])` → `["a", "b", "c"]`

`flatten([[1, [2]], 3])` → `[1, [2], 3]`

`zip([1, 2, 3], ["a", "b"])` → `[[1, "a"], [2, "b"]]`

`copy` does not share the list. [copy.relic](../../examples/ref/lists/copy.relic) appends to the copy and prints `[1, 2]` then `[1, 2, 3]`.

### Calls that take a function

`map(xs, f)` builds `[f(x), ...]`. `filter(xs, keep)` keeps items where `keep(x)` is true. `reduce(xs, f, start)` is `f(f(start, first), next)`. `any` and `all` stop at the first item that decides. An empty list is false for `any` and true for `all`. `find` returns the item or `none`. `find_index` returns the index or `-1`. `partition` returns `[yes, no]`. `group_by` and `count_by` key the items. The key must be a string. `min_by` and `max_by` pick the item whose key is smallest or largest, the first of a tie. `sum_by` adds `f(x)`. `sort_by(xs, less)` is stable: `less(a, b)` says whether `a` belongs before `b`.

[map.relic](../../examples/ref/lists/map.relic) prints `[2, 4, 6]`. [filter.relic](../../examples/ref/lists/filter.relic) prints `[2, 4]`. [reduce.relic](../../examples/ref/lists/reduce.relic) prints `10` and then `10` for an empty list started at 10. [partition.relic](../../examples/ref/lists/partition.relic) prints `[[2, 4], [1, 3, 5]]`. [group_by.relic](../../examples/ref/lists/group_by.relic) prints `{a: ["ant", "asp"], b: ["bee"], c: ["cow"]}`. [sort_by.relic](../../examples/ref/lists/sort_by.relic) prints `["a", "d", "cc", "bbb"]`.

`fun_add` and `natural` are helpers behind `sum` and `sort`. They are not part of the call list above.
