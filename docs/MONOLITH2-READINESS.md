# MONOLITH 2 readiness

What a Relic-written OS shell and its apps (editor, terminal, files, paint,
settings) need from the language, runtime and stdlib. Status is for Relic main.

| Need | Status | Where |
|---|---|---|
| Error handling (`try` / `catch`, `fail`) | done | language |
| Modules, private `_names` | done | `use`, DESIGN.md |
| Text files: read, write, append, lines | done | std/files |
| Directories: list, make, remove, rename, is_dir, size | missing | std/files |
| Paths: join, dir, base, ext, normalize | missing | std/paths |
| String utils (split, join, trim, find, replace, pad) | partial, no pad | std/strings |
| Sorting | done | std/lists |
| Time: clocks, sleep, frame timer | partial, no sleep or timer | std/time |
| Serialization (JSON) for settings and files | missing | std/json |
| Process: args, env, cwd, exit | partial, args only | std/os |
| Input events: keys with modifiers, text input, mouse buttons, wheel, motion | missing (polling only) | std/graphics |
| Resizable window and its size | missing | std/graphics |
| Drawing: rect, pixel, text, images, fonts | done | std/graphics |
| Drawing: lines, outlines, clipping | missing | std/graphics |
| Clipboard | missing | std/graphics |
| 60fps redraw on the VM | not measured | bench/ |
| `use std/...` in the self-hosted interpreter | missing | selfhost/ |
| Stdlib reference docs | missing | docs/STDLIB.md |
