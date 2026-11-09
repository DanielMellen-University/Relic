# Useful CLI application review record, 2026-11-08

## Result and scope

Built **tablecheck**, one standalone Relic CLI for validating unquoted CSV/TSV
exports and reporting per-column present, empty and distinct counts. An optional
`--key COLUMN` rejects blank or repeated keys and identifies the first duplicate
line. It validates all input before printing a summary and never writes input.

Observed: the default C-only suite passed **263/263**, the existing consumer
suite passed **6/6**, and a fresh application consumer build following the new
README passed **6/6**. Its application case performs **49 exact stdout, stderr
and exit-status checks** with `RELIC_LEAK_CHECK=1`. Both realistic samples and a
copied standalone binary passed. No compiler, runtime, bootstrap or stdlib fix
was required or made.

The baseline C++ host build failed on the installed GCC 11/libstdc++ before
running tests. Host parity remains unverified in this environment. The separate
**unfamiliar-human README-only FizzBuzz acceptance gate remains unverified**.
An agent executing these application instructions does not satisfy that gate.

All changes are uncommitted and unstaged. No commit, push, PR, merge, tag,
release, publication, purchase, dependency installation, other coding agent or
coding batch was created. No other checkout, held candidate or application
source was inspected or modified. No alternate compiler/model, approval bypass,
permission expansion or chmod workaround was used after the host failure.

## Checkout, instructions and tools

- Working checkout: `/home/daniel/codex-runs/Relic-useful-app-20261108`.
- Branch at start and finish: `app/useful-cli-20261108`.
- Clean starting HEAD and unchanged finishing HEAD:
  `4ad4ec65357a28db99eeacf16117dfd39293dd42` (Relic 1.0.53).
- Initial `git status --short` was empty. No repository `AGENTS.md` was found;
  the supplied global instructions applied. Read the root README,
  CONTRIBUTING.md and the architecture, language, stdlib, CLI, testing, host and
  C-generation documentation, then inspected relevant stdlib, build and test
  source. The explicit no-commit/task-branch instructions override the
  contributor document's ordinary commit-to-main workflow.
- Intended identity was supplied process-locally using
  `git -c user.name='Daniel Mellen' -c user.email='113362200+DanielMellen-University@users.noreply.github.com' ...`.
  `git ... var GIT_AUTHOR_IDENT` confirmed that identity. No persistent Git
  configuration was changed. Identity was never used to commit.
- CMake **3.22.1**, `cc` and `c++` **Ubuntu GCC 11.4.0**, POSIX bash/Make.
  Python **3.10.12** was used for a documentation edit, not as an application
  dependency. C++ is installed here; this run does not prove operation on a
  machine with no C++ compiler installed.

Unchanged source pins (SHA-256, in addition to the Git HEAD above):

```text
d2b963a973f9937864b8cfc84b230985225a4407d86566c676696b4a9413b67d  bootstrap/cgen.c
bdfc4e4945fa08a6a9700faf168de54fa336fa8d8fd17f136112911372715468  selfhost/cgen.relic
670c101147eb7255f1c2079a4367bcc893dd4cc1d5b11b5f4478fbb2af3ed5af  std/files.relic
6822599c226d2bd96341030ac2f61178d1e7ca9203cf9941475dcfc13f92174f  std/strings.relic
fffa65fabdfbb790d5d38b3b19fd7990ed308e2515a455c38522b3bd4155f23a  std/os.relic
```

New implementation/instruction pins (uncommitted working files):

```text
1477a1f4fee682e048d8fd1364fa02336ebed085931b38be8ad7aa192ede777e  examples/tablecheck/main.relic
368bca0b80d4b0f9d658792c9254ad9c223436ff8322933b886e844f8b8b757a  examples/tablecheck/table.relic
e72894b6cf388d832229f4e5caae85677e90d54a8e8dfe9bdc00e5ba478eada6  examples/tablecheck/CMakeLists.txt
00379269413f233db488c56c1e00813679274823f4f5b5a17f5754f1e07fea30  examples/tablecheck/README.md
61aa0a7470cdfbb5e2a0b5840055bd1888effe561a8287e6f0c691ce3576ec72  examples/tablecheck/tests/check.cmake
```

