# std/time

Clocks, a short wait, and a frame timer. Times are seconds as floats, except `sleep`, which takes milliseconds. `use std/time`.

The numbers change every run, so the checked programs print a fact about the value rather than the value itself.

## std/time

`now()` is UTC seconds since 1970. [now.relic](../../examples/ref/time/now.relic) prints `true` when that is a positive float.

`clock()` is a steady clock. Only differences mean anything. `since(start)` is `clock() - start`. `start` must be a number.

[since.relic](../../examples/ref/time/since.relic) prints `true` when the gap is a float and is not negative.

`sleep(ms)` waits. Zero returns at once. [sleep.relic](../../examples/ref/time/sleep.relic) prints `awake`.

### Frame timer

`timer()` returns a map `{last, fps, frames, window}`. `tick(t)` returns seconds since the last tick and updates the map. `t.fps` stays `0` until about a second of frames has been collected. `limit(t, fps)` sleeps so frames run no faster than `fps`, which must be a number above 0. Call `tick`, then draw, then `limit`.

`t` must be a map from `timer()`. [timer.relic](../../examples/ref/time/timer.relic) ticks once and prints `true`, `1`, `true`, and `["last", "fps", "frames", "window"]`.
