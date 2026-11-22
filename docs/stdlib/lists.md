# std/lists

Build, search and reorder lists. `use std/lists`, then `lists.sum(...)`.

A string passed as `xs` is read as its characters. A map is read as its keys. Anything else fails with the function's name: `lists.sum(): xs must be a list, got int`.

## std/lists

### Adding and counting

`sum` adds the numbers, from 0. `product` multiplies them, and an empty list is 1. `mean` is the sum divided by the count. `median` is the middle item once sorted, or the mean of the two middle items (a float) when the count is even. `count` is how many items are `==` the one you ask about. `1` and `1.0` match.

`sum([1, 2, 3, 4])` → `10`

`sum([])` → `0`

`product([2, 3, 4])` → `24`

`product([])` → `1`

`mean([1, 2, 3, 4])` → `2.5`

`median([3, 1, 2])` → `2`

`median([4, 1, 3, 2])` → `2.5`

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

`bisect` works on a list that is already sorted. It says where an item would go: the index of the first item that is not smaller, so before equal ones. It is a binary search, so a long list costs a handful of compares.

`bisect([1, 3, 3, 5], 3)` → `1`

`bisect([1, 3, 3, 5], 4)` → `3`

`bisect(["apple", "fig", "pear"], "kiwi")` → `2`

`insert_sorted` returns a new sorted list with the item added after any equal ones, so adding items one at a time keeps ties in the order they came, as `sort` does.

`insert_sorted([1, 3, 5], 4)` → `[1, 3, 4, 5]`

`union`, `intersect` and `difference` treat two lists as sets. The result has no item twice and keeps the order items first came in, `a` before `b`. Items match by `==`, so `1` and `1.0` are one item and the first is kept.

`union([1, 2, 2], [3, 2])` → `[1, 2, 3]`

`intersect([1, 2, 3, 2], [2, 3, 5])` → `[2, 3]`

`difference(["a", "b", "a"], ["b"])` → `["a"]`

`copy` does not share the list. [copy.relic](../../examples/ref/lists/copy.relic) appends to the copy and prints `[1, 2]` then `[1, 2, 3]`.

### Calls that take a function

`map(xs, f)` builds `[f(x), ...]`. `filter(xs, keep)` keeps items where `keep(x)` is true. `reduce(xs, f, start)` is `f(f(start, first), next)`. `scan(xs, f, start)` gives every one of those steps in a list, so its last item is what `reduce` returns and an empty list gives `[]`. `any` and `all` stop at the first item that decides. An empty list is false for `any` and true for `all`. `find` returns the item or `none`. `find_index` returns the index or `-1`. `take_while` keeps items from the front while `test(x)` is true and stops at the first false one. `drop_while` returns the rest, from that item on. `partition` returns `[yes, no]`. `group_by` and `count_by` key the items, and `unique_by` keeps the first item for each key. The key must be a string. `min_by` and `max_by` pick the item whose key is smallest or largest, the first of a tie. `sum_by` adds `f(x)`. `sort_by(xs, less)` is stable: `less(a, b)` says whether `a` belongs before `b`.

[map.relic](../../examples/ref/lists/map.relic) prints `[2, 4, 6]`. [filter.relic](../../examples/ref/lists/filter.relic) prints `[2, 4]`. [reduce.relic](../../examples/ref/lists/reduce.relic) prints `10` and then `10` for an empty list started at 10. [partition.relic](../../examples/ref/lists/partition.relic) prints `[[2, 4], [1, 3, 5]]`. [group_by.relic](../../examples/ref/lists/group_by.relic) prints `{a: ["ant", "asp"], b: ["bee"], c: ["cow"]}`. [sort_by.relic](../../examples/ref/lists/sort_by.relic) prints `["a", "d", "cc", "bbb"]`.

`fun_add` and `natural` are helpers behind `sum` and `sort`. They are not part of the call list above.

### enumerate

`enumerate(xs)` is `[index, item]` pairs from 0. A string is its characters.

`enumerate([10, 20])` → `[[0, 10], [1, 20]]`

`enumerate([])` → `[]`

### concat

`concat(parts)` joins a list of lists into one, in order. An item that is not a list is an error.

`concat([[1, 2], [], [3]])` → `[1, 2, 3]`

`concat([])` → `[]`

### intersperse

`intersperse(xs, sep)` puts `sep` between the items, not before the first or after the last.

`intersperse([1, 2, 3], 0)` → `[1, 0, 2, 0, 3]`

`intersperse([1], 0)` → `[1]`

### compact

`compact(xs)` drops `none` and keeps `false`, `0` and `""`.

`compact([1, none, false, 0, ""])` → `[1, false, 0, ""]`

### reject

`reject(xs, test)` is the items `test(x)` is not true for, in order. It is the opposite of `filter`.

### find_last

`find_last(xs, test)` is the last item `test(x)` is true for, or `none`. `test` is called on every item.

### count_if

`count_if(xs, test)` is how many items `test(x)` is true for. An empty list is 0.

### without

`without(xs, item)` is a new list with every item `==` `item` removed, so `1` and `1.0` both go. `xs` is unchanged.

`without([1, 1.0, 2, 1], 1)` → `[2]`

`without([1, 2], 9)` → `[1, 2]`

### dedupe

`dedupe(xs)` drops items that are `==` the one just before them. A later copy that is not next to the first stays.

`dedupe([1, 1.0, 2, 2, 1])` → `[1, 2, 1]`

`dedupe([])` → `[]`

### split_at

`split_at(xs, n)` is `[the first n items, the rest]`. `n` past the end takes everything and leaves the rest empty.

`split_at([1, 2, 3, 4], 2)` → `[[1, 2], [3, 4]]`

`split_at([1], 5)` → `[[1], []]`

### take_last

`take_last(xs, n)` is the last `n` items, or all of them when there are fewer.

`take_last([1, 2, 3, 4], 2)` → `[3, 4]`

`take_last([1, 2], 9)` → `[1, 2]`

### drop_last

`drop_last(xs, n)` is the items with the last `n` dropped, or `[]` when `n` is at least the length.

`drop_last([1, 2, 3, 4], 2)` → `[1, 2]`

`drop_last([1], 3)` → `[]`

### flat_map

`flat_map(xs, f)` calls `f(x)` for each item and joins the lists it returns. `f` must give a list.
