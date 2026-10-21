# Relic docs

## Reading order

**Writing Relic programs:**

1. [Project README](../README.md): build it, and the five-minute tour
2. [GUIDE.md](GUIDE.md): the language step by step, with examples you can run
3. [LANGUAGE.md](LANGUAGE.md): every rule, for looking things up
4. [STDLIB.md](STDLIB.md): every `std/` module and function
5. [CLI.md](CLI.md): the `relic` command, `relic check`, the REPL
6. [Examples](../examples/README.md): small programs, including graphics

**Working on Relic itself:**

1. [CONTRIBUTING.md](../CONTRIBUTING.md): rules for a change, commits, versions
2. [ARCHITECTURE.md](ARCHITECTURE.md): the host, the two engines, memory, `selfhost/`
3. [TESTING.md](TESTING.md): golden, comparison, unit and graphics tests, CI
4. [HOST.md](HOST.md): `host_*` built-ins, adding one, embedding `relic_core`
5. [C-RUNTIME.md](C-RUNTIME.md): the C runtime that compiled Relic links against
6. [CGEN.md](CGEN.md): the code generator that compiles Relic to C
7. [DESIGN.md](DESIGN.md): why the language is the way it is, open questions
8. [ROADMAP.md](ROADMAP.md) and [PROJECT_PLAN.md](PROJECT_PLAN.md): what is done and what is next

## All docs

| File | For | What it covers |
|---|---|---|
| [GUIDE.md](GUIDE.md) | users | a walk through the language |
| [LANGUAGE.md](LANGUAGE.md) | users | syntax, types, scope, operators, modules, errors, built-ins, limits |
| [STDLIB.md](STDLIB.md) | users | each std module, with return values and errors; graphics events and headless testing |
| [CLI.md](CLI.md) | users | commands, exit codes, `relic check`, REPL, environment variables |
| [ARCHITECTURE.md](ARCHITECTURE.md) | contributors | folders, build targets, pipeline, VM, memory, self-host and its limits, the compiler to C |
| [TESTING.md](TESTING.md) | contributors | how each kind of test works and how to run it |
| [HOST.md](HOST.md) | contributors | the C++ built-ins under `std/`, and running Relic from C++ |
| [C-RUNTIME.md](C-RUNTIME.md) | contributors | `runtime/c/`: values, ownership, calling convention, errors, built-ins, cycle collector, tests |
| [CGEN.md](CGEN.md) | contributors | `selfhost/cgen.relic`: using it, what it covers, the generated C, scopes, modules, error positions, tests, compiling itself, speed, decided and open choices |
| [DESIGN.md](DESIGN.md) | both | the reasons behind the language, the syntax freeze, open questions |
| [VISION.md](VISION.md) | both | the goal, principles, non-goals, and the borrowed pieces list |
| [ROADMAP.md](ROADMAP.md) | both | phases with tickable outcomes |
| [PROJECT_PLAN.md](PROJECT_PLAN.md) | contributors | milestones M0 to M12, SH1 to SH4 and C1 to C5 with status, exit criteria, risks, decisions |
| [MONOLITH2-READINESS.md](MONOLITH2-READINESS.md) | both | what the MONOLITH 2 desktop needs from Relic, and what is done |
| [1.0-CHECKLIST.md](1.0-CHECKLIST.md) | record | what 1.0 needed (released as `v1.0.0`) |
| [BOOTSTRAP.md](BOOTSTRAP.md) | record | how the host was started from an empty repo |
| [CHANGELOG.md](../CHANGELOG.md) | both | changes by version |

## Tags used across the docs

- **Default**: a choice made to keep moving. Built and tested, but easy to change.
- **Open**: needs a decision from Daniel before work on it starts.
