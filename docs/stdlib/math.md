# std/math

Rounding, roots, angles and whole-number helpers. `use std/math`.

`pi` and `e` are float constants. Printing them shows 15 digits: `3.14159265358979` and `2.71828182845905`. [constants.relic](../../examples/ref/math/constants.relic) prints both, and `cos(pi)`, which is `-1.0`.

## std/math

### Size and comparison

`abs` is never negative. `min` and `max` use `<`, so they take anything that compares, including strings. `clamp(x, lo, hi)` is `min(max(x, lo), hi)`. `sign` is `-1`, `0` or `1`.

`abs(-3)` → `3`

`abs(2.5)` → `2.5`

`min(3, 1)` → `1`

`min("b", "a")` → `"a"`

`max(3, 1)` → `3`

`clamp(-5, 0, 10)` → `0`

`clamp(15, 0, 10)` → `10`

`sign(-4)` → `-1`

`sign(0)` → `0`

`sign(0.5)` → `1`

### Rounding

`floor` goes down, `ceil` goes up, `round` takes halves up (`round(-2.5)` is `-2`), and `trunc` drops the fraction toward 0. An int comes back as an int. nan, an infinity, or a float past the int range is an error.

`floor(2.9)` → `2`

`floor(-2.1)` → `-3`

`ceil(2.1)` → `3`

`ceil(-2.1)` → `-2`

`round(2.5)` → `3`

`round(-2.5)` → `-2`

`trunc(-2.7)` → `-2`

`trunc(2.7)` → `2`

`is_whole` is true for an int, or a float with nothing after the point.

`is_whole(2)` → `true`

`is_whole(2.0)` → `true`

`is_whole(2.5)` → `false`

### Roots, powers, angles

`sqrt`, `pow`, `sin`, `cos`, `exp` and `log` return floats. Outside the domain of `sqrt` and `log` the result is nan or an infinity, not an error. `sin` and `cos` take radians. `radians` and `degrees` convert.

`sqrt(4)` → `2.0`

`sqrt(0)` → `0.0`

`sqrt(2)` → `1.4142135623731`

`pow(2, 10)` → `1024.0`

`pow(2, -1)` → `0.5`

`pow(3, 2)` → `9.0`

`sin(0)` → `0.0`

`cos(0)` → `1.0`

`cos(math.pi)` → `-1.0`

`exp(0)` → `1.0`

`log(1)` → `0.0`

`radians(0)` → `0.0`

`radians(180)` → `3.14159265358979`

`degrees(0)` → `0.0`

`degrees(math.pi)` → `180.0`

### Whole numbers and distance

`gcd` and `lcm` want ints and are never negative. `gcd(0, 0)` is 0. `lcm` is 0 when either argument is 0. `is_even` and `is_odd` want ints. `lerp(a, b, t)` is `a + (b - a) * t` and is not clamped. `hypot(x, y)` is the distance from the origin. `dist` is the distance between two points.

`gcd(12, 8)` → `4`

`gcd(-12, 8)` → `4`

`gcd(0, 0)` → `0`

`lcm(4, 6)` → `12`

`lcm(0, 5)` → `0`

`is_even(4)` → `true`

`is_even(-3)` → `false`

`is_odd(-3)` → `true`

`lerp(0, 10, 0)` → `0`

`lerp(0, 10, 1)` → `10`

`lerp(0, 10, 0.5)` → `5.0`

`hypot(3, 4)` → `5.0`

`dist(0, 0, 3, 4)` → `5.0`

`dist(1, 1, 1, 1)` → `0.0`
