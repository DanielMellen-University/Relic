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

### Dates

Calendar dates in UTC, for years 1 to 9999, with no leap seconds. `secs` counts from 1970 the way `now()` does. A float's fraction is dropped, rounding down. Local time zones are not covered.

`date(secs)` is a map `{year, month, day, hour, minute, second, weekday, yday}`. `weekday` is 1 for Monday to 7 for Sunday, and `yday` is the day of the year from 1. [date.relic](../../examples/ref/time/date.relic) prints the parts of one moment.

`from_date(year, month, day, hour, minute, second)` goes back to seconds. The time parts default to 0. A part out of range is an error that names it, like `time.from_date(): day must be from 1 to 28 in 2023-02, got 29`.

`from_date(1970, 1, 1)` → `0`

`from_date(2000, 2, 29)` → `951782400`

`iso(secs)` writes ISO 8601 in UTC, and `parse_iso(text)` reads it back. `parse_iso` takes `"YYYY-MM-DDTHH:MM:SSZ"` or a date alone, which is midnight. [iso.relic](../../examples/ref/time/iso.relic) reads one back and moves it a week on.

`iso(0)` → `"1970-01-01T00:00:00Z"`

`iso(-1)` → `"1969-12-31T23:59:59Z"`

`parse_iso("1969-12-31")` → `-86400`

`parse_iso("2026-11-21T18:01:00Z")` → `1795284060`

`is_leap(year)` follows the Gregorian rule, and `days_in_month(year, month)` is 28 to 31.

`is_leap(1900)` → `false`

`is_leap(2024)` → `true`

`days_in_month(2023, 2)` → `28`

### add_days

`add_days(secs, days)` moves a UTC timestamp by whole days. `days` may be negative. A result outside the years 1 to 9999 is an error.

`add_days(0, 1)` → `86400`

`add_days(86400, -1)` → `0`

### days_between

`days_between(a, b)` is the UTC day number of `b` minus the day number of `a`. A time of day does not count, and a later `a` gives a negative result.

`days_between(0, 86400)` → `1`

`days_between(3600, 86399)` → `0`

`days_between(86400, 0)` → `-1`

### start_of_day

`start_of_day(secs)` is midnight at the start of the UTC day that holds `secs`.

`start_of_day(86400 + 5)` → `86400`

`start_of_day(100)` → `0`
