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
#ifdef RELIC_HAVE_SDL_TTF
std::vector<TTF_Font*> g_fonts;  // index = font id; freed on close

TTF_Font* font_of(const Value& v);
#endif

std::string lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

// Drains SDL's queue into our state. Every input call runs this first.
void pump() {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
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
                 {"ctrl", Value((mod & KMOD_CTRL) != 0)}, {"alt", Value((mod & KMOD_ALT) != 0)}}));
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
    if (id < 0 || id >= static_cast<int>(g_fonts.size()) || !g_fonts[id]) throw std::string("not an open font");
    return g_fonts[id];
}
#endif

SDL_Renderer* need_window() {
    if (!g_renderer) throw std::string("no window is open; call open first");
    return g_renderer;
}

void close_window() {
    for (SDL_Texture* t : g_images)
        if (t) SDL_DestroyTexture(t);
    g_images.clear();
    g_sizes.clear();
#ifdef RELIC_HAVE_SDL_TTF
    for (TTF_Font* f : g_fonts)
        if (f) TTF_CloseFont(f);
    g_fonts.clear();
#endif
    g_keys.clear();
    g_events.clear();
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
                                    SDL_WINDOWPOS_CENTERED, w, h, SDL_WINDOW_RESIZABLE);
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
        TTF_Font* f = TTF_OpenFont(path.c_str(), size);
        if (!f) throw std::string("cannot load font ") + path + ": " + TTF_GetError();
        g_fonts.push_back(f);
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
        SDL_Rect dst{x, y, s->w, s->h};
        SDL_FreeSurface(s);
        if (tex) {
            SDL_RenderCopy(r, tex, nullptr, &dst);
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
        auto l = std::make_shared<List>();
        l->push_back(Value(static_cast<long long>(w)));
        l->push_back(Value(static_cast<long long>(h)));
        return Value(l);
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
    // kind: "key" (a = key name), "text" (a = text), "click" (a = x, b = y),
    // "wheel" (a = dx, b = dy), "resize" (a = width, b = height).
    add("host_gfx_simulate", 3, [](std::vector<Value>& a) {
        need_window();
        std::string kind = str(a[0], "kind");
        SDL_Event e{};
        if (kind == "key") {
            e.type = SDL_KEYDOWN;
            e.key.keysym.sym = SDL_GetKeyFromName(str(a[1], "key").c_str());
            if (e.key.keysym.sym == SDLK_UNKNOWN) throw std::string("unknown key name");
            SDL_PushEvent(&e);
        } else if (kind == "text") {
            std::string t = str(a[1], "text");
            if (t.size() >= sizeof(e.text.text)) throw std::string("text is too long");
            e.type = SDL_TEXTINPUT;
            std::copy(t.begin(), t.end(), e.text.text);
            SDL_PushEvent(&e);
        } else if (kind == "click") {
            for (Uint32 t : {SDL_MOUSEBUTTONDOWN, SDL_MOUSEBUTTONUP}) {
                SDL_Event b{};
                b.type = t;
                b.button.button = SDL_BUTTON_LEFT;
                b.button.clicks = 1;
                b.button.x = whole(a[1], "x");
                b.button.y = whole(a[2], "y");
                SDL_PushEvent(&b);
            }
        } else if (kind == "wheel") {
            e.type = SDL_MOUSEWHEEL;
            e.wheel.x = whole(a[1], "dx");
            e.wheel.y = whole(a[2], "dy");
            SDL_PushEvent(&e);
        } else if (kind == "resize") {
            e.type = SDL_WINDOWEVENT;
            e.window.event = SDL_WINDOWEVENT_SIZE_CHANGED;
            e.window.data1 = whole(a[1], "width");
            e.window.data2 = whole(a[2], "height");
            SDL_PushEvent(&e);
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
