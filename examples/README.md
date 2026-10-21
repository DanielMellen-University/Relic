# Examples

Small programs that show the language at work. They are examples, not the MONOLITH 2 environment, which lives in [M0N0LITH-2](https://github.com/DanielMellen-University/M0N0LITH-2).

All three need a graphics build. Run the commands from the repo root unless the table says otherwise.

```
cmake -S . -B build-sdl -DRELIC_SDL=ON     # needs SDL2 (apt install libsdl2-dev, brew install sdl2)
cmake --build build-sdl
```

| Example | Shows | Run |
|---|---|---|
| `hello_window.relic` | opening a window, drawing shapes, a 2-second loop | `build-sdl/relic examples/hello_window.relic` |
| `input_demo.relic` | keyboard and mouse input, text, a BMP image (`sprite.bmp`) | `build-sdl/relic examples/input_demo.relic` (from the repo root; Escape quits) |
| [`mini_desktop/`](mini_desktop/README.md) | one program with modules as values, an event loop, focus, files and JSON settings | `cd examples/mini_desktop && ../../build-sdl/relic desktop.relic` |

- Add `SDL_VIDEODRIVER=dummy` in front to run with no visible window.
- In a build without SDL, each one prints a short message saying it needs a graphics build.
- For programs without graphics, see the [guide](../docs/GUIDE.md) and the golden tests in `tests/golden/`.
