// Graphics primitives (M11), backed by SDL2 when built with -DRELIC_SDL=ON.
// One window at a time. Without SDL every call except host_gfx_available()
// stops with a message saying how to rebuild. std/graphics.relic wraps these.
#include "builtins/graphics.hpp"

#include <cctype>
#include <cmath>
#include <deque>
#include <string>
#include <vector>

#ifdef RELIC_HAVE_SDL
#include <SDL.h>
#include "builtins/font8x12.inc"
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
bool g_quit = false;
std::deque<std::string> g_keys;      // keys pressed since the last next_key()
std::vector<SDL_Texture*> g_images;  // index = image id; freed on close
std::vector<std::pair<int, int>> g_sizes;

std::string lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

// Drains SDL's queue into our state. Every input call runs this first.
void pump() {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) g_quit = true;
        else if (e.type == SDL_KEYDOWN && !e.key.repeat) {
            g_keys.push_back(lower(SDL_GetKeyName(e.key.keysym.sym)));
            if (g_keys.size() > 64) g_keys.pop_front();
        }
    }
}

std::string str(const Value& v, const char* what) {
    if (!v.is<std::string>()) throw std::string(what) + " must be a string, got " + type_name(v);
    return v.as<std::string>();
}

int image_id(const Value& v) {
    int id = whole(v, "image");
    if (id < 0 || id >= static_cast<int>(g_images.size()) || !g_images[id]) throw std::string("not an open image");
    return id;
}

SDL_Renderer* need_window() {
    if (!g_renderer) throw std::string("no window is open; call open first");
    return g_renderer;
}

void close_window() {
    for (SDL_Texture* t : g_images)
        if (t) SDL_DestroyTexture(t);
    g_images.clear();
    g_sizes.clear();
    g_keys.clear();
    g_quit = false;
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
        pump();
        return Value(g_quit);
    });
    add("host_gfx_key_down", 1, [](std::vector<Value>& a) {
        need_window();
        pump();
        std::string name = str(a[0], "key");
        SDL_Keycode k = SDL_GetKeyFromName(name.c_str());
        if (k == SDLK_UNKNOWN) throw std::string("unknown key name: ") + name;
        const Uint8* st = SDL_GetKeyboardState(nullptr);
        return Value(st[SDL_GetScancodeFromKey(k)] != 0);
    });
    add("host_gfx_next_key", 0, [](std::vector<Value>&) {
        need_window();
        pump();
        if (g_keys.empty()) return Value();
        std::string k = g_keys.front();
        g_keys.pop_front();
        return Value(k);
    });
    add("host_gfx_mouse_x", 0, [](std::vector<Value>&) {
        need_window();
        pump();
        int x = 0;
        SDL_GetMouseState(&x, nullptr);
        return Value(static_cast<double>(x));
    });
    add("host_gfx_mouse_y", 0, [](std::vector<Value>&) {
        need_window();
        pump();
        int y = 0;
        SDL_GetMouseState(nullptr, &y);
        return Value(static_cast<double>(y));
    });
    add("host_gfx_mouse_down", 1, [](std::vector<Value>& a) {
        need_window();
        pump();
        int b = whole(a[0], "button");
        if (b < 1 || b > 3) throw std::string("button must be 1 (left), 2 (middle) or 3 (right)");
        return Value((SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON(b)) != 0);
    });
    // Built-in 8x12 font, ASCII only; other characters draw as '?'.
    add("host_gfx_text", 7, [](std::vector<Value>& a) {
        SDL_Renderer* r = need_window();
        int x = whole(a[0], "x"), y = whole(a[1], "y");
        std::string t = str(a[2], "text");
        int sc = whole(a[3], "scale");
        if (sc < 1) throw std::string("scale must be 1 or more");
        SDL_SetRenderDrawColor(r, channel(a[4], "red"), channel(a[5], "green"), channel(a[6], "blue"), 255);
        int cx = x;
        for (unsigned char c : t) {
            if (c == '\n') { cx = x; y += 12 * sc; continue; }
            if (c < 32 || c > 126) c = '?';
            const unsigned char* g = kFont8x12[c - 32];
            for (int row = 0; row < 12; row++)
                for (int col = 0; col < 8; col++)
                    if (g[row] & (0x80 >> col)) {
                        SDL_Rect px{cx + col * sc, y + row * sc, sc, sc};
                        SDL_RenderFillRect(r, &px);
                    }
            cx += 8 * sc;
        }
        return Value();
    });
    add("host_gfx_image_load", 1, [](std::vector<Value>& a) {
        SDL_Renderer* r = need_window();
        std::string path = str(a[0], "path");
        SDL_Surface* surf = SDL_LoadBMP(path.c_str());
        if (!surf) throw std::string("cannot load image ") + path + ": " + SDL_GetError();
        SDL_Texture* t = SDL_CreateTextureFromSurface(r, surf);
        int w = surf->w, h = surf->h;
        SDL_FreeSurface(surf);
        if (!t) throw std::string("cannot use image ") + path + ": " + SDL_GetError();
        g_images.push_back(t);
        g_sizes.push_back({w, h});
        return Value(static_cast<double>(g_images.size() - 1));
    });
    add("host_gfx_image", 3, [](std::vector<Value>& a) {
        SDL_Renderer* r = need_window();
        int id = image_id(a[0]);
        SDL_Rect dst{whole(a[1], "x"), whole(a[2], "y"), g_sizes[id].first, g_sizes[id].second};
        SDL_RenderCopy(r, g_images[id], nullptr, &dst);
        return Value();
    });
    add("host_gfx_image_width", 1, [](std::vector<Value>& a) {
        need_window();
        return Value(static_cast<double>(g_sizes[image_id(a[0])].first));
    });
    add("host_gfx_image_height", 1, [](std::vector<Value>& a) {
        need_window();
        return Value(static_cast<double>(g_sizes[image_id(a[0])].second));
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
        {"host_gfx_key_down", 1}, {"host_gfx_next_key", 0}, {"host_gfx_mouse_x", 0}, {"host_gfx_mouse_y", 0},
        {"host_gfx_mouse_down", 1}, {"host_gfx_text", 7}, {"host_gfx_image_load", 1}, {"host_gfx_image", 3},
        {"host_gfx_image_width", 1}, {"host_gfx_image_height", 1},
    };
    for (const auto& s : stubs) add(s.first, s.second, [](std::vector<Value>&) -> Value { missing(); });
    (void)channel;
#endif
}

}  // namespace relic
