# std/result

A value that is either a success or a failure, so a caller can pass the failure on without `try` / `catch`. `ok(value)` holds a value. `err(message)` holds a string. `kind` is `"ok"` or `"err"`. `use std/result`.

## std/result


### ok

`ok(value)` is a success holding `value`. `value` may be anything, including `none`.

`ok(1)` → `{kind: "ok", value: 1}`

### err

`err(message)` is a failure. `message` must be a string, the same rule `fail` has.

`err("no")` → `{kind: "err", message: "no"}`

### is_ok

`is_ok(r)` is true for a result from `ok`.

`is_ok(result.ok(1))` → `true`

`is_ok(result.err("no"))` → `false`

### is_err

`is_err(r)` is true for a result from `err`.

`is_err(result.err("no"))` → `true`

`is_err(result.ok(1))` → `false`
