// Graphics primitives (M11), backed by SDL2 when built with -DRELIC_SDL=ON.
// One window at a time. Without SDL every call except host_gfx_available()
// stops with a message saying how to rebuild. std/graphics.relic wraps these.
#include "builtins/graphics.hpp"

#include <cctype>
#include <cstring>
#include <cmath>
#include <deque>
#include <string>
#include <vector>

#ifdef RELIC_HAVE_SDL
#include <SDL.h>
#include "builtins/font8x12.inc"
#include "builtins/gfx_scale.h"
#ifdef RELIC_HAVE_SDL_IMAGE
#include <SDL_image.h>
#endif
#ifdef RELIC_HAVE_SDL_TTF
#include <SDL_ttf.h>
#endif
#endif

namespace relic {

namespace {

double num(const Value& v, const char* what) {
    if (!is_num(v)) throw std::string(what) + " must be a number, got " + type_name(v);
    return to_num(v);
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

std::string g_clipboard;

std::string str_any(const Value& v) {
    if (!v.is<std::string>()) throw std::string("text must be a string, got ") + type_name(v);
    return v.as<std::string>();
}  // used when there is no SDL window

#ifdef RELIC_HAVE_SDL
Value event(const char* type, std::initializer_list<std::pair<const char*, Value>> fields) {
    auto m = std::make_shared<Map>();
    m->set("type", Value(std::string(type)));
    for (auto& f : fields) m->set(f.first, f.second);
    return Value(m);
}

SDL_Window* g_window = nullptr;
SDL_Renderer* g_renderer = nullptr;
bool g_quit = false;
std::deque<std::string> g_keys;      // keys pressed since the last next_key()
std::vector<SDL_Texture*> g_images;  // index = image id; freed on close
std::vector<std::pair<int, int>> g_sizes;
std::deque<Value> g_events;          // event maps since the last next_event()
Uint64 g_opened = 0;                 // performance counter when the window opened
Uint64 g_last_present = 0;           // performance counter at the last present, 0 before the first
double g_frame_time = 0;             // seconds between the last two presents

// System cursors for set_cursor, made on first use and freed on close.
struct CursorName { const char* name; SDL_SystemCursor id; };
const CursorName kCursors[] = {
    {"arrow", SDL_SYSTEM_CURSOR_ARROW}, {"text", SDL_SYSTEM_CURSOR_IBEAM}, {"wait", SDL_SYSTEM_CURSOR_WAIT},
    {"crosshair", SDL_SYSTEM_CURSOR_CROSSHAIR}, {"hand", SDL_SYSTEM_CURSOR_HAND}, {"move", SDL_SYSTEM_CURSOR_SIZEALL},
    {"no", SDL_SYSTEM_CURSOR_NO}, {"resize_ew", SDL_SYSTEM_CURSOR_SIZEWE}, {"resize_ns", SDL_SYSTEM_CURSOR_SIZENS},
    {"resize_nwse", SDL_SYSTEM_CURSOR_SIZENWSE}, {"resize_nesw", SDL_SYSTEM_CURSOR_SIZENESW},
};
constexpr int kNumCursors = sizeof(kCursors) / sizeof(kCursors[0]);
SDL_Cursor* g_cursors[kNumCursors] = {};
#ifdef RELIC_HAVE_SDL_TTF
// A font is opened at size * scale() pixels; when the scale changes it is
// opened again at the new size.
struct Font {
    TTF_Font* f;
    std::string path;
    int size;      // in window units, as asked for
    double scale;  // the scale f was opened at
};
std::vector<Font> g_fonts;  // index = font id; freed on close

TTF_Font* font_of(const Value& v);
#endif

double current_scale();
void sync_scale();

std::string lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

// Turns one SDL event into our state: an event map, a key press, quit.
void take(const SDL_Event& e) {
    using I = long long;
    if (e.type == SDL_QUIT) {
        g_quit = true;
        g_events.push_back(event("quit", {}));
    } else if (e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) {
        std::string k = lower(SDL_GetKeyName(e.key.keysym.sym));
        if (e.type == SDL_KEYDOWN && !e.key.repeat) {
            g_keys.push_back(k);
            if (g_keys.size() > 64) g_keys.pop_front();
        }
        Uint16 mod = e.key.keysym.mod;
        g_events.push_back(event(e.type == SDL_KEYDOWN ? "key_down" : "key_up",
            {{"key", Value(k)}, {"repeat", Value(e.key.repeat != 0)}, {"shift", Value((mod & KMOD_SHIFT) != 0)},
             {"ctrl", Value((mod & KMOD_CTRL) != 0)}, {"alt", Value((mod & KMOD_ALT) != 0)},
             {"gui", Value((mod & KMOD_GUI) != 0)}}));
    } else if (e.type == SDL_TEXTINPUT) {
        g_events.push_back(event("text", {{"text", Value(std::string(e.text.text))}}));
    } else if (e.type == SDL_MOUSEBUTTONDOWN || e.type == SDL_MOUSEBUTTONUP) {
        g_events.push_back(event(e.type == SDL_MOUSEBUTTONDOWN ? "mouse_down" : "mouse_up",
            {{"button", Value(I(e.button.button))}, {"x", Value(I(e.button.x))}, {"y", Value(I(e.button.y))},
             {"clicks", Value(I(e.button.clicks))}}));
    } else if (e.type == SDL_MOUSEMOTION) {
        g_events.push_back(event("mouse_move", {{"x", Value(I(e.motion.x))}, {"y", Value(I(e.motion.y))},
            {"dx", Value(I(e.motion.xrel))}, {"dy", Value(I(e.motion.yrel))}}));
    } else if (e.type == SDL_MOUSEWHEEL) {
        int dy = e.wheel.y, dx = e.wheel.x;
        if (e.wheel.direction == SDL_MOUSEWHEEL_FLIPPED) { dx = -dx; dy = -dy; }
        g_events.push_back(event("wheel", {{"dx", Value(I(dx))}, {"dy", Value(I(dy))}}));
    } else if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
        g_events.push_back(event("resize", {{"width", Value(I(e.window.data1))}, {"height", Value(I(e.window.data2))}}));
    }
    if (g_events.size() > 512) g_events.pop_front();
}

// Drains SDL's queue into our state. Every input call runs this first.
void pump() {
    SDL_Event e;
    while (SDL_PollEvent(&e)) take(e);
    sync_scale();
}

// A simulated event goes after what SDL already holds, straight into our
// state. It does not go through SDL_PushEvent: sdl2-compat (SDL2 over SDL3,
// which is what Homebrew's sdl2 is) crashes on a pushed SDL_TEXTINPUT and
// loses a pushed wheel's distance.
void simulated(const SDL_Event& e) {
    pump();
    take(e);
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

#ifdef RELIC_HAVE_SDL_TTF
TTF_Font* font_of(const Value& v) {
    int id = whole(v, "font");
    if (id < 0 || id >= static_cast<int>(g_fonts.size()) || !g_fonts[id].f) throw std::string("not an open font");
    Font& font = g_fonts[id];
    double sc = current_scale();
    if (sc != font.scale) {
        // Opened again for the new scale; the old one stays if that fails.
        TTF_Font* f = TTF_OpenFont(font.path.c_str(), gfx_font_pixels(font.size, sc));
        if (f) {
            TTF_CloseFont(font.f);
            font.f = f;
            font.scale = sc;
        }
    }
    return font.f;
}
#endif

SDL_Renderer* need_window() {
    if (!g_renderer) throw std::string("no window is open; call open first");
    return g_renderer;
}

// Pixels per window unit: the drawable size over the window size. While a
// canvas is the target SDL reports the canvas size as the output size, so
// the last value seen with the window as the target is used then.
double g_win_scale = 1;
double current_scale() {
    if (SDL_GetRenderTarget(g_renderer)) return g_win_scale;
    int pw = 0, ph = 0, ww = 0, wh = 0;
    SDL_GetRendererOutputSize(g_renderer, &pw, &ph);
    SDL_GetWindowSize(g_window, &ww, &wh);
    g_win_scale = gfx_scale_of(pw, ww);
    return g_win_scale;
}

// Scales the renderer by scale(), so drawing stays in window units on a
// high-DPI screen. Canvases keep scale 1 (SDL saves the window's scale while
// one is the target), so this waits until drawing is back on the window.
void sync_scale() {
    if (!g_renderer || SDL_GetRenderTarget(g_renderer)) return;
    float s = static_cast<float>(current_scale()), sx = 1, sy = 1;
    SDL_RenderGetScale(g_renderer, &sx, &sy);
    if (sx != s || sy != s) SDL_RenderSetScale(g_renderer, s, s);
}

bool on_off(const Value& v, const char* what) {
    if (!v.is<bool>()) throw std::string(what) + " must be true or false, got " + type_name(v);
    return v.as<bool>();
}

Value pair(int a, int b) {
    auto l = std::make_shared<List>();
    l->push_back(Value(static_cast<long long>(a)));
    l->push_back(Value(static_cast<long long>(b)));
    return Value(l);
}

// Shows the frame and notes how long it was since the last one.
void present_frame() {
    SDL_RenderPresent(g_renderer);
    Uint64 now = SDL_GetPerformanceCounter();
    if (g_last_present) g_frame_time = static_cast<double>(now - g_last_present) / static_cast<double>(SDL_GetPerformanceFrequency());
    g_last_present = now;
    sync_scale();  // the window may have moved to a screen with another scale
}

void close_window() {
    for (SDL_Texture* t : g_images)
        if (t) SDL_DestroyTexture(t);
    g_images.clear();
    g_sizes.clear();
#ifdef RELIC_HAVE_SDL_TTF
    for (Font& f : g_fonts)
        if (f.f) TTF_CloseFont(f.f);
    g_fonts.clear();
#endif
    g_keys.clear();
    g_events.clear();
    g_quit = false;
    g_opened = g_last_present = 0;
    g_frame_time = 0;
    g_win_scale = 1;
    for (SDL_Cursor*& c : g_cursors) {
        if (c) SDL_FreeCursor(c);
        c = nullptr;
    }
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
#ifdef SDL_HINT_WINDOWS_DPI_SCALING
        // Windows: window sizes in scaled units and the drawable in pixels, as on macOS.
        SDL_SetHint(SDL_HINT_WINDOWS_DPI_SCALING, "1");
#endif
        if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) throw std::string("cannot start video: ") + SDL_GetError();
        g_window = SDL_CreateWindow(a[2].as<std::string>().c_str(), SDL_WINDOWPOS_CENTERED,
                                    SDL_WINDOWPOS_CENTERED, w, h, SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
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
        SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_BLEND);
        SDL_StartTextInput();
        sync_scale();
        g_opened = SDL_GetPerformanceCounter();
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
        need_window();
        present_frame();
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
        return Value(static_cast<long long>(x));
    });
    add("host_gfx_mouse_y", 0, [](std::vector<Value>&) {
        need_window();
        pump();
        int y = 0;
        SDL_GetMouseState(nullptr, &y);
        return Value(static_cast<long long>(y));
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
#ifdef RELIC_HAVE_SDL_IMAGE
        SDL_Surface* surf = IMG_Load(path.c_str());
#else
        auto ends = [&](const char* ext) {
            std::string e = ext;
            return path.size() >= e.size() && lower(path.substr(path.size() - e.size())) == e;
        };
        if (!ends(".bmp"))
            throw std::string("cannot load image ") + path + ": only BMP is built in; rebuild with SDL2_image installed for PNG";
        SDL_Surface* surf = SDL_LoadBMP(path.c_str());
#endif
        if (!surf) throw std::string("cannot load image ") + path + ": " + SDL_GetError();
        SDL_Texture* t = SDL_CreateTextureFromSurface(r, surf);
        int w = surf->w, h = surf->h;
        SDL_FreeSurface(surf);
        if (!t) throw std::string("cannot use image ") + path + ": " + SDL_GetError();
        g_images.push_back(t);
        g_sizes.push_back({w, h});
        return Value(static_cast<long long>(g_images.size() - 1));
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
        return Value(static_cast<long long>(g_sizes[image_id(a[0])].first));
    });
    add("host_gfx_image_height", 1, [](std::vector<Value>& a) {
        need_window();
        return Value(static_cast<long long>(g_sizes[image_id(a[0])].second));
    });
    add("host_gfx_has_png", 0, [](std::vector<Value>&) {
#ifdef RELIC_HAVE_SDL_IMAGE
        return Value(true);
#else
        return Value(false);
#endif
    });
    add("host_gfx_has_fonts", 0, [](std::vector<Value>&) {
#ifdef RELIC_HAVE_SDL_TTF
        return Value(true);
#else
        return Value(false);
#endif
    });
#ifdef RELIC_HAVE_SDL_TTF
    // TrueType text. load gives a font id; text is drawn blended (smooth edges).
    add("host_gfx_font_load", 2, [](std::vector<Value>& a) {
        need_window();
        std::string path = str(a[0], "path");
        int size = whole(a[1], "size");
        if (size < 1) throw std::string("size must be 1 or more");
        if (!TTF_WasInit() && TTF_Init() != 0) throw std::string("cannot start fonts: ") + TTF_GetError();
        double sc = current_scale();
        TTF_Font* f = TTF_OpenFont(path.c_str(), gfx_font_pixels(size, sc));
        if (!f) throw std::string("cannot load font ") + path + ": " + TTF_GetError();
        g_fonts.push_back(Font{f, path, size, sc});
        return Value(static_cast<long long>(g_fonts.size() - 1));
    });
    add("host_gfx_font_text", 7, [](std::vector<Value>& a) {
        SDL_Renderer* r = need_window();
        TTF_Font* f = font_of(a[0]);
        int x = whole(a[1], "x"), y = whole(a[2], "y");
        std::string t = str(a[3], "text");
        SDL_Color c{static_cast<Uint8>(channel(a[4], "red")), static_cast<Uint8>(channel(a[5], "green")),
                    static_cast<Uint8>(channel(a[6], "blue")), 255};
        if (t.empty()) return Value();
        SDL_Surface* s = TTF_RenderUTF8_Blended(f, t.c_str(), c);
        if (!s) throw std::string("cannot draw text: ") + TTF_GetError();
        SDL_Texture* tex = SDL_CreateTextureFromSurface(r, s);
        // The text is s->w by s->h pixels; in window units that is 1/scale of it.
        double sc = current_scale();
        SDL_FRect dst{static_cast<float>(x), static_cast<float>(y), static_cast<float>(s->w / sc),
                      static_cast<float>(s->h / sc)};
        SDL_FreeSurface(s);
        if (tex) {
            SDL_RenderCopyF(r, tex, nullptr, &dst);
            SDL_DestroyTexture(tex);
        }
        return Value();
    });
    add("host_gfx_font_size", 2, [](std::vector<Value>& a) {
        need_window();
        TTF_Font* f = font_of(a[0]);
        std::string t = str(a[1], "text");
        int w = 0, h = TTF_FontHeight(f);
        if (!t.empty() && TTF_SizeUTF8(f, t.c_str(), &w, &h) != 0) throw std::string("cannot measure text: ") + TTF_GetError();
        double sc = current_scale();
        return pair(gfx_units_up(w, sc), gfx_units_up(h, sc));
    });
#else
    const std::pair<const char*, int> no_fonts[] = {{"host_gfx_font_load", 2}, {"host_gfx_font_text", 7}, {"host_gfx_font_size", 2}};
    for (const auto& s : no_fonts)
        add(s.first, s.second, [](std::vector<Value>&) -> Value {
            throw std::string("TrueType fonts are not built in; rebuild with SDL2_ttf installed");
        });
#endif
    add("host_gfx_next_event", 0, [](std::vector<Value>&) {
        need_window();
        pump();
        if (g_events.empty()) return Value();
        Value v = g_events.front();
        g_events.pop_front();
        return v;
    });
    // Puts a made-up event in SDL's queue, for tests and scripted demos.
    // kind / a / b:
    //   "key"/"keyup"  a=name  b=none or "shift"/"ctrl"/"alt"/"gui" (join with +)
    //   "text"         a=text  b=none
    //   "click"        a=x     b=y     (left down then up)
    //   "mousedown"/"mouseup"  a=x  b=y  (left button)
    //   "move"         a=x     b=y     (dx and dy are 0)
    //   "wheel"        a=dx    b=dy
    //   "resize"       a=w     b=h
    //   "quit"         a=none  b=none
    add("host_gfx_simulate", 3, [](std::vector<Value>& a) {
        need_window();
        std::string kind = str(a[0], "kind");
        auto parse_mods = [](const Value& v) -> Uint16 {
            if (v.is<NoneV>()) return 0;
            std::string s = str(v, "mods");
            for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            Uint16 mod = 0;
            auto has = [&](const char* word) {
                auto p = s.find(word);
                if (p == std::string::npos) return false;
                auto end = p + std::strlen(word);
                bool left = p == 0 || s[p - 1] == '+' || s[p - 1] == ',' || s[p - 1] == ' ';
                bool right = end >= s.size() || s[end] == '+' || s[end] == ',' || s[end] == ' ';
                return left && right;
            };
            if (has("shift")) mod |= KMOD_SHIFT;
            if (has("ctrl")) mod |= KMOD_CTRL;
            if (has("alt")) mod |= KMOD_ALT;
            if (has("gui")) mod |= KMOD_GUI;
            if (mod == 0 && !s.empty()) throw std::string("unknown modifier: ") + str(v, "mods");
            return mod;
        };
        SDL_Event e{};
        if (kind == "key" || kind == "keyup") {
            e.type = kind == "key" ? SDL_KEYDOWN : SDL_KEYUP;
            e.key.keysym.sym = SDL_GetKeyFromName(str(a[1], "key").c_str());
            if (e.key.keysym.sym == SDLK_UNKNOWN) throw std::string("unknown key name");
            e.key.keysym.mod = parse_mods(a[2]);
            simulated(e);
        } else if (kind == "text") {
            std::string t = str(a[1], "text");
            if (t.size() >= sizeof(e.text.text)) throw std::string("text is too long");
            e.type = SDL_TEXTINPUT;
            std::copy(t.begin(), t.end(), e.text.text);
            simulated(e);
        } else if (kind == "click") {
            for (Uint32 t : {SDL_MOUSEBUTTONDOWN, SDL_MOUSEBUTTONUP}) {
                SDL_Event b{};
                b.type = t;
                b.button.button = SDL_BUTTON_LEFT;
                b.button.clicks = 1;
                b.button.x = whole(a[1], "x");
                b.button.y = whole(a[2], "y");
                simulated(b);
            }
        } else if (kind == "mousedown" || kind == "mouseup") {
            e.type = kind == "mousedown" ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
            e.button.button = SDL_BUTTON_LEFT;
            e.button.clicks = 1;
            e.button.x = whole(a[1], "x");
            e.button.y = whole(a[2], "y");
            simulated(e);
        } else if (kind == "move") {
            e.type = SDL_MOUSEMOTION;
            e.motion.x = whole(a[1], "x");
            e.motion.y = whole(a[2], "y");
            e.motion.xrel = 0;
            e.motion.yrel = 0;
            simulated(e);
        } else if (kind == "wheel") {
            e.type = SDL_MOUSEWHEEL;
            e.wheel.x = whole(a[1], "dx");
            e.wheel.y = whole(a[2], "dy");
            simulated(e);
        } else if (kind == "resize") {
            e.type = SDL_WINDOWEVENT;
            e.window.event = SDL_WINDOWEVENT_SIZE_CHANGED;
            e.window.data1 = whole(a[1], "width");
            e.window.data2 = whole(a[2], "height");
            simulated(e);
        } else if (kind == "quit") {
            e.type = SDL_QUIT;
            simulated(e);
        } else {
            throw std::string("unknown event kind: ") + kind;
        }
        return Value();
    });
    add("host_gfx_size", 0, [](std::vector<Value>&) {
        need_window();
        int w = 0, h = 0;
        SDL_GetRendererOutputSize(g_renderer, &w, &h);
        auto l = std::make_shared<List>();
        l->push_back(Value(static_cast<long long>(w)));
        l->push_back(Value(static_cast<long long>(h)));
        return Value(l);
    });
    add("host_gfx_line", 7, [](std::vector<Value>& a) {
        SDL_Renderer* r = need_window();
        SDL_SetRenderDrawColor(r, channel(a[4], "red"), channel(a[5], "green"), channel(a[6], "blue"), 255);
        SDL_RenderDrawLine(r, whole(a[0], "x1"), whole(a[1], "y1"), whole(a[2], "x2"), whole(a[3], "y2"));
        return Value();
    });
    add("host_gfx_outline", 7, [](std::vector<Value>& a) {
        SDL_Renderer* r = need_window();
        SDL_Rect rect{whole(a[0], "x"), whole(a[1], "y"), whole(a[2], "width"), whole(a[3], "height")};
        SDL_SetRenderDrawColor(r, channel(a[4], "red"), channel(a[5], "green"), channel(a[6], "blue"), 255);
        SDL_RenderDrawRect(r, &rect);
        return Value();
    });
    // Rect with alpha 0 (clear) to 255 (solid), blended over what is there.
    add("host_gfx_rect_alpha", 8, [](std::vector<Value>& a) {
        SDL_Renderer* r = need_window();
        SDL_Rect rect{whole(a[0], "x"), whole(a[1], "y"), whole(a[2], "width"), whole(a[3], "height")};
        SDL_SetRenderDrawColor(r, channel(a[4], "red"), channel(a[5], "green"), channel(a[6], "blue"), channel(a[7], "alpha"));
        SDL_RenderFillRect(r, &rect);
        return Value();
    });
    // Limits drawing to a rectangle; width or height 0 or less turns it off.
    add("host_gfx_clip", 4, [](std::vector<Value>& a) {
        SDL_Renderer* r = need_window();
        SDL_Rect rect{whole(a[0], "x"), whole(a[1], "y"), whole(a[2], "width"), whole(a[3], "height")};
        SDL_RenderSetClipRect(r, rect.w > 0 && rect.h > 0 ? &rect : nullptr);
        return Value();
    });
    // Canvas: an image you can draw into, for paint-style apps that keep
    // what was drawn. draw_to(id) sends drawing there; draw_to(none) back to the window.
    add("host_gfx_canvas", 2, [](std::vector<Value>& a) {
        SDL_Renderer* r = need_window();
        int w = whole(a[0], "width"), h = whole(a[1], "height");
        if (w <= 0 || h <= 0) throw std::string("width and height must be above 0");
        SDL_Texture* t = SDL_CreateTexture(r, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, w, h);
        if (!t) throw std::string("cannot make a canvas: ") + SDL_GetError();
        SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
        g_images.push_back(t);
        g_sizes.push_back({w, h});
        return Value(static_cast<long long>(g_images.size() - 1));
    });
    add("host_gfx_draw_to", 1, [](std::vector<Value>& a) {
        SDL_Renderer* r = need_window();
        SDL_Texture* t = a[0].is<NoneV>() ? nullptr : g_images[image_id(a[0])];
        if (SDL_SetRenderTarget(r, t) != 0) throw std::string("cannot draw into that image: ") + SDL_GetError();
        sync_scale();
        return Value();
    });
    // ---- the window: title, size, fullscreen, pixel scale ----
    add("host_gfx_title", 0, [](std::vector<Value>&) {
        need_window();
        return Value(std::string(SDL_GetWindowTitle(g_window)));
    });
    add("host_gfx_set_title", 1, [](std::vector<Value>& a) {
        need_window();
        if (!a[0].is<std::string>()) throw std::string("title must be a string, got ") + type_name(a[0]);
        SDL_SetWindowTitle(g_window, a[0].as<std::string>().c_str());
        return Value();
    });
    // In window units, which mouse positions use. size() is in pixels.
    add("host_gfx_window_size", 0, [](std::vector<Value>&) {
        need_window();
        int w = 0, h = 0;
        SDL_GetWindowSize(g_window, &w, &h);
        return pair(w, h);
    });
    add("host_gfx_set_window_size", 2, [](std::vector<Value>& a) {
        need_window();
        int w = whole(a[0], "width"), h = whole(a[1], "height");
        if (w <= 0 || h <= 0) throw std::string("width and height must be above 0");
        SDL_SetWindowSize(g_window, w, h);
        sync_scale();
        return Value();
    });
    // Pixels per window unit across: size()[0] / window_size()[0].
    add("host_gfx_scale", 0, [](std::vector<Value>&) {
        need_window();
        return Value(current_scale());
    });
    add("host_gfx_fullscreen", 0, [](std::vector<Value>&) {
        need_window();
        return Value((SDL_GetWindowFlags(g_window) & SDL_WINDOW_FULLSCREEN) != 0);
    });
    add("host_gfx_set_fullscreen", 1, [](std::vector<Value>& a) {
        need_window();
        bool on = on_off(a[0], "on");
        if (SDL_SetWindowFullscreen(g_window, on ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0) != 0)
            throw std::string("cannot change fullscreen: ") + SDL_GetError();
        sync_scale();
        return Value();
    });
    // ---- the mouse cursor ----
    // true once the cursor is set; false when this system cannot show that shape.
    add("host_gfx_set_cursor", 1, [](std::vector<Value>& a) {
        need_window();
        std::string name = str(a[0], "cursor");
        for (int i = 0; i < kNumCursors; i++) {
            if (name != kCursors[i].name) continue;
            if (!g_cursors[i]) g_cursors[i] = SDL_CreateSystemCursor(kCursors[i].id);
            if (!g_cursors[i]) return Value(false);
            SDL_SetCursor(g_cursors[i]);
            return Value(true);
        }
        throw std::string("unknown cursor: ") + name +
            "; use arrow, text, wait, crosshair, hand, move, no, resize_ew, resize_ns, resize_nwse or resize_nesw";
    });
    add("host_gfx_show_cursor", 1, [](std::vector<Value>& a) {
        need_window();
        SDL_ShowCursor(on_off(a[0], "on") ? SDL_ENABLE : SDL_DISABLE);
        return Value();
    });
    add("host_gfx_cursor_visible", 0, [](std::vector<Value>&) {
        need_window();
        return Value(SDL_ShowCursor(SDL_QUERY) == SDL_ENABLE);
    });
    // ---- frames: vsync and timing ----
    // true when the renderer took the setting.
    add("host_gfx_set_vsync", 1, [](std::vector<Value>& a) {
        SDL_Renderer* r = need_window();
        return Value(SDL_RenderSetVSync(r, on_off(a[0], "on") ? 1 : 0) == 0);
    });
    add("host_gfx_vsync", 0, [](std::vector<Value>&) {
        SDL_Renderer* r = need_window();
        SDL_RendererInfo info{};
        if (SDL_GetRendererInfo(r, &info) != 0) return Value(false);
        return Value((info.flags & SDL_RENDERER_PRESENTVSYNC) != 0);
    });
    // Whole milliseconds since open.
    add("host_gfx_ticks", 0, [](std::vector<Value>&) {
        need_window();
        double d = static_cast<double>(SDL_GetPerformanceCounter() - g_opened);
        return Value(static_cast<long long>(d * 1000.0 / static_cast<double>(SDL_GetPerformanceFrequency())));
    });
    // Seconds between the last two presents; 0 until there have been two.
    add("host_gfx_frame_time", 0, [](std::vector<Value>&) {
        need_window();
        return Value(g_frame_time);
    });
    // present() that first waits until 1/fps seconds have passed since the last one.
    add("host_gfx_present_capped", 1, [](std::vector<Value>& a) {
        need_window();
        double fps = num(a[0], "fps");
        if (!(fps > 0)) throw std::string("fps must be above 0");
        if (g_last_present) {
            double freq = static_cast<double>(SDL_GetPerformanceFrequency());
            double left = 1.0 / fps - static_cast<double>(SDL_GetPerformanceCounter() - g_last_present) / freq;
            if (left > 0) SDL_Delay(static_cast<Uint32>(left * 1000));
            while (static_cast<double>(SDL_GetPerformanceCounter() - g_last_present) / freq < 1.0 / fps) {
            }
        }
        present_frame();
        return Value();
    });
    add("host_gfx_wait", 1, [](std::vector<Value>& a) {
        int ms = whole(a[0], "milliseconds");
        if (ms > 0) SDL_Delay(static_cast<Uint32>(ms));
        return Value();
    });
#else
    add("host_gfx_available", 0, [](std::vector<Value>&) { return Value(false); });
    add("host_gfx_has_png", 0, [](std::vector<Value>&) { return Value(false); });
    add("host_gfx_has_fonts", 0, [](std::vector<Value>&) { return Value(false); });
    const std::pair<const char*, int> stubs[] = {
        {"host_gfx_open", 3}, {"host_gfx_close", 0}, {"host_gfx_clear", 3}, {"host_gfx_rect", 7},
        {"host_gfx_pixel", 5}, {"host_gfx_present", 0}, {"host_gfx_quit_requested", 0}, {"host_gfx_wait", 1},
        {"host_gfx_key_down", 1}, {"host_gfx_next_key", 0}, {"host_gfx_mouse_x", 0}, {"host_gfx_mouse_y", 0},
        {"host_gfx_mouse_down", 1}, {"host_gfx_text", 7}, {"host_gfx_image_load", 1}, {"host_gfx_image", 3},
        {"host_gfx_image_width", 1}, {"host_gfx_image_height", 1},
        {"host_gfx_font_load", 2}, {"host_gfx_font_text", 7}, {"host_gfx_font_size", 2},
        {"host_gfx_next_event", 0}, {"host_gfx_simulate", 3}, {"host_gfx_size", 0}, {"host_gfx_line", 7},
        {"host_gfx_outline", 7}, {"host_gfx_rect_alpha", 8}, {"host_gfx_clip", 4},
        {"host_gfx_canvas", 2}, {"host_gfx_draw_to", 1},
        {"host_gfx_title", 0}, {"host_gfx_set_title", 1}, {"host_gfx_window_size", 0},
        {"host_gfx_set_window_size", 2}, {"host_gfx_scale", 0}, {"host_gfx_fullscreen", 0},
        {"host_gfx_set_fullscreen", 1}, {"host_gfx_set_cursor", 1}, {"host_gfx_show_cursor", 1},
        {"host_gfx_cursor_visible", 0}, {"host_gfx_set_vsync", 1}, {"host_gfx_vsync", 0},
        {"host_gfx_ticks", 0}, {"host_gfx_frame_time", 0}, {"host_gfx_present_capped", 1},
    };
    for (const auto& s : stubs) add(s.first, s.second, [](std::vector<Value>&) -> Value { missing(); });
    (void)channel;
#endif
    // Clipboard: the system one while an SDL window is open, else one kept in
    // this process, so text copy and paste work in tests too.
    add("host_clipboard_set", 1, [](std::vector<Value>& a) {
        g_clipboard = str_any(a[0]);
#ifdef RELIC_HAVE_SDL
        if (g_window) SDL_SetClipboardText(g_clipboard.c_str());
#endif
        return Value();
    });
    add("host_clipboard_get", 0, [](std::vector<Value>&) {
#ifdef RELIC_HAVE_SDL
        if (g_window && SDL_HasClipboardText()) {
            char* t = SDL_GetClipboardText();
            std::string s = t ? t : "";
            SDL_free(t);
            return Value(s);
        }
#endif
        return Value(g_clipboard);
    });
}

}  // namespace relic
