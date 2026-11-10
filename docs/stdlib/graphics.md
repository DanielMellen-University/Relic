# std/graphics

A window, drawing, keyboard and mouse. `use std/graphics`.

This module needs a host built with `-DRELIC_SDL=ON`, or a compiled program built with `relic_add_program(name file GRAPHICS)` against a runtime configured with `-DRELIC_C_SDL=ON`. `available()` is false otherwise, and every other drawing call then fails, telling you to rebuild with SDL2.

`available()`, `has_fonts()` and `has_png()` are bools in every build. The bool itself follows how the binary was built, so the reference does not pin those three values. The clipboard does not need a window. [clipboard.relic](../../examples/ref/graphics/clipboard.relic) copies a word and prints:

```
relic
bool
bool
bool
```

Colors are 0 to 255 per channel. One window at a time. The long version of this page, including high-DPI rules and the event fields, is [STDLIB.md](../STDLIB.md#stdgraphics).

## std/graphics

### Window

| Call | Returns |
|---|---|
| `available()` | bool |
| `open(w, h, title)` | none. Width and height must be above 0. The title must be a string |
| `close()` | none |
| `size()` | `[w, h]` in pixels. Fails if no window is open |
| `present()` | none. Shows what was drawn |
| `quit_requested()` | bool. Also pumps events |
| `wait(ms)` | none |
| `title()` / `set_title(s)` | the title, or none |
| `window_size()` | `[w, h]` in window units, the units mouse positions use |
| `set_window_size(w, h)` | none. Both must be ints above 0 |
| `scale()` | float. Pixels per window unit |
| `fullscreen()` / `set_fullscreen(on)` | bool, or none. `on` must be true or false |

`examples/hello_window.relic` opens a window when graphics are built, and prints one line when they are not.

### Drawing

| Call | What it draws |
|---|---|
| `clear(r, g, b)` | the whole target |
| `rect(x, y, w, h, r, g, b)` | a filled rectangle |
| `rect_alpha(..., a)` | the same, blended, alpha 0 to 255 |
| `pixel(x, y, r, g, b)` | one pixel |
| `line(x1, y1, x2, y2, r, g, b)` | a line |
| `outline(x, y, w, h, r, g, b)` | a rectangle border |
| `clip(x, y, w, h)` | later drawing stays inside. `w` or `h` of 0 or less clears the clip |
| `unclip()` | the same as `clip(0, 0, 0, 0)` |

`text(x, y, s, r, g, b)` draws the built-in 8 by 12 ASCII font. Other characters draw as `?`. `text_scaled` takes a scale of 1 or more.

`has_fonts()` is true when the build has SDL2_ttf. `load_font(path, size)` returns a font id. `font_text` draws UTF-8. `font_size(font, s)` returns `[w, h]` in window units.

`has_png()` is true when the build has SDL2_image. `load_image(path)` loads a BMP always, and PNG or JPG when `has_png()` is true. Open a window first. `image(id, x, y)` draws it. `image_width` and `image_height` return ints.

`canvas(w, h)` returns an image id you can draw on. `draw_to(c)` sends later drawing to that canvas. `draw_to(none)` sends it back to the window.

### Input and events

`key_down(name)` is true while the key is held. `next_key()` is the next pressed key name, or `none`. `mouse_x` and `mouse_y` are window pixels. `mouse_down(button)` takes 1 for left, 2 for middle, 3 for right.

`next_event()` is the oldest event map, or `none`. Each map has `type`:

| type | Fields |
|---|---|
| `key_down`, `key_up` | `key`, `repeat`, `shift`, `ctrl`, `alt`, `gui` |
| `text` | `text` |
| `mouse_down`, `mouse_up` | `button`, `x`, `y`, `clicks` |
| `mouse_move` | `x`, `y`, `dx`, `dy` |
| `wheel` | `dx`, `dy` |
| `resize` | `width`, `height` |
| `quit` | none extra |

`simulate(kind, a, b)` queues a fake event. Pass `none` for an unused argument. Kinds are `key`, `keyup`, `text`, `click`, `mousedown`, `mouseup`, `move`, `wheel`, `resize` and `quit`. A simulated `key` does not also queue `text`.

`copy(s)` and `clipboard()` use the system clipboard while a window is open, and an in-process buffer otherwise. That is why [clipboard.relic](../../examples/ref/graphics/clipboard.relic) runs in a build without SDL.

`examples/input_demo.relic` reads the keyboard and the mouse. `examples/mini_desktop/` is a longer event loop.
