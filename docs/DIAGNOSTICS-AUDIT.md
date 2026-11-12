# Diagnostics audit

Local audit of `cf2d793` and the fixes on `feat/diagnostics-standalone-20261111`.
No SDL build or network checks. Programs and their temporary C output were put
under `/tmp/relic-diagnostics-audit`, outside the source tree.

## Method

For each small source, compare the exit status, stdout and stderr from:

- `build/relic FILE`
- `build/relic --tree FILE`
- `build/relic selfhost/run.relic FILE`
- `build/relicc FILE -o /tmp/relic-diagnostics-audit/probe.c --stub`, then
  `cc -std=c99 -I runtime/c /tmp/relic-diagnostics-audit/probe.c build/runtime/c/librelic_rt.a -lm -o /tmp/relic-diagnostics-audit/probe`

`--stub` checks syntax errors through a compiled error stub. Runtime probes
run generated code. Compiling a syntax error directly was also checked through
`relicc_cli`. Temporary C files and executables are removed after probing.

## Initial results

44 ordinary probes agreed across all four paths, including exact positions and
messages. Agreement alone does not establish that a diagnostic is helpful.

| Class | Probes |
|---|---|
| Lexer | unknown and Unicode escapes, unterminated plain and interpolated strings, malformed exponents, large integers, stray non-ASCII characters, tabs, unclosed and mismatched brackets |
| Parser | missing name, missing body, indentation, expression cut off at EOF, multiline calls and indexes |
| Names and modules | undefined name, missing module, private name, error in a module, module closure, parameter hint at a module call |
| Runtime | mixed types, list index and map key errors, invalid index and key types, assignment key type, division by zero, function and built-in arity |
| Unwinding | error in a closure, return expression, error raised in a catch, a second error after a catch, catch around a module call |
| Standard library | argument type and arity, error in a user callback invoked by std code |
| Positions | UTF-8 before an error, tabs inside string text, multiline unary operand, interpolation expression, multiline hinted call |

Additional boundary probes found these defects:

| Defect | Initial behavior |
|---|---|
| GCC 11 host build | `Map` instantiates its hash table before `Value` is complete; the host does not build. Fixed by defining `Map` after `Value`. `map_value_types` covers its value kinds. |
| Unterminated interpolated string | Fixed: retain the opening quote separately from segment token positions, including nested strings and later interpolations. |
| Brackets inside interpolation | Fixed: every nested bracket is checked, and closers cannot cross the interpolation boundary. |
| Malformed exponent | `1e+` suggests separating a name from a number instead of completing the exponent. |
| Malformed UTF-8 | A lone continuation byte inside a string costs no column on the host, but one on the self-hosted path. Snippet padding also skips that byte. |
| Module parse retry | Fixed: the VM now keeps a module registered after a caught syntax error, as the other three paths already did. |
| Uncaught NUL message | Fixed: compiled error reporting writes the full message by length; the golden runners compare raw stderr bytes too. |
| Module syntax notes | Lex and parse errors loaded with `use` lose their hint and underline span. |

These rows are observations, not new language rules. Each behavior change needs
its own regression and the corresponding language or CLI documentation.

## Unreadable command-line input

Follow-up to the frozen `e768961` baseline. Permission probes found that the
self-hosted command drivers and `relicc` reported unreadable input as errors
inside their own source, with exit 1. The regression fixture uses chmod 000 on
a path with spaces and restores its permissions before checking results. It
skips with a reason where Unix permissions cannot deny reads, including root
and Windows.

| Tool fixed | Verified result | Regression |
|---|---|---|
| `selfhost/run.relic` | exit 2, empty stdout, `relic: cannot open FILE`; host VM and tree-walker | `selfhost_run_unreadable`, failed before the fix and passed after it |
