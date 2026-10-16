# Standard library

Load a module with `use std/NAME`, then call `NAME.fun(...)`. Modules are
plain Relic in `std/`; the few `host_*` calls they make are the C++ host.
Errors raise with `fail` and can be caught with `try` / `catch`.

## std/strings
`find(s, sub)` (index or -1), `contains`, `starts_with`, `ends_with`,
`upper`, `lower`, `trim`, `split(s, sep)`, `join(items, sep)`,
`replace(s, old, new)`, `repeat(s, n)`, `pad_left(s, width, fill)`,
`pad_right(s, width, fill)`, `is_space(c)`.

## std/lists
`map(xs, f)`, `filter(xs, keep)`, `reduce(xs, f, start)`, `sum`,
`index_of(xs, item)`, `reverse`, `copy`, `sort(xs)`, `sort_by(xs, less)`.
Sorting gives a new list and is stable.

## std/maps
`values(m)`, `items(m)` (list of `[key, value]`), `get(m, k, fallback)`, `merge(a, b)`.

## std/math
`abs`, `min`, `max`, `clamp(x, lo, hi)`, `floor`, `ceil`, `round`, `sqrt`,
`pow`, `sin`, `cos`, `exp`, `log`, `is_whole`.

## std/files
Text: `read(path)`, `write(path, text)`, `append_text(path, text)`,
`lines(path)`, `exists(path)`.
Folders: `list(path)` (sorted names), `is_dir`, `make_dir` (parents too),
`remove` (file or empty folder), `remove_tree`, `rename(from, to)`, `size` (bytes).
Paths are relative to where `relic` runs.

## std/paths
`join(a, b)`, `base(p)`, `dir(p)`, `ext(p)` (with the dot), `stem(p)`,
`normalize(p)`. Always `/`; a `\` in input is read as `/`.

## std/json
`encode(v)`, `pretty(v)` (2-space indent), `decode(text)`. Objects are maps
and keep key order, `null` is `none`. Bad input fails with `json: ... at N`.

## std/os
`args()`, `env(name)` (string or `none`), `cwd()`, `exit(code)`.

## std/time
`now()` (seconds since 1970, UTC), `clock()` (steady, for differences),
`since(start)`, `sleep(ms)`.
Frame timer: `let t = time.timer()`, then each frame `let dt = time.tick(t)`;
`t.fps` is the measured rate and `time.limit(t, 60)` caps it.

## std/graphics
Needs a host built with `-DRELIC_SDL=ON`; `available()` says if it was.

- Window: `open(w, h, title)` (resizable), `close()`, `size()` gives `[w, h]`,
  `present()`, `quit_requested()`, `wait(ms)`.
- Drawing: `clear`, `rect`, `pixel`, `line`, `outline`, `rect_alpha` (last arg 0 to 255),
  `clip(x, y, w, h)` and `unclip()`. Colors are 0 to 255 per channel.
- Text: `text`, `text_scaled` (built-in 8x12 font); with SDL2_ttf `load_font`,
  `font_text`, `font_size`.
- Images: `load_image` (BMP, plus PNG and JPG with SDL2_image), `image`,
  `image_width`, `image_height`. `canvas(w, h)` makes an image to draw into with
  `draw_to(c)`; `draw_to(none)` goes back to the window.
- Events: `next_event()` gives the oldest event map or `none`. Types:
  `key_down` / `key_up` (key, repeat, shift, ctrl, alt), `text` (text),
  `mouse_down` / `mouse_up` (button, x, y, clicks), `mouse_move` (x, y, dx, dy),
  `wheel` (dx, dy), `resize` (width, height), `quit`.
  `simulate(kind, a, b)` queues a fake one for tests.
- Polling still works: `key_down(name)`, `next_key()`, `mouse_x()`, `mouse_y()`, `mouse_down(button)`.
- Clipboard: `copy(s)`, `clipboard()`. Uses the system clipboard while a window is open.

Window drag and resize inside a Relic desktop are built from `mouse_down`,
`mouse_move` and `mouse_up` events; the host window itself sends `resize`.
