# Standard library

Load a module with `use std/NAME`, then call `NAME.fun(...)`. Modules are
plain Relic in `std/`; the few `host_*` calls they make are the C++ host
([HOST.md](HOST.md)). Errors raise with `fail` and can be caught with
`try` / `catch`. Built-in functions that need no `use` are listed in
[LANGUAGE.md](LANGUAGE.md#built-in-functions).

| Module | For |
|---|---|
| [std/strings](#stdstrings) | searching, splitting, case, padding |
| [std/lists](#stdlists) | map, filter, reduce, sorting |
| [std/maps](#stdmaps) | values, items, get with a fallback, merge, drop |
| [std/math](#stdmath) | rounding, roots, trig, constants |
| [std/files](#stdfiles) | text files and folders on the host disk |
| [std/paths](#stdpaths) | joining and splitting `/` paths |
| [std/vfs](#stdvfs) | an in-memory filesystem |
| [std/json](#stdjson) | JSON in and out |
| [std/os](#stdos) | args, environment, current folder, exit |
| [std/time](#stdtime) | clocks, sleep, a frame timer |
| [std/graphics](#stdgraphics) | a window, drawing, input, images, fonts (SDL builds) |

## std/strings

| Call | Returns | Notes |
|---|---|---|
| `find(s, sub)` | int | index of first `sub`, or `-1` |
| `contains(s, sub)` | bool | |
| `starts_with(s, pre)` | bool | |
| `ends_with(s, suf)` | bool | |
| `upper(s)` / `lower(s)` | string | ASCII letters only |
| `trim(s)` | string | leading and trailing space |
| `split(s, sep)` | list | |
| `join(items, sep)` | string | |
| `replace(s, old, new)` | string | all non-overlapping |
| `repeat(s, n)` | string | `n` a whole number; 0 or less gives `""` |
| `pad_left(s, width, fill)` / `pad_right(...)` | string | adds `fill` whole until the string is at least `width` long (a longer `fill` can overshoot); an empty `fill` that would have to be added is an error, and so is a `width` of +infinity |
| `is_space(c)` | bool | space, tab, newline, CR |

`upper`, `lower`, `trim`, `split` and `repeat` fail with `strings.upper(): argument must be a string, got int` (since 1.0.28) when given something else (`split` says `text` or `separator`, `repeat` says `text`). Since 1.0.35 `repeat` also wants a whole-number count (`strings.repeat(): count must be a whole number`), and `pad_left` and `pad_right` check `text`, `fill` and `width` (`strings.pad_left(): width must not be infinite`; an infinite width used to pad forever). Their messages start with `strings.` like the rest.

## std/lists

| Call | Returns | Notes |
|---|---|---|
| `map(xs, f)` | list | |
| `filter(xs, keep)` | list | |
| `reduce(xs, f, start)` | any | |
| `sum(xs)` | number | |
| `index_of(xs, item)` | int | or `-1` |
| `reverse(xs)` | list | new list |
| `copy(xs)` | list | shallow |
| `sort(xs)` | list | new list, stable |
| `sort_by(xs, less)` | list | `less(a, b)` is bool |

`fun_add` and `natural` are also visible: they are the helpers behind `sum` and
`sort`, not meant to be called.

## std/maps

| Call | Returns | Notes |
|---|---|---|
| `values(m)` | list | insertion order |
| `items(m)` | list | `[key, value]` pairs |
| `get(m, k, fallback)` | any | fallback when `k` missing |
| `merge(a, b)` | map | new map; **`b` wins** on shared keys |
| `drop(m, k)` | bool | removes `k` in place if it is there (`true`), else `false`; for code where a function of its own named `remove` hides the built-in, like `std/vfs` |

## std/math

| Call | Returns |
|---|---|
| `abs(x)`, `min(a, b)`, `max(a, b)`, `clamp(x, lo, hi)` | number |
| `floor(x)`, `ceil(x)`, `round(x)` | int; `round` takes halves up, so `round(-2.5)` is `-2`; an int comes back as it is; nan, an infinity or a float past the int range is `math.floor(): number is out of the int range` |
| `sqrt(x)`, `pow(x, y)`, `sin(x)`, `cos(x)`, `exp(x)`, `log(x)` | float |
| `is_whole(x)` | bool |
| `pi` `e` | float constants |

`abs`, `floor`, `ceil` and `round` fail with `math.abs(): argument must be a number, got string` (since 1.0.28) when `x` is not a number. `min`, `max` and `clamp` compare with `<`, so they take anything that compares, and their errors are the language's (`cannot compare a string and an int`).

## std/files

Paths are relative to where `relic` runs. Caught errors use the std name
(`files.list(): cannot list x`), not the `host_*` name.

| Call | Returns | On failure |
|---|---|---|
| `read(path)` | string | `cannot open {path}` (also for a folder) |
| `write(path, text)` | none | `cannot write {path}` |
| `append_text(path, text)` | none | `cannot write {path}` |
| `lines(path)` | list of strings | same as `read`; strips a trailing empty line |
| `exists(path)` | bool | never fails; true for a file **or** a folder |
| `list(path)` | list of names | sorted; `cannot list {path}` |
| `is_dir(path)` | bool | never fails (`false` if missing) |
| `make_dir(path)` | none | parents too; **ok if it already exists**; `cannot make folder {path}` |
| `remove(path)` | none | file or **empty** folder; fails if missing, or if the folder still has entries: `cannot remove {path}` |
| `remove_tree(path)` | none | folder and contents, or one file; **missing path is a no-op** |
| `rename(from, to)` | none | `cannot rename {from}` |
| `size(path)` | int (bytes) | `cannot get the size of {path}` |

## std/paths

Always `/`. A `\` in input is read as `/`.

| Call | Returns | Example |
|---|---|---|
| `join(a, b)` | string | `join("a", "b")` → `"a/b"`; absolute `b` wins |
| `base(p)` | string | `"a/b.txt"` → `"b.txt"` |
| `dir(p)` | string | `"a/b.txt"` → `"a"`; no slash → `""` |
| `ext(p)` | string | with the dot: `".txt"`, or `""`; leading dots belong to the name, so `".hidden"` and `".."` have none (`".."` gave `"."` before 1.0.35) |
| `stem(p)` | string | `"b.txt"` → `"b"` |
| `normalize(p)` | string | collapses `.` and `..` |


## std/vfs

In-memory virtual filesystem (M11). The M0N0LITH-2 apps share one. Not the host disk; use
`std/files` for that. Create one with `vfs.new()`, then pass it as the first
argument. Paths use `/` and are normalized (`std/paths`). `new()` seeds
`/home/welcome.txt` and an empty `/tmp`.

| Call | Returns | On failure |
|---|---|---|
| `new()` | vfs | |
| `exists(fs, path)` | bool | never fails; a path that goes through a file (`/home/welcome.txt/x`) is not there |
| `is_dir(fs, path)` | bool | never fails (`false` if missing) |
| `list(fs, path)` | list of names | sorted; `cannot list {path}` |
| `read(fs, path)` | string | `cannot read {path}` |
| `write(fs, path, text)` | none | `cannot write {path}` |
| `append(fs, path, text)` | none | creates the file if missing; `cannot append {path}` |
| `make_dir(fs, path)` | none | parents too; ok if it already exists; `cannot make folder {path}` |
| `remove(fs, path)` | none | file or **empty** folder; `cannot remove {path}` |
| `remove_tree(fs, path)` | none | folder and contents, or one file; **missing path is a no-op** (except `/`) |
| `rename(fs, from, to)` | none | `cannot rename {from}`, also when `to` exists or is inside `from` |
| `size(fs, path)` | int | length of the file text; `cannot get the size of {path}` |

## std/json

Objects become maps (key order kept). Arrays become lists. `null` is `none`.

| Call | Returns | On failure |
|---|---|---|
| `encode(v)` | string | `json: cannot encode a {type}` for function/module, `json: cannot encode inf or nan` for those floats |
| `pretty(v)` | string | same; 2-space indent |
| `decode(text)` | value | `json: {what} at {N}` where `N` is a character index; numbers follow JSON's grammar (no leading zeros, no `+`, digits after `.` and `e`, no `NaN`, `Infinity` or hex); since 1.0.36 a number too small for a normal float (one `encode` wrote for a subnormal, or `1e-400`) reads as that float or `0.0`, and one too large is still `bad number`; a value that is not a string is `json.decode(): text must be a string, got int` (since 1.0.37) |

`catch err` after a bad `decode` gives `{message, line, col}`. The useful
part is `err.message` (includes the `at N` index). `line` / `col` point at
the `fail` inside `std/json`, not at a spot in the JSON text.

## std/os

| Call | Returns | Notes |
|---|---|---|
| `args()` | list of strings | words after the file name |
| `env(name)` | string or `none` | |
| `cwd()` | string | `/` separators |
| `exit(code)` | never returns | flushes output first; `code` must be a whole number (`os.exit(): code must be a whole number`); the system keeps only part of it, so on Linux and macOS `-1` exits with 255 and `256` with 0 |
| `read_line()` | string or `none` | the next line of standard input (see below) |
| `prompt(text)` | string or `none` | `read_line()`, after showing `text` when stdin is a terminal |

`os.args()` is the same list as the built-in `args()`. Prefer `os.args()` in
modules; `args()` is also fine at the top level of a script.

**Standard input** (1.0.31). `read_line()` gives the next line without its line
end, which is `"\n"` or `"\r\n"`, and `none` once the input has run out (and
on every call after that). So a line of text comes back as a string, even an
empty one (`""`), and `none` can only mean the end:

```
use std/os
let total = 0
while true
    let line = os.read_line()
    if line == none
        break
    total = total + num(line)
print(total)
```

- Only the line end is cut: spaces and tabs stay, and a lone `"\r"` that is not
  followed by `"\n"` stays. A last line with no newline at the end of the input
  comes back as it is; an input that ends with a newline has no extra empty line.
- Lines can be any length and hold any bytes, as in `files.read`: bytes that are
  not valid UTF-8 and NUL bytes come through unchanged, and so does a Ctrl-Z byte.
  On Windows the input is read in binary mode, so nothing is translated.
- `prompt(text)` is for programs that talk to a person. The text is written to
  the program's output and shown at once, but only when stdin is a terminal;
  with input from a pipe or a file it is left out, so a program prints the same
  with or without a prompt. When the input ends on a terminal (Ctrl-D) a newline
  follows the prompt, so the shell prompt starts on a fresh line. `text` must be a
  string: `os.prompt(): prompt must be a string, got int`.
- A program, the REPL and `selfhost/run.relic` read the same standard input in
  order, so a line the REPL reads is not seen by the program and the other way
  round.
- The name is the one the other `os` and `files` calls would give it: `read_line`,
  like `files.lines`, and the `host_read_line(prompt)` primitive underneath
  (a bare `os.read_line()` is `host_read_line("")`).

## std/time

Times are in seconds (floating point).

| Call | Returns | Notes |
|---|---|---|
| `now()` | float | UTC seconds since 1970 |
| `clock()` | float | steady clock; only differences mean anything |
| `since(start)` | float | `clock() - start` |
| `sleep(ms)` | none | milliseconds |
| `timer()` | map | `{last, fps, frames, window}` |
| `tick(t)` | float | seconds since last tick; updates `t.fps` |
| `limit(t, fps)` | none | sleeps so frames run no faster than `fps` |

Frame timer: `let t = time.timer()`, then each frame `let dt = time.tick(t)`.
`t.fps` is averaged over about one second of frames (it stays `0` until that
window fills). Call `time.limit(t, 60)` at the end of a frame to cap the rate.
`limit` measures from `t.last` (set by the last `tick`), so call `tick` then
draw then `limit`.

## std/graphics

Needs a host built with `-DRELIC_SDL=ON`, or for compiled programs a C
runtime built with `-DRELIC_C_SDL=ON` and a program built with
`relic_add_program(name file GRAPHICS)`. Both give the same results and errors.
`available()` is `false` otherwise; every other call then fails with a rebuild
message.

Colors are 0 to 255 per channel. One window at a time.

### Window

| Call | Args | Returns | Errors |
|---|---|---|---|
| `available()` | | bool | |
| `open(w, h, title)` | int, int, string | none | size must be > 0; title must be string |
| `close()` | | none | |
| `size()` | | `[w, h]` | no window open; in pixels |
| `present()` | | none | no window |
| `quit_requested()` | | bool | no window; also pumps events |
| `wait(ms)` | number | none | |
| `title()` / `set_title(s)` | string | string / none | no window; title must be a string |
| `window_size()` | | `[w, h]` | no window; in window units, the ones mouse positions use |
| `set_window_size(w, h)` | ints > 0 | none | no window; size must be > 0 |
| `scale()` | | float | no window; pixels per window unit, `size()[0] / window_size()[0]` |
| `fullscreen()` / `set_fullscreen(on)` | bool | bool / none | no window; `on` must be true or false; the system can refuse |

`set_window_size` and `set_fullscreen` change the drawing area, and the change
comes back as a `resize` event like one the user makes.

High-DPI screens (1.0.12, Daniel's pick A): windows open in high-DPI mode and
the renderer is scaled by `scale()`, so every drawing call, clip rectangle and
mouse position stays in window units and programs need no change. Shapes and
the built-in font are drawn at full resolution. TrueType fonts are opened at
`size * scale()` pixels, so text is sharp, and `font_size` gives window units
(rounded up). Images and canvases are drawn at their normal size in window
units, so on a high-DPI screen they are stretched; a canvas is drawn into one
unit per texel. `scale()` follows the window when it is resized or moves to a
screen with another scale (checked on every input call and `present`), and
fonts are opened again at the new size when that happens. On Windows Relic
asks SDL for DPI scaling, so window sizes are in scaled units there too.

### Cursor

| Call | Args | Returns | Errors |
|---|---|---|---|
| `set_cursor(name)` | string | bool | no window; unknown name. `false` when the system cannot show that shape (the headless dummy driver never can) |
| `show_cursor(on)` | bool | none | no window |
| `cursor_visible()` | | bool | no window |

Cursor names: `arrow`, `text`, `wait`, `crosshair`, `hand`, `move`, `no`,
`resize_ew`, `resize_ns`, `resize_nwse`, `resize_nesw`.

### Frames

| Call | Args | Returns | Errors |
|---|---|---|---|
| `set_vsync(on)` | bool | bool | no window; `true` when the renderer took it |
| `vsync()` | | bool | no window |
| `ticks()` | | int | no window; whole milliseconds since `open` |
| `frame_time()` | | float | no window; seconds between the last two presents, 0.0 before two |
| `present_capped(fps)` | number > 0 | none | no window; `present()` that first waits so frames come no faster than `fps` a second |

```
while not graphics.quit_requested()
    let dt = graphics.frame_time()   # move things by speed * dt
    draw()
    graphics.present_capped(60)
```

`std/time` has a frame timer too (`timer`, `tick`, `limit`) that works without
a window.

### Drawing

| Call | Args | Returns | Errors |
|---|---|---|---|
| `clear(r, g, b)` | 3 channels | none | no window; channel 0..255 |
| `rect(x, y, w, h, r, g, b)` | ints + channels | none | |
| `rect_alpha(x, y, w, h, r, g, b, a)` | same + alpha 0..255 | none | blends over what is there |
| `pixel(x, y, r, g, b)` | | none | |
| `line(x1, y1, x2, y2, r, g, b)` | | none | |
| `outline(x, y, w, h, r, g, b)` | | none | rectangle border |
| `clip(x, y, w, h)` | | none | draw only inside; `w`/`h` <= 0 clears |
| `unclip()` | | none | same as `clip(0,0,0,0)` |

### Text

| Call | Args | Returns | Errors |
|---|---|---|---|
| `text(x, y, s, r, g, b)` | | none | built-in 8x12 ASCII; other chars draw as `?` |
| `text_scaled(x, y, s, scale, r, g, b)` | scale >= 1 | none | each glyph is `8*scale` wide, `12*scale` tall |
| `has_fonts()` | | bool | true when built with SDL2_ttf |
| `load_font(path, size)` | string, int >= 1 (window units) | font id (int) | needs fonts; bad path |
| `font_text(font, x, y, s, r, g, b)` | | none | UTF-8; needs fonts |
| `font_size(font, s)` | | `[w, h]` in window units | needs fonts |

### Images and canvas

| Call | Args | Returns | Errors |
|---|---|---|---|
| `has_png()` | | bool | true when built with SDL2_image |
| `load_image(path)` | string | image id (int) | open window first; BMP always; PNG/JPG need `has_png()` |
| `image(id, x, y)` | | none | draws at full size |
| `image_width(id)` / `image_height(id)` | | int | |
| `canvas(w, h)` | ints > 0 | image id | paint target |
| `draw_to(c)` | id or `none` | none | `none` returns drawing to the window |

While `draw_to(c)` is active, every draw call (including `clear`) goes to that
canvas only, not the window. `clear` fills the whole canvas.

```
let c = graphics.canvas(64, 64)
graphics.draw_to(c)
graphics.clear(20, 20, 40)
graphics.rect(8, 8, 48, 48, 200, 80, 40)
graphics.draw_to(none)
graphics.image(c, 10, 10)   # stamp the canvas onto the window
```

### Polling input

| Call | Args | Returns | Errors |
|---|---|---|---|
| `key_down(name)` | string | bool | unknown key name |
| `next_key()` | | string or `none` | next pressed key since last call |
| `mouse_x()` / `mouse_y()` | | int | **window pixel** coords (0,0 is the top-left of the client area) |
| `mouse_down(button)` | 1 left, 2 middle, 3 right | bool | |

### Events

`next_event()` gives the oldest event map or `none`. Each map has `type`
plus the fields below.

| `type` | Fields |
|---|---|
| `key_down` / `key_up` | `key` (string), `repeat` (bool), `shift` `ctrl` `alt` `gui` (bool; `gui` is Cmd on macOS and the Windows key elsewhere, since 1.0.40) |
| `text` | `text` (UTF-8 string; use this for typing into a box) |
| `mouse_down` / `mouse_up` | `button` (1/2/3), `x`, `y`, `clicks` (x/y are window pixels) |
| `mouse_move` | `x`, `y`, `dx`, `dy` (window pixels) |
| `wheel` | `dx`, `dy` (`dy` > 0 scrolls up) |
| `resize` | `width`, `height` |
| `quit` | (no extra fields) |

`click` is **not** an event type. It is a `simulate` kind that queues a
`mouse_down` then a `mouse_up` (left button, `clicks` 1). Real events always
use `mouse_down` / `mouse_up`.

### simulate

`simulate(kind, a, b)` queues a fake event. Needs an open window. Pass `none`
for an unused arg.

| kind | a | b | Event(s) queued |
|---|---|---|---|
| `"key"` | key name | `none` or mods | one `key_down` only (**does not** also queue `text`) |
| `"keyup"` | key name | `none` or mods | one `key_up` |
| `"text"` | string | `none` | one `text` (use this for typing into a box; pair with `"key"` if you also need the key event) |
| `"click"` | x | y | `mouse_down` then `mouse_up` (left) |
| `"mousedown"` | x | y | one `mouse_down` (left, `clicks` 1) |
| `"mouseup"` | x | y | one `mouse_up` (left, `clicks` 1) |
| `"move"` | x | y | one `mouse_move` (`dx` and `dy` are 0) |
| `"wheel"` | dx | dy | one `wheel` |
| `"resize"` | width | height | one `resize` |
| `"quit"` | `none` | `none` | one `quit` (and `quit_requested()` becomes true) |

Mods for `"key"` / `"keyup"`: `"shift"`, `"ctrl"`, `"alt"`, `"gui"`, or join with `+`
(`"ctrl+shift"`). They set the matching bools on the event. Unknown kind or
key name fails.

### Key names

Any name `SDL_GetKeyFromName` accepts works (case-insensitive). Unknown names
fail with `unknown key name`. Common ones:

| Name | Key |
|---|---|
| `"a"` .. `"z"` | letters |
| `"0"` .. `"9"` | digits |
| `"space"` | space |
| `"return"` | Enter |
| `"escape"` | Esc |
| `"backspace"` | Backspace |
| `"tab"` | Tab |
| `"left"` `"right"` `"up"` `"down"` | arrows |
| `"lshift"` `"rshift"` | Shift |
| `"lctrl"` `"rctrl"` | Ctrl |
| `"lalt"` `"ralt"` | Alt |
| `"f1"` .. `"f12"` | function keys |

`key_down("left")` polls the held state. Event `key` fields use the same
names. Prefer `text` events for typed characters (shift, layout, Unicode).
A simulated `"key"` does **not** produce a `text` event; call `simulate("text", ...)` too when testing typing.

### Clipboard

| Call | Args | Returns |
|---|---|---|
| `copy(s)` | string | none |
| `clipboard()` | | string |

Uses the system clipboard while a window is open; otherwise keeps text inside
this process (so tests work headless).

### Running headless / testing GUI apps

Build with SDL, then run with a fake display:

```
cmake -S . -B build-sdl -DRELIC_SDL=ON
cmake --build build-sdl
SDL_VIDEODRIVER=dummy ./build-sdl/relic your_app.relic
```

`open` still works. There is no visible window. Drive input with `simulate`,
read it with `next_event`, and assert on your app state (see the
`examples/mini_desktop/` language example and `tests/sdl/gfx_events.relic`).

`ctest --test-dir build-sdl` sets `SDL_VIDEODRIVER=dummy` for the graphics
tests automatically.

A compiled program works the same way. Configure with `-DRELIC_C_SDL=ON`,
build it with `relic_add_program(name file GRAPHICS)`, and run it with `SDL_VIDEODRIVER=dummy`.

Window drag and resize inside a Relic desktop (like M0N0LITH-2) are built from `mouse_down`,
`mouse_move` and `mouse_up`; the host window itself sends `resize`.
