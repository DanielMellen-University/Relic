# Learn by running

Small programs you can read from top to bottom. From the repo root, after a host build:

```
build/relic examples/learn/fizzbuzz.relic
```

A build without the host can compile the same programs. Each one is also `tests/cgen/learn_<name>.relic`, which `use`s the program, and ctest runs it as `cgen_only_learn_<name>`.

Every program has a sibling `.out` file. `stdlib_examples` runs the program and requires that exact stdout. The compiled test requires the same bytes.

`examples/word_count.relic` is the same kind of program, one directory up. It counts the words in a file, or in a short text of its own when you pass no file.

| Program | Shows |
|---|---|
| `fizzbuzz.relic` | `if` / `else if` and a range |
| `json_pretty.relic` | `std/json` encode, pretty and a round trip |
| `todo.relic` | a list of tasks and a tiny command script |
| `matrix.relic` | multiply, transpose, determinant |
| `adventure.relic` | rooms, a bag, and a fixed command script |
| `primes.relic` | the sieve of Eratosthenes |
| `search.relic` | binary search |
| `hanoi.relic` | towers of Hanoi |
| `collatz.relic` | the 3n+1 sequence |
| `roman.relic` | Roman numerals, both ways |
| `caesar.relic` | a letter shift |
| `rpn.relic` | reverse Polish notation |
| `calc.relic` | infix `+ - * /` and parentheses |
| `stats.relic` | sum, mean, median, min, max |
| `distance.relic` | Levenshtein edit distance |
| `maze.relic` | breadth-first search |
| `luhn.relic` | the Luhn checksum |
| `morse.relic` | Morse code |
| `grades.relic` | scores and letter grades |
| `units.relic` | a few unit conversions |
| `hangman.relic` | a scripted game of hangman |
| `calendar.relic` | one month, Sunday first |
| `budget.relic` | a ledger kept in cents |
| `histogram.relic` | a bar chart of word counts |
| `wrap.relic` | wrapping text on word boundaries |
| `anagrams.relic` | grouping anagrams |
| `piglatin.relic` | Pig Latin |
| `brackets.relic` | balanced brackets |
| `rationals.relic` | fractions reduced with `math.gcd` |

One call at a time, with the same kind of expected output, is under `examples/ref/`. The pages in [docs/stdlib](../../docs/stdlib/README.md) point at those files.