## Why this application and why Relic supports it now

A ticket or stock export can have short rows, unnamed columns, missing owners
or duplicated identifiers while still looking plausible on screen. This tool
finds structural problems before import and makes missing-value and category
counts visible without numeric coercion. Samples use incident tickets, teams,
statuses and owners, and stock SKUs, warehouses and bin locations.

The actual sources already provide `files.read`, `strings.lines` (LF/CRLF and
final-line handling), `strings.split`, `strings.trim`, UTF-8 character iteration,
mutable lists and maps, `os.args`, `eprint`, `try/catch`, type hints and
`os.exit`. The application uses these directly. `relicc` compiles it and its
modules into one C99 file; `relic_add_program` links the existing C runtime.
The executable runs without a Relic interpreter or stdlib source at runtime.

The scope is intentionally unquoted delimited text. Quotes, ASCII controls
other than TSV separators, DEL and a leading BOM get repair hints. Quoted CSV,
embedded delimiters/newlines, encoding validation and streaming are not
implemented. The README states these limits. Whole-file text and distinct-value
maps must fit memory. Counts compare exact strings and exclude only empty
strings, while a key must also be non-whitespace. This avoids silently treating
`01` as `1` or dropping blank records.

## Baseline before source changes

All commands below ran from the checkout root. Initial baseline logs were
redirected to `/tmp/relic-useful-baseline-*.log`, then copied into the ignored
workspace directory `build-useful-evidence/`. Those temporary log writes are a
deviation from the requested workspace-only boundary; source and build work
stayed in this checkout. Subsequent evidence was written inside the checkout.

Exact baseline build/test commands (each chain exits at its first failure):

```sh
cmake -S . -B build-useful-baseline-default -DCMAKE_BUILD_TYPE=Release > /tmp/relic-useful-baseline-configure.log 2>&1 && cmake --build build-useful-baseline-default -j4 > /tmp/relic-useful-baseline-build.log 2>&1 && ctest --test-dir build-useful-baseline-default --output-on-failure -j4 > /tmp/relic-useful-baseline-ctest.log 2>&1
cmake -S . -B build-useful-baseline-host -DCMAKE_BUILD_TYPE=Release -DRELIC_BUILD_HOST=ON > /tmp/relic-useful-baseline-host-configure.log 2>&1 && cmake --build build-useful-baseline-host -j4 > /tmp/relic-useful-baseline-host-build.log 2>&1 && ctest --test-dir build-useful-baseline-host --output-on-failure -j4 > /tmp/relic-useful-baseline-host-ctest.log 2>&1
cmake -S tests/consumer -B build-useful-baseline-consumer -DCMAKE_BUILD_TYPE=Release > /tmp/relic-useful-baseline-consumer-configure.log 2>&1 && cmake --build build-useful-baseline-consumer -j2 > /tmp/relic-useful-baseline-consumer-build.log 2>&1 && ctest --test-dir build-useful-baseline-consumer --output-on-failure > /tmp/relic-useful-baseline-consumer-ctest.log 2>&1
```

| Baseline | Configure | Build | Tests |
|---|---|---|---|
| Default C-only | Exit 0 | Exit 0 | 262 passed, 0 failed, 4.00 s, exit 0 |
| Existing consumer | Exit 0 | Exit 0 | 6 passed, 0 failed, 2.19 s, exit 0 |
| C++ host | Exit 0 | Exit 2 | Not run; build failed |

Host failure excerpt:

```text
/usr/include/c++/11/bits/stl_pair.h:218:11: error: 'std::pair<_T1, _T2>::second' has incomplete type
```

The instantiation trace names `src/interp/value.hpp:73:44` (the map's
`std::unordered_map<std::string, Value>`) and the forward declaration
`src/interp/value.hpp:15:8`. Both `core.cpp` and `lexer.cpp` failed compilation.
This is observed on the clean pinned base, before the application existed.
It was not repaired, retried with a different toolchain, or used to justify
unrelated host changes. There is no baseline host ctest log because the `&&`
chain correctly stopped before ctest.

