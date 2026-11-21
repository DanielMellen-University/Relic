# std/csv

Comma-separated tables in and out, as RFC 4180 has them. `use std/csv`.

A field may be wrapped in double quotes. Inside them a quote is written twice, and the separator and line breaks are plain text. Lines end in LF or CRLF. `sep` is one character and defaults to `","`.

## std/csv

### Reading

`parse(text, sep)` gives the rows, each a list of strings. A line break after the last row adds no row. [parse.relic](../../examples/ref/csv/parse.relic) reads quoted fields.

`parse("a,b\n1,2\n")` → `[["a", "b"], ["1", "2"]]`

`parse("x;\"y;z\"", ";")` → `[["x", "y;z"]]`

`parse("")` → `[]`

`records(text, sep)` gives the rows after the first as maps keyed by the first row's names. Empty lines are skipped. Every row must have as many fields as the header. [records.relic](../../examples/ref/csv/records.relic) prints one line per person.

`records("name,age\nAda,36\n")` → `[{name: "Ada", age: "36"}]`

Bad text is an error with the line and column, such as `csv.parse(): line 1, column 2: a quote inside an unquoted field; quote the whole field and write the quote twice`.

### Writing

`write(rows, sep)` joins each row's fields with `sep` and ends every row with `"\n"`. Only fields that need quotes get them: ones holding `sep`, a quote, a CR or a LF, or starting or ending with a space. Numbers and bools are written as `str()` shows them, and `none` as an empty field. [write.relic](../../examples/ref/csv/write.relic) writes a small table and reads it back.

So `write([["a", "b,c"]])` is the text `a,"b,c"` and a line break.
