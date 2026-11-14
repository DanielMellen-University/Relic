# tablecheck

Check a CSV or TSV export before importing it into a spreadsheet, ticket
tracker, or inventory system. `tablecheck` validates the header and the shape
of every row, accepts RFC 4180 quoted fields, optionally enforces a unique
key or nonblank required columns, and reports present, empty, and distinct
values for every column. It reads the input without changing it. A bad row
produces an error with its line and column, and no partial summary.

This is a standalone CLI written in Relic. Relic already supplies whole-file
reads (`std/files`), strings, lists, maps, arguments (`std/os`), stderr, and
exit codes. The compiler embeds the modules into C99 and links the C runtime.
No language extension or C++ host is needed.

## Quickstart

From the repository root, with a C99 compiler, CMake 3.16+, and a CMake build
tool (for example Make). One command configures, builds, and checks the sample:

```sh
cmake -S examples/tablecheck -B build-tablecheck -DCMAKE_BUILD_TYPE=Release && cmake --build build-tablecheck -j2 && ./build-tablecheck/tablecheck --key ticket examples/tablecheck/samples/tickets.csv
```

Input `examples/tablecheck/samples/tickets.csv`:

```csv
ticket,team,status,owner
INC-1042,platform,open,Sam
INC-1043,platform,closed,Lee
INC-1044,support,open,
INC-1045,support,pending,Sam
INC-1046,platform,open,Zoë
INC-1047,support,closed,
```

Output from that command (exit 0, empty stderr). The column header is
separated by tabs:

```text
rows: 6
columns: 4
column	present	empty	distinct
ticket	6	0	6
team	6	0	2
status	6	0	3
owner	4	2	3
```

The same build reads quoted fields. Input
`examples/tablecheck/samples/quoted.csv` (a comma and a quote inside one
field, and a line break inside another):

```csv
ticket,team,status,note
INC-1042,platform,open,"waiting on ""vendor"", then QA"
INC-1043,support,closed,"line one
line two"
```

```sh
./build-tablecheck/tablecheck --key ticket examples/tablecheck/samples/quoted.csv
```

Output from that command (exit 0, empty stderr):

```text
rows: 2
columns: 4
column	present	empty	distinct
ticket	2	0	2
team	2	0	2
status	2	0	2
note	2	0	2
```

`ctest --test-dir build-tablecheck --output-on-failure` runs the exact
stdout, stderr, and exit checks, including this sample.

On Windows the executable is `build-tablecheck\Release\tablecheck.exe` in a
multi-config generator. The checks themselves are part of the default CI
build on Linux, macOS, and Windows.

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

The check exits 0 and prints nothing. No source file, module directory, or
host executable is needed by that copied binary at runtime. The invalid
`RELIC_STD` setting checks that it does not load standard-library source.

## Usage and data rules

```text
tablecheck [--delimiter csv|tsv] [--key COLUMN] [--require COLUMN]... [--] FILE
tablecheck --help
```

- Default delimiter: comma. `tsv` selects a literal tab. There is no delimiter
  detection. Quote shell arguments containing spaces. Use `--` before a path
  starting with `-`. Options can precede or follow the path, until `--`.
- The first record is a header. Names must be nonempty and unique, with no
  surrounding whitespace, no tab, and no line break.
- Quoting follows RFC 4180. A field wrapped in double quotes may contain the
  delimiter and a line break (LF or CRLF). A quote inside a quoted field is
  two quotes (`""`), and the surrounding quotes are removed. Spaces inside a
  field are kept. A quote in an unquoted field, text after a closing quote,
  and a quote that is never closed are errors. The message points at the
  opening quote when a field is left unclosed.
- A CRLF or LF ends a record, including when it is inside a quoted field
  (there it becomes one newline in the value). A bare carriage return is an
  error. A file may omit the final newline. Blank lines are records, never
  silently skipped. A header-only file has zero rows. A trailing delimiter
  supplies an empty final field. Every row must have the header's field count.
- Error positions are 1-based and count Unicode code points. For a CRLF line
  break the column is the carriage return. Too few fields are reported at the
  line break that ended the record; too many are reported at the first extra
  field.