## Compiled application probes and final repository checks

Before source changes, the baseline checks above finished. After adding the
application, these direct compiler/linker probes ran without `--stub`:

```sh
mkdir -p build-useful-probe
./build-useful-baseline-default/relicc examples/tablecheck/main.relic -o build-useful-probe/tablecheck.c && cc -std=c99 -O2 -Wall -Wextra -pedantic -Werror -I runtime/c build-useful-probe/tablecheck.c build-useful-baseline-default/runtime/c/librelic_rt.a -lm -o build-useful-probe/tablecheck && ./build-useful-probe/tablecheck --key ticket examples/tablecheck/samples/tickets.csv
./build-useful-baseline-default/relicc examples/tablecheck/tests/type_error.relic -o build-useful-probe/type_error.c && cc -std=c99 -O2 -Wall -Wextra -pedantic -Werror -I runtime/c build-useful-probe/type_error.c build-useful-baseline-default/runtime/c/librelic_rt.a -lm -o build-useful-probe/type_error && ./build-useful-probe/type_error
./build-useful-baseline-default/relicc examples/tablecheck/tests/syntax_error.relic -o build-useful-probe/syntax_error.c
```

Results: the application compiled and linked warning-free with `-Werror` and
printed `tests/tickets.out`, exit 0. The type fixture compiled and linked
warning-free, then exited 1 with the diagnostic below. The syntax compilation
exited 1 with the diagnostic below. These probes exercised the actual compiler,
generated C and runtime, not only a host interpreter.

Final default and existing-consumer checks reused their baseline build trees;
the independent README build below started with an absent build directory:

```sh
cmake -S . -B build-useful-baseline-default -DCMAKE_BUILD_TYPE=Release > build-useful-evidence/final-default-configure.log 2>&1 && cmake --build build-useful-baseline-default -j4 > build-useful-evidence/final-default-build.log 2>&1 && ctest --test-dir build-useful-baseline-default --output-on-failure -j4 > build-useful-evidence/final-default-ctest.log 2>&1
cmake -S tests/consumer -B build-useful-baseline-consumer -DCMAKE_BUILD_TYPE=Release > build-useful-evidence/final-consumer-configure.log 2>&1 && cmake --build build-useful-baseline-consumer -j2 > build-useful-evidence/final-consumer-build.log 2>&1 && ctest --test-dir build-useful-baseline-consumer --output-on-failure > build-useful-evidence/final-consumer-ctest.log 2>&1
ctest --test-dir build-useful-baseline-default -R '^tablecheck_goldens$' -V > build-useful-evidence/tablecheck-verbose.log 2>&1
```

| Check | Final observed result | Change from baseline |
|---|---|---|
| Default | Configure/build exit 0; 263 passed, 0 failed, 2.70 s, exit 0 | One application ctest added |
| Existing consumer | Configure/build exit 0; 6 passed, 0 failed, 0.87 s, exit 0 | Same test count |
| Application verbose | 49 exact stdout/stderr/exit checks passed, exit 0 | New coverage |
| C++ host | Not rerun after the baseline failure | Still blocked/unverified |

The default suite includes `bootstrap_fresh`, `bootstrap_fixed_point`,
`c_runtime`, `c_demo`, `depth_limit`, `doc_counts`, `example_compiled` and all
251 existing compiled goldens plus the compiled-only cases. None were skipped
or failed. No warning/error matches were found in the final default, consumer
or clean application build logs. `git diff --check` passed.

The updated TESTING.md describes 263 default, 550 host and 566 full SDL test
registrations. Only 263 default tests were actually run here; the host/SDL
numbers are the existing documented counts plus the one added application
case, not new successful host/SDL or CI results. Historical acceptance records
were left intact. Language golden-corpus counts did not change.

## Fresh README-only application run evidence

