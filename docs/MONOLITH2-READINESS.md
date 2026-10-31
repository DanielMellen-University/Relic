# MONOLITH 2 readiness

What a Relic-written OS shell and its apps (editor, terminal, files, IDE, paint,
settings) need from the language, runtime and stdlib. Status is for Relic main.
Reference: [STDLIB.md](STDLIB.md).

| Need | Status | Where |
|---|---|---|
| Error handling (`try` / `catch`, `fail`) | done | language |
| Modules, private `_names` | done | `use`, LANGUAGE.md |
| Text files: read, write, append, lines | done | std/files |
| Folders: list, make, remove, rename, is_dir, size | done | std/files |
| Paths: join, dir, base, ext, stem, normalize | done | std/paths |
| String utils, padding | done | std/strings |
| Sorting (stable, custom order) | done | std/lists |
| Time: clocks, sleep, frame timer | done | std/time |
| JSON for settings and files | done | std/json |
| Process: args, env, cwd, exit | done | std/os |
| Events: keys with modifiers, text input, mouse buttons, motion, wheel, resize | done | std/graphics |
| Resizable window and its size | done | std/graphics |
| Drawing: rect, pixel, line, outline, alpha, clip, text, fonts, images | done | std/graphics |
| Canvas to draw into (paint) | done | std/graphics |
| Clipboard | done | std/graphics |
| Window title, size, fullscreen; cursor shapes; vsync and frame timing | done (1.0.11) | std/graphics |
| Sharp drawing on high-DPI screens, coordinates in window units | done (1.0.12) | std/graphics |
| 60fps redraw on the VM | done, see below | bench/redraw.relic |
| `use std/...` in the self-hosted interpreter | done | selfhost/ |
| A shared in-memory filesystem for the apps, with quick listing of big folders | done (1.0.42) | std/vfs |
| Queues and big text: removing from the front of a list, `x = x + t`, find and sort in linear time | done (1.0.42, 1.0.43) | language, std/strings, std/lists |
| Running a program inside the desktop (IDE): captured output, a step limit, refused built-ins | done (1.0.44) | selfhost/interp.relic, [ARCHITECTURE.md](ARCHITECTURE.md) |
| That program's `use` of the desktop's files, and input line by line | done (1.0.45) | selfhost/interp.relic |
| Stdlib reference docs | done | docs/STDLIB.md |

## Performance

`bench/redraw.relic` draws a taskbar and 6 windows with 20 lines of text each,
with no wait between frames. On the Release VM with SDL's software (dummy)
renderer it takes about 4 ms a frame (about 240 fps). A Debug build takes about
8 ms. Both are under the 16.7 ms a 60fps frame allows, so no fix was needed.
10,000 rectangle hit tests take about 4 ms. Sorting 5,000 numbers took about 40 ms
before 1.0.43 and takes about 0.4 ms since.

## Not in yet, and why

- Running host programs from a terminal app. MONOLITH 1's terminal runs its
  own commands, so MONOLITH 2 can do the same in Relic.
- File change times and folder watching. None of the MONOLITH 1 apps need them.