- A present value is anything other than the empty string. `""` is empty.
  Distinct counts exclude empty strings and compare exact strings: `01`, `1`,
  ` 1`, `1 `, `é`, and `É` stay distinct. Values are not trimmed or
  interpreted as numbers. A newline kept from a quoted field is part of the
  value.
- `--key COLUMN` requires that header and rejects empty or whitespace-only key
  values and exact duplicate keys, naming the first line that used a
  duplicate. Keys with surrounding spaces remain distinct. No key check is
  done by default.
- `--require COLUMN` requires that header and a nonblank value in every row.
  Repeat it for several columns, for example `--require team --require status`.
  Repeating the same name is harmless. Empty strings and values containing only
  spaces, tabs, CR or LF are blank. Duplicates are allowed and values are kept
  unchanged in the summary. A header-only file passes if the required names exist.
- A tab outside quotes is allowed only as the TSV delimiter. A tab inside a
  quoted field is kept. Other ASCII controls, DEL, and a leading UTF-8 BOM
  are rejected. UTF-8 text works, but this tool does not validate UTF-8
  encoding; Relic preserves invalid bytes as text.
- Output order follows the header, independent of map iteration order. The
  complete file and distinct-value sets are held in memory. Use it for exports
  that fit comfortably in memory; it is not a streaming or large-data tool.

| Exit | Meaning | stdout | stderr |
|---|---|---|---|
| 0 | Valid input or `--help` | Summary or help | Empty |
| 2 | Invalid command line | Empty | Usage error and help hint |
| 3 | Cannot read input or invalid data | Empty | Path, line and column for data errors, and a repair hint |

## Error example and exact checks

```sh
./build-tablecheck/tablecheck examples/tablecheck/samples/broken.csv > build-tablecheck/broken.out 2> build-tablecheck/broken.err
status=$?
test "$status" -eq 3 && test ! -s build-tablecheck/broken.out
cat build-tablecheck/broken.err
```

The check exits 0. `cat` prints this stderr, copied from that run:

```text
tablecheck: examples/tablecheck/samples/broken.csv: line 3, column 24: expected 4 fields, got 3; check the delimiter and supply empty trailing fields
```

`ctest` includes `tablecheck_goldens`. Its checked-in runner
`tests/check.cmake` compares stdout, stderr, and exit status for every case,
with `RELIC_LEAK_CHECK=1`. The sample stdout goldens are `tests/tickets.out`,
`tests/stock.out`, and `tests/quoted.out` (exit 0); help has `tests/help.out`.
Additional expectations are literal strings and statuses in the runner. It
generates isolated input fixtures in the build directory, including 250 rows
with long repeated values, quoted commas, escaped quotes, and a quoted field
that contains a line break. CRLF inputs are written with CMake `configure_file`
`NEWLINE_STYLE CRLF` from LF sources, so Windows text mode cannot turn one
carriage return into two. The runner rejects a fixture that does not contain a
single CRLF or that contains `CR CR`.

It checks errors after valid rows, exact values, line endings, blank and
trailing fields, header-only input, paths and key names containing spaces,
`--`, malformed quotes, controls, missing files and folders, and
invalid or repeated options.

Two intentional source mistakes exercise the compiler and runtime diagnostics:

- `tests/syntax_error.relic` has `if` without a condition. `relicc` exits 1:
  `expected an expression, got end of line`, with line and caret. Add a bool
  condition and an indented body to repair it. It is never compiled with `--stub`.
- `tests/type_error.relic` passes `42` as the text to `table.summarize`.
  Compilation succeeds; running the binary exits 1:
  `summarize() argument 'text' must be string, got int`. Pass file contents as a
  string. Relic checks type hints at runtime, not statically.

Their exact stderr is in the adjacent `.err` files; both require empty stdout.
Only the absolute checkout prefix in the runtime type diagnostic is stripped
for comparison. Output CRLF is normalized as in Relic's existing golden runner.

The top-level Relic build also builds this application and runs the same checks.
Its executable is `build/examples/tablecheck/tablecheck` for a build called
`build`. The standalone command above is enough to use the tool.

These automated instructions and agent-run checks do not verify Relic's
separate acceptance gate: an unfamiliar human writing and running FizzBuzz
using only the repository README. That gate remains unverified.
