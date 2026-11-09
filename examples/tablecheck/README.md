# tablecheck

Check an unquoted CSV or TSV export before importing it into a spreadsheet,
ticket tracker or inventory system. `tablecheck` validates its shape and header,
optionally enforces a unique key, and reports present, empty and distinct values
for every column. It reads the input without changing it. A bad row produces an
actionable error and no partial summary.

This is a standalone CLI written in Relic. Relic already supplies whole-file
reads (`std/files`), line and delimiter splitting (`std/strings`), lists for
columns, maps for distinct values and key locations, arguments (`std/os`),
stderr and exit codes. The existing compiler embeds the modules into C99 and
links the existing C runtime. No language extension or C++ host is needed.

## Clean build and run

Use a POSIX shell at the Relic checkout root, with a C99 compiler, CMake 3.16+
and a CMake build tool (for example Make). No download or install step is used.
Start with no `build-tablecheck` directory. These instructions build the compiler
from committed bootstrap C and this checkout's Relic sources, then compile the
application and a deliberate type-error test executable. They do not depend on
another build directory. `-j2` bounds concurrent compilation.

```sh
cmake -S examples/tablecheck -B build-tablecheck -DCMAKE_BUILD_TYPE=Release
cmake --build build-tablecheck -j2
ctest --test-dir build-tablecheck --output-on-failure
./build-tablecheck/tablecheck --key ticket examples/tablecheck/samples/tickets.csv
```

Expected stdout (the column table uses literal tabs), exit **0**, empty stderr:

```text
rows: 6
columns: 4
column	present	empty	distinct
ticket	6	0	6
team	6	0	2
status	6	0	3
owner	4	2	3
```

```sh
./build-tablecheck/tablecheck --delimiter tsv --key sku examples/tablecheck/samples/stock.tsv
```

Expected stdout, exit **0**, empty stderr:

```text
rows: 4
columns: 3
column	present	empty	distinct
sku	4	0	4
warehouse	4	0	2
bin	3	1	2
```

The executable contains the Relic modules and the runtime. To copy just the
binary into a fresh run folder and check its stdout and exit status:

```sh
mkdir -p build-tablecheck/run
cp build-tablecheck/tablecheck build-tablecheck/run/
cp examples/tablecheck/samples/tickets.csv build-tablecheck/run/
cp examples/tablecheck/tests/tickets.out build-tablecheck/run/expected.out
(
    cd build-tablecheck/run
    RELIC_STD=/nonexistent RELIC_LEAK_CHECK=1 ./tablecheck --key ticket tickets.csv > actual.out 2> actual.err
    status=$?
    test "$status" -eq 0 && test ! -s actual.err && cmp expected.out actual.out
)
```

The check exits **0** and prints nothing. No source file, module directory or
host executable is needed by that copied binary at runtime. The invalid
`RELIC_STD` setting checks that it does not load standard-library source.

## Usage and data rules

```text
tablecheck [--delimiter csv|tsv] [--key COLUMN] [--] FILE
tablecheck --help
```

- Default delimiter: comma. `tsv` selects a literal tab. There is no delimiter
  detection. Quote shell arguments containing spaces. Use `--` before a path
  starting with `-`. Options can precede or follow the path, until `--`.
- The first physical line is a header. Names must be nonempty, unique, have no
  surrounding whitespace, and contain no quotes or control characters.
- This is **unquoted delimited text**, not an RFC 4180 CSV parser. All double
  quotes are rejected, including escaped quotes. Fields cannot contain the
  delimiter or newlines. Use an exporter configured for this format. Changing
  delimiters does not make embedded newlines valid.
- LF and CRLF line endings are accepted, with or without a final newline. Blank
  lines are records, never silently skipped. A header-only file has zero rows.
  A trailing delimiter supplies an empty final field. Every row must have the
  header's field count.
- A present value is anything other than the empty string. Distinct counts
  exclude empty strings and compare exact strings: `01`, `1`, ` 1`, `1 `, `é`
  and `É` stay distinct. Values are not trimmed or interpreted as numbers.
- `--key COLUMN` requires that header and rejects empty or whitespace-only key
  values and exact duplicate keys, naming the first line that used a duplicate.
  Keys with surrounding spaces remain distinct. No key check is done by default.
- Tabs are allowed only as the TSV delimiter. Other ASCII control characters,
  DEL, a bare CR, and a leading UTF-8 BOM are rejected. UTF-8 text works, but this
  tool does not validate UTF-8 encoding; Relic preserves invalid bytes as text.
- Output order follows the header, independent of map iteration order. The
  complete file and distinct-value sets are held in memory. Use it for exports
  that fit comfortably in memory; it is not a streaming or large-data tool.

| Exit | Meaning | stdout | stderr |
|---|---|---|---|
| 0 | Valid input or `--help` | Summary or help | Empty |
| 2 | Invalid command line | Empty | Usage error and help hint |
| 3 | Cannot read input or invalid data | Empty | Path, line for data errors, and repair hint |

## Error example and exact checks

```sh
./build-tablecheck/tablecheck examples/tablecheck/samples/broken.csv > build-tablecheck/broken.out 2> build-tablecheck/broken.err
status=$?
test "$status" -eq 3 && test ! -s build-tablecheck/broken.out
cat build-tablecheck/broken.err
```

The check exits **0**; `cat` prints this stderr diagnostic from the application:

```text
tablecheck: examples/tablecheck/samples/broken.csv: line 3: expected 4 fields, got 3; check the delimiter and supply empty trailing fields
```

`ctest` includes `tablecheck_goldens`. Its checked-in runner
`tests/check.cmake` compares **stdout, stderr and exit status** for every case,
with `RELIC_LEAK_CHECK=1`. The two sample stdout goldens are `tests/tickets.out`
and `tests/stock.out` (exit 0); help has `tests/help.out`. Additional expectations
are literal strings and statuses in the runner. It generates isolated input
fixtures in the build directory, including 250 rows with long repeated values.
It checks errors after valid rows, exact values, line endings, blank and trailing
fields, header-only input, paths and key names containing spaces, `--`, malformed
data, controls, missing files and folders, and invalid/repeated options.

Two intentional source mistakes exercise the compiler and runtime diagnostics:

- `tests/syntax_error.relic` has `if` without a condition. `relicc` exits **1**:
  `expected an expression, got end of line`, with line and caret. Add a bool
  condition and an indented body to repair it. It is never compiled with `--stub`.
- `tests/type_error.relic` passes `42` as the text to `table.summarize`.
  Compilation succeeds; running the binary exits **1**:
  `summarize() argument 'text' must be string, got int`. Pass file contents as a
  string. Relic checks type hints at runtime, not statically.

Their exact stderr is in the adjacent `.err` files; both require empty stdout.
Only the absolute checkout prefix in the runtime type diagnostic is stripped
for comparison. Output CRLF is normalized as in Relic's existing golden runner.

The top-level Relic build also builds this application and runs the same checks.
Its executable is `build/examples/tablecheck/tablecheck` for a build called
`build`. The standalone commands above are sufficient for using this tool.

These automated instructions and agent-run checks do **not** verify Relic's
separate acceptance gate: an unfamiliar human writing and running FizzBuzz
using only the repository README. That gate remains **unverified**.
