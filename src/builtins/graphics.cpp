// Graphics primitives (M11), backed by SDL2 when built with -DRELIC_SDL=ON.
// One window at a time. Without SDL every call except host_gfx_available()
// stops with a message saying how to rebuild. std/graphics.relic wraps these.
#include "builtins/graphics.hpp"

#include <cmath>
#include <string>

#ifdef RELIC_HAVE_SDL
#include <SDL.h>
#endif

namespace relic {

namespace {

double num(const Value& v, const char* what) {
    if (!v.is<double>()) throw std::string(what) + " must be a number, got " + type_name(v);
    return v.as<double>();
}

int whole(const Value& v, const char* what) {
    double d = num(v, what);
    if (d != std::floor(d)) throw std::string(what) + " must be a whole number";
    return static_cast<int>(d);
}

int channel(const Value& v, const char* what) {
    int c = whole(v, what);
    if (c < 0 || c > 255) throw std::string(what) + " must be 0 to 255";
    return c;
}

#ifdef RELIC_HAVE_SDL
SDL_Window* g_window = nullptr;
SDL_Renderer* g_renderer = nullptr;

SDL_Renderer* need_window() {
    if (!g_renderer) throw std::string("no window is open; call open first");
    return g_renderer;
}

void close_window() {
    if (g_renderer) SDL_DestroyRenderer(g_renderer);
    if (g_window) SDL_DestroyWindow(g_window);
    g_renderer = nullptr;
    g_window = nullptr;
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
}
#else
[[noreturn]] void missing() {
    throw std::string("graphics are not built in; rebuild with cmake -DRELIC_SDL=ON (needs SDL2)");
}
#endif

}  // namespace

void add_graphics_builtins(const AddBuiltin& add) {
#ifdef RELIC_HAVE_SDL
    add("host_gfx_available", 0, [](std::vector<Value>&) { return Value(true); });
    add("host_gfx_open", 3, [](std::vector<Value>& a) {
        int w = whole(a[0], "width"), h = whole(a[1], "height");
        if (!a[2].is<std::string>()) throw std::string("title must be a string, got ") + type_name(a[2]);
        if (w <= 0 || h <= 0) throw std::string("width and height must be above 0");
        if (g_window) close_window();
        if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) throw std::string("cannot start video: ") + SDL_GetError();
        g_window = SDL_CreateWindow(a[2].as<std::string>().c_str(), SDL_WINDOWPOS_CENTERED,
                                    SDL_WINDOWPOS_CENTERED, w, h, 0);
        if (!g_window) {
            std::string err = SDL_GetError();
            close_window();
            throw std::string("cannot open a window: ") + err;
        }
        g_renderer = SDL_CreateRenderer(g_window, -1, 0);
        if (!g_renderer) g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_SOFTWARE);
        if (!g_renderer) {
            std::string err = SDL_GetError();
            close_window();
            throw std::string("cannot draw in the window: ") + err;
        }
        return Value();
    });
    add("host_gfx_close", 0, [](std::vector<Value>&) {
        close_window();
        return Value();
    });
    add("host_gfx_clear", 3, [](std::vector<Value>& a) {
        SDL_Renderer* r = need_window();
        SDL_SetRenderDrawColor(r, channel(a[0], "red"), channel(a[1], "green"), channel(a[2], "blue"), 255);
        SDL_RenderClear(r);
        return Value();
    });
    add("host_gfx_rect", 7, [](std::vector<Value>& a) {
        SDL_Renderer* r = need_window();
        SDL_Rect rect{whole(a[0], "x"), whole(a[1], "y"), whole(a[2], "width"), whole(a[3], "height")};
        SDL_SetRenderDrawColor(r, channel(a[4], "red"), channel(a[5], "green"), channel(a[6], "blue"), 255);
        SDL_RenderFillRect(r, &rect);
        return Value();
    });
    add("host_gfx_pixel", 5, [](std::vector<Value>& a) {
        SDL_Renderer* r = need_window();
        SDL_SetRenderDrawColor(r, channel(a[2], "red"), channel(a[3], "green"), channel(a[4], "blue"), 255);
        SDL_RenderDrawPoint(r, whole(a[0], "x"), whole(a[1], "y"));
        return Value();
    });
    add("host_gfx_present", 0, [](std::vector<Value>&) {
        SDL_RenderPresent(need_window());
        return Value();
    });
    // Handles pending window events. true once the user asked to close the window.
    add("host_gfx_quit_requested", 0, [](std::vector<Value>&) {
        need_window();
        bool quit = false;
        SDL_Event e;
        while (SDL_PollEvent(&e))
            if (e.type == SDL_QUIT) quit = true;
        return Value(quit);
    });
    add("host_gfx_wait", 1, [](std::vector<Value>& a) {
        int ms = whole(a[0], "milliseconds");
        if (ms > 0) SDL_Delay(static_cast<Uint32>(ms));
        return Value();
    });
#else
    add("host_gfx_available", 0, [](std::vector<Value>&) { return Value(false); });
    const std::pair<const char*, int> stubs[] = {
        {"host_gfx_open", 3}, {"host_gfx_close", 0}, {"host_gfx_clear", 3}, {"host_gfx_rect", 7},
        {"host_gfx_pixel", 5}, {"host_gfx_present", 0}, {"host_gfx_quit_requested", 0}, {"host_gfx_wait", 1},
    };
    for (const auto& s : stubs) add(s.first, s.second, [](std::vector<Value>&) -> Value { missing(); });
    (void)channel;
#endif
}

}  // namespace relic