Followed **examples/tablecheck/README.md only** for the application build and
run. `test ! -e build-tablecheck` returned 0 immediately before configuration.
The application project adds this source checkout as a C-only consumer; it
builds its own bootstrap compiler, stage-1 compiler and runtime. No artifact
from the baseline build trees is referenced.

The README's exact build/run lines were executed, with log redirection added:

```sh
cmake -S examples/tablecheck -B build-tablecheck -DCMAKE_BUILD_TYPE=Release
cmake --build build-tablecheck -j2
ctest --test-dir build-tablecheck --output-on-failure
./build-tablecheck/tablecheck --key ticket examples/tablecheck/samples/tickets.csv
./build-tablecheck/tablecheck --delimiter tsv --key sku examples/tablecheck/samples/stock.tsv
```

Configure and build exited 0. ctest: **6 passed, 0 failed, 1.45 s**, exit 0:
`tablecheck_goldens`, `doc_counts`, `bootstrap_fresh`, `bootstrap_fixed_point`,
`c_runtime`, `c_demo`. A verbose application-only run again reported 49 checks.
CSV and TSV invocations both exited 0 with empty stderr; `cmp` against the
checked-in stdout goldens exited 0.

Also executed the README's complete copy-and-run block, verbatim:

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

The subshell check exited 0 and printed nothing. The copied binary exited 0,
stderr was empty, stdout matched, and the invalid stdlib path did not prevent
execution. That folder contains the binary, input, expected output and captured
output, with no Relic source or stdlib directory.

Executed the README's broken-input block too: application exit 3, zero-byte
stdout, and the documented diagnostic below; the shell status/empty-output
check and the `cat` command each exited 0. Evidence is in
`build-tablecheck/broken.out` and `broken.err`.

Local ignored evidence files: `build-useful-evidence/readme-configure.log`,
`readme-build.log`, `readme-ctest.log`, `readme-app-verbose.log`,
`readme-tickets.out`, `readme-tickets.err`, `readme-stock.out`, `readme-stock.err`,
`app-sha256.txt`, plus the baseline/final logs named above. They remain available
for local review; the checked-in fixtures and this report carry the reproducible
expectations. This was an agent-executed README run, not a human acceptance run.

## Deterministic goldens and negative diagnostics

CSV stdout, exit **0**, empty stderr (`tests/tickets.out`; literal tabs):

```text
rows: 6
columns: 4
column	present	empty	distinct
ticket	6	0	6
team	6	0	2
status	6	0	3
owner	4	2	3
```

TSV stdout, exit **0**, empty stderr (`tests/stock.out`):

```text
rows: 4
columns: 3
column	present	empty	distinct
sku	4	0	4
warehouse	4	0	2
bin	3	1	2
```

Broken sample stderr, exit **3**, empty stdout:

```text
tablecheck: examples/tablecheck/samples/broken.csv: line 3: expected 4 fields, got 3; check the delimiter and supply empty trailing fields
```

Representative generated-fixture diagnostics, exact stderr, empty stdout:

```text
tablecheck: duplicate.csv: line 4: duplicate key 'id' value '1' (first seen on line 2); make keys unique
tablecheck: blank_key.csv: line 3: key 'id' is blank; supply a nonblank unique value
tablecheck: quoted.csv: line 2: quoted fields are unsupported; export unquoted CSV/TSV without embedded delimiters or newlines
tablecheck: cannot read 'missing.csv'; check that it is a readable file (files.read(): cannot open missing.csv)
```

Each is a separate invocation with exit **3**. Usage errors have exit **2**, for
example no arguments gives exactly:

```text
tablecheck: expected exactly one input file, got 0; run tablecheck --help
```

Syntax fixture, compile-time exit **1**, empty stdout (from the application
directory, as the test runner invokes it):

```text
tests/syntax_error.relic:2:3: error: expected an expression, got end of line
    if
      ^
```

Repair: provide a bool condition and an indented body. No `--stub` is used.

Type fixture compiles successfully, then runs with exit **1**, empty stdout:

```text
tests/type_error.relic:3:16: error: summarize() argument 'text' must be string, got int
    table.summarize(42, ",", "")
                   ^
```

