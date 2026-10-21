# Mini desktop

A small Relic app shaped like a desktop: taskbar, Notes, Terminal, JSON settings.

This is a language example. It shows how one program uses std/graphics, std/files and std/json together. It is not the MONOLITH 2 environment; the real desktop and its apps live in [M0N0LITH-2](https://github.com/DanielMellen-University/M0N0LITH-2).

```
cd examples/mini_desktop
SDL_VIDEODRIVER=dummy ../../build-sdl/relic desktop.relic
```

Drop `SDL_VIDEODRIVER=dummy` to open a real window. See [Running headless](../../docs/STDLIB.md#running-headless--testing-gui-apps).
