# std/random

Seeded random numbers. `use std/random`.

Make a generator with `new(seed)` and pass it first to every other call. The same seed gives the same numbers on every engine, so a game or a test can replay a run. For numbers that change each run, seed from the clock: `random.new(int(time.now() * 1000))`.

## std/random

### Making a generator

`new(seed)` takes any int. Two generators with one seed give the same numbers. [new.relic](../../examples/ref/random/new.relic) makes two with seed `7` and prints `84 51 96` from each.

`new(1.5)` is an error: `random.new(): seed must be an int, got float`.

### Numbers

`float(r)` is above 0 and below 1, never either end. [float.relic](../../examples/ref/random/float.relic) checks 1,000 of them and prints `true`.

`below(r, n)` is an int from `0` to `n - 1`. `n` is from 1 to 4294967087.

`range(r, lo, hi)` is an int from `lo` to `hi - 1`, the same numbers `lo..hi` walks. [range.relic](../../examples/ref/random/range.relic) rolls a die ten times with `range(r, 1, 7)`.

`chance(r, p)` returns `true` with probability `p`, a number from 0 to 1. `0` is never and `1` is always. [chance.relic](../../examples/ref/random/chance.relic) flips 1,000 coins.

### Lists

`pick(r, xs)` is one item of a list. An empty list is `random.pick(): the list is empty`. [pick.relic](../../examples/ref/random/pick.relic) shows both.

`shuffle(r, xs)` is a new list with the items in a random order. `sample(r, xs, k)` is `k` items from different places, in the order drawn. Both leave `xs` as it is. [shuffle.relic](../../examples/ref/random/shuffle.relic) shuffles a hand and deals two.

### How it works

The generator is L'Ecuyer's MRG32k3a. Its period is about 2^191 and its arithmetic fits in an int, so it is plain Relic with no `host_*` call. Ints are drawn again when they land in the last partial block, so no result is favoured. It is not for secrets: someone who sees enough numbers can work out the next ones.