Repair: pass the input text string rather than a row-count integer. Type hints
are runtime checks in this language. The test strips only the absolute
application-directory prefix from this diagnostic. CRLF output normalization
matches the repository's existing golden convention. Direct root-directory
probes above show the same messages with `examples/tablecheck/` before `tests/`.

The 49 checks cover 47 application runs and the two source-error cases. Inputs
cover ordinary CSV/TSV, unique-key success, help, paths and key names with spaces,
`--` and dash filenames, CRLF, absent final newline, header-only input, trailing
empty fields, Unicode, exact string identity, one column, 250 long-value rows,
empty/blank/duplicate/spaced headers, short/long/blank rows, quotes in data and
headers, tabs in CSV, bare CR, DEL, BOM, absent/blank/whitespace/duplicate keys,
duplicate values allowed without `--key`, missing files, folders, and
missing/unknown/repeated command-line arguments. Errors after valid rows must
still have empty stdout. The help stdout golden is `tests/help.out`. All other
stdout, stderr and status expectations are literals in `tests/check.cmake`.

## Files changed

Modified existing files:

- `CMakeLists.txt`: one `add_subdirectory(examples/tablecheck)` inside the
  top-level C-runtime build, leaving other consumers' target sets unchanged.
- `README.md` and `examples/README.md`: discoverability links and application
  purpose.
- `docs/TESTING.md`: new application case and current registration counts.

Added application files:

- `examples/tablecheck/main.relic`: CLI parsing, file access, error/exit boundary.
- `examples/tablecheck/table.relic`: typed validation, counts and reporting.
- `examples/tablecheck/CMakeLists.txt`: standalone consumer and normal top-level
  integration, application/type-fixture targets and exact-check ctest case.
- `examples/tablecheck/README.md`: self-contained build/run instructions,
  contract, limitations, sample output and repairs for deliberate mistakes.
- `examples/tablecheck/samples/tickets.csv`, `stock.tsv`, `broken.csv`.
- `examples/tablecheck/tests/check.cmake`.
- `examples/tablecheck/tests/tickets.out`, `stock.out`, `help.out`.
- `examples/tablecheck/tests/syntax_error.relic`, `syntax_error.err`.
- `examples/tablecheck/tests/type_error.relic`, `type_error.err`.
- `docs/reviews/useful-app-20261108.md`: this report.

No language/compiler/runtime fix or associated language-regression test was
needed. The 49 new checks are application regression/acceptance coverage and
compiler/runtime diagnostic exercises. `bootstrap/cgen.c`, `selfhost/`, `src/`,
`runtime/`, `std/` and the existing language goldens remain unchanged.

## Omitted checks, limitations and remaining blockers

- **Host build/tests:** environment/toolchain blocker observed on clean base;
  no final host run, VM/tree comparison, self-host interpreter comparison,
  guide/stdlib host checks, host-vs-C generation comparison or host name check
  is claimed. Investigating that host failure is separate work, not a fix
  required by this C-only application.
- **Human README-only FizzBuzz:** explicitly **unverified**. No unfamiliar human
  wrote or ran FizzBuzz in this session. The acceptance checkbox in
  `docs/1.0-CHECKLIST.md` remains open and unchanged.
- **Other platforms/CI:** no Windows, macOS, Alpine-without-C++, remote CI or
  cross-machine portability result was obtained. Only the installed machine
  toolchain was used. No external service was contacted.
- **Graphics/sanitizers/fuzz/bench:** omitted. This CLI has no graphics and no
  language/runtime changes. The relevant default runtime and consumer tests
  plus application leak checks ran; those are not ASan/UBSan or a performance
  guarantee. No dependencies were installed to expand testing.
- **Input scope:** no full quoted CSV, embedded newlines/delimiters, numeric
  aggregates, encoding validation, streaming or memory-cap guarantee. A
  same-width wrong delimiter cannot always be detected; select the right one.
- **Review:** independent review has not happened. All modifications and new
  source/docs are left uncommitted and unstaged on the requested branch. Build
  artifacts are ignored local evidence, not release assets. Work stops after
  this one deliverable.
