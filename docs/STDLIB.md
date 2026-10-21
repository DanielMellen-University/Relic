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
| [std/maps](#stdmaps) | values, items, get with a fallback, merge |
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
| `repeat(s, n)` | string | |
| `pad_left(s, width, fill)` / `pad_right(...)` | string | |
| `is_space(c)` | bool | space, tab, newline, CR |

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

## std/math

| Call | Returns |
|---|---|
| `abs(x)`, `min(a, b)`, `max(a, b)`, `clamp(x, lo, hi)` | number |
| `floor(x)`, `ceil(x)`, `round(x)` | int; `round` takes halves up, so `round(-2.5)` is `-2` |
| `sqrt(x)`, `pow(x, y)`, `sin(x)`, `cos(x)`, `exp(x)`, `log(x)` | float |
| `is_whole(x)` | bool |
| `pi` `e` | float constants |

## std/files

Paths are relative to where `relic` runs. Caught errors use the std name
(`files.list(): cannot list x`), not the `host_*` name.

| Call | Returns | On failure |
|---|---|---|
| `read(path)` | string | `cannot open {path}` |
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
| `ext(p)` | string | with the dot: `".txt"`, or `""` |
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
| `exists(fs, path)` | bool | never fails |
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
| `encode(v)` | string | `json: cannot encode a {type}` for function/module |
| `pretty(v)` | string | same; 2-space indent |
| `decode(text)` | value | `json: {what} at {N}` where `N` is a character index |

`catch err` after a bad `decode` gives `{message, line, col}`. The useful
part is `err.message` (includes the `at N` index). `line` / `col` point at
the `fail` inside `std/json`, not at a spot in the JSON text.

## std/os

| Call | Returns | Notes |
|---|---|---|
| `args()` | list of strings | words after the file name |
| `env(name)` | string or `none` | |
| `cwd()` | string | `/` separators |
| `exit(code)` | never returns | flushes output first |

`os.args()` is the same list as the built-in `args()`. Prefer `os.args()` in
modules; `args()` is also fine at the top level of a script.

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

Needs a host built with `-DRELIC_SDL=ON`. `available()` is `false` otherwise;
every other call then fails with a rebuild message.

Colors are 0 to 255 per channel. One window at a time.

### Window

| Call | Args | Returns | Errors |
|---|---|---|---|
| `available()` | | bool | |
| `open(w, h, title)` | int, int, string | none | size must be > 0; title must be string |
| `close()` | | none | |
| `size()` | | `[w, h]` | no window open |
| `present()` | | none | no window |
| `quit_requested()` | | bool | no window; also pumps events |
| `wait(ms)` | number | none | |

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
| `load_font(path, size)` | string, int >= 1 | font id (int) | needs fonts; bad path |
| `font_text(font, x, y, s, r, g, b)` | | none | UTF-8; needs fonts |
| `font_size(font, s)` | | `[w, h]` | needs fonts |

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
| `key_down` / `key_up` | `key` (string), `repeat` (bool), `shift` `ctrl` `alt` (bool) |
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

Mods for `"key"` / `"keyup"`: `"shift"`, `"ctrl"`, `"alt"`, or join with `+`
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

Window drag and resize inside a Relic desktop (like M0N0LITH-2) are built from `mouse_down`,
`mouse_move` and `mouse_up`; the host window itself sends `resize`.
