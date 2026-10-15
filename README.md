# Relic

Relic is the programming language for MONOLITH 2.

> Status: **design phase**. This repo holds design and roadmap docs only. There is no compiler or interpreter yet.

## What Relic is

- A small, readable language built from scratch for MONOLITH 2.
- The long-term goal is a stack where nothing is borrowed. Relic, its runtime, its standard library, the desktop shell and the apps are all written in-house.
- Phase 1 uses a temporary C/C++ host to bootstrap the first interpreter. That host is a scaffold and gets replaced once Relic can build itself.

## How it relates to MONOLITH

- **MONOLITH 1.0** is finished and lives in its own repo. It stays C++ and SDL2. Relic and its IDE do not go there.
- **MONOLITH 2** is the next version. Its desktop shell and apps will be rewritten in Relic.
- Relic lives here so the language can grow on its own schedule.

## Docs

- [Vision](docs/VISION.md): the goal and the non-goals
- [Roadmap](docs/ROADMAP.md): phased milestones
- [Design](docs/DESIGN.md): the shape of the language (draft)
- [Project plan](docs/PROJECT_PLAN.md): milestones, exit criteria, risks
- [Architecture](docs/ARCHITECTURE.md): bootstrap host layout
- [Bootstrap](docs/BOOTSTRAP.md): empty repo to `print("hello")`
- [Testing](docs/TESTING.md): golden tests and CI
- [1.0 checklist](docs/1.0-CHECKLIST.md): what a first usable Relic needs

## Contributing

Design decisions are still open. Items marked **Default** or **Open** in the docs are proposals, not final calls.
