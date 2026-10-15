# Design (draft)

Everything here is a proposal. **Default** marks a choice made to keep moving. **Open** marks a question that needs Daniel's call.

## File extension

**Default:** `.relic`. Short and clear. Open alternative: `.rl`.

## Values

- Numbers: **Default:** one number type (64-bit float) for 1.0. Separate int and float is **Open** for later.
- Strings: UTF-8, immutable.
- Bools: `true`, `false`.
- Nothing: `nil`.
- Lists: ordered, growable, any value.
- Maps: planned after 1.0.

## Typing

**Default:** dynamic typing for 1.0. Optional type hints are **Open** for a later phase.

## Variables

**Default:** declared with `let`. Reassigning an undeclared name is an error.

## Control flow

- `if` / `else`
- `while`
- `for` over lists: **Open** (may land after 1.0)
- `break`, `continue`, `return`

## Functions

- Declared with `fn`, first-class values.
- Closures: **Default:** yes, since they are cheap in a tree-walker.

## Blocks and syntax style

**Default:** curly braces, no semicolons needed at line ends. Indentation-based blocks are the **Open** alternative.

Sketch only, not final syntax:

```
let name = "MONOLITH"
fn greet(who) {
  print("hello " + who)
}
if name != "" {
  greet(name)
}
```

## Modules

**Open.** Planned for phase 4. Proposal: one file is one module, loaded with `import`.

## Memory model

**Open.** Options: reference counting, a tracing garbage collector, or arenas. Phase 1 can rely on the host. A decision is needed before phase 4.

## Errors

**Default:** runtime errors stop the program with a message and location. Catchable errors are **Open**.

## Architecture

1. **Lexer**: source text to tokens.
2. **Parser**: tokens to an AST.
3. **Interpreter**: walks the AST directly. Simple and easy to debug.
4. **Later**: compile the AST to bytecode for a VM.
5. **Much later**: native code generation.

The REPL and the file runner share the same pipeline.

## Grammar

No formal grammar yet. One will be written once the syntax above settles, and it will be marked draft until then.
