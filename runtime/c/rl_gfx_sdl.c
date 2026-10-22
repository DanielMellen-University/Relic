/* rl_gfx_sdl.c: the graphics module over SDL2 for compiled programs.
 *
 * Built in place of rl_gfx_stub.c when the C runtime is configured with
 * -DRELIC_C_SDL=ON. It defines the same host_gfx_* table as the C++ host's
 * src/builtins/graphics.cpp, with the same names, arity, results and error
 * messages, so std/graphics works the same compiled or on the host. One
 * window at a time. SDL2_image (PNG and JPG) and SDL2_ttf (TrueType) are
 * used when RL_HAVE_SDL_IMAGE and RL_HAVE_SDL_TTF are defined. */
#include "relic.h"
#include "rl_internal.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#define SDL_MAIN_HANDLED
#include <SDL.h>
#ifdef RL_HAVE_SDL_IMAGE
#include <SDL_image.h>
#endif
#ifdef RL_HAVE_SDL_TTF
#include <SDL_ttf.h>
#endif

#include "font8x12.inc"

#define S(v) ((rl_str *)(v).u.o)
#define BI(name) static int name(rl_ctx *cx, rl_fun *self, int argc, const rl_value *a, rl_value *out)
#define UNUSED (void)self; (void)argc; (void)a

/* ---- state ---- */

static SDL_Window *g_window = NULL;
static SDL_Renderer *g_renderer = NULL;
static int g_quit = 0;

#define MAX_KEYS 64
#define MAX_EVENTS 512
static rl_value g_keys[MAX_KEYS];      /* key names pressed since the last next_key(), oldest first */
static int g_nkeys = 0;
static rl_value g_events[MAX_EVENTS];  /* event maps since the last next_event(), oldest first */
static int g_nevents = 0;

typedef struct { SDL_Texture *tex; int w, h; } image;
static image *g_images = NULL;         /* index = image id; freed on close */
static int g_nimages = 0, g_capimages = 0;

#ifdef RL_HAVE_SDL_TTF
static TTF_Font **g_fonts = NULL;      /* index = font id; freed on close */
static int g_nfonts = 0, g_capfonts = 0;
#endif

/* The clipboard when no window is open (and the last text set). */
static char *g_clip = NULL;
static size_t g_clip_len = 0;

/* ---- helpers ---- */

static int whole(rl_ctx *cx, rl_value v, const char *what, int *r) {
    long long n = 0;
    if (rl_arg_whole(cx, v, what, &n)) return RL_ERR;
    *r = (int)n;
    return RL_OK;
}

static int channel(rl_ctx *cx, rl_value v, const char *what, Uint8 *c) {
    int n = 0;
    if (whole(cx, v, what, &n)) return RL_ERR;
    if (n < 0 || n > 255) return rl_fail(cx, "%s must be 0 to 255", what);
    *c = (Uint8)n;
    return RL_OK;
}

/* Reads red, green and blue from a[i], a[i+1], a[i+2]. */
static int rgb(rl_ctx *cx, const rl_value *a, Uint8 *r, Uint8 *g, Uint8 *b) {
    if (channel(cx, a[0], "red", r)) return RL_ERR;
    if (channel(cx, a[1], "green", g)) return RL_ERR;
    return channel(cx, a[2], "blue", b);
}

static int rect_of(rl_ctx *cx, const rl_value *a, SDL_Rect *r) {
    if (whole(cx, a[0], "x", &r->x)) return RL_ERR;
    if (whole(cx, a[1], "y", &r->y)) return RL_ERR;
    if (whole(cx, a[2], "width", &r->w)) return RL_ERR;
    return whole(cx, a[3], "height", &r->h);
}

static int need_window(rl_ctx *cx) {
    if (!g_renderer) return rl_fail(cx, "no window is open; call open first");
    return RL_OK;
}

static rl_value lower_str(const char *s) {
    size_t n = strlen(s), i;
    char *t = (char *)malloc(n ? n : 1);
    rl_value v;
    if (!t) abort();
    for (i = 0; i < n; ++i) t[i] = (char)tolower((unsigned char)s[i]);
    v = rl_str_new(t, n);
    free(t);
    return v;
}

static void set_field(rl_map *m, const char *key, rl_value v) {
    rl_value k = rl_str_c(key);
    rl_map_set(m, S(k), v);
    rl_release(k);
    rl_release(v);
}

static rl_value event_new(const char *type) {
    rl_value m = rl_map_new();
    set_field((rl_map *)m.u.o, "type", rl_str_c(type));
    return m;
}

#define F(m, key, v) set_field((rl_map *)(m).u.o, key, v)

static void push_event(rl_value e) {
    if (g_nevents == MAX_EVENTS) {
        rl_release(g_events[0]);
        memmove(g_events, g_events + 1, sizeof(rl_value) * (MAX_EVENTS - 1));
        --g_nevents;
    }
    g_events[g_nevents++] = e;
}

static void push_key(rl_value k) {
    if (g_nkeys == MAX_KEYS) {
        rl_release(g_keys[0]);
        memmove(g_keys, g_keys + 1, sizeof(rl_value) * (MAX_KEYS - 1));
        --g_nkeys;
    }
    g_keys[g_nkeys++] = k;
}

/* Drains SDL's queue into our state. Every input call runs this first. */
static void pump(void) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        rl_value ev;
        if (e.type == SDL_QUIT) {
            g_quit = 1;
            push_event(event_new("quit"));
        } else if (e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) {
            rl_value k = lower_str(SDL_GetKeyName(e.key.keysym.sym));
            Uint16 mod = e.key.keysym.mod;
            if (e.type == SDL_KEYDOWN && !e.key.repeat) push_key(rl_retain(k));
            ev = event_new(e.type == SDL_KEYDOWN ? "key_down" : "key_up");
            F(ev, "key", k);
            F(ev, "repeat", rl_bool(e.key.repeat != 0));
            F(ev, "shift", rl_bool((mod & KMOD_SHIFT) != 0));
            F(ev, "ctrl", rl_bool((mod & KMOD_CTRL) != 0));
            F(ev, "alt", rl_bool((mod & KMOD_ALT) != 0));
            push_event(ev);
        } else if (e.type == SDL_TEXTINPUT) {
            ev = event_new("text");
            F(ev, "text", rl_str_c(e.text.text));
            push_event(ev);
        } else if (e.type == SDL_MOUSEBUTTONDOWN || e.type == SDL_MOUSEBUTTONUP) {
            ev = event_new(e.type == SDL_MOUSEBUTTONDOWN ? "mouse_down" : "mouse_up");
            F(ev, "button", rl_int(e.button.button));
            F(ev, "x", rl_int(e.button.x));
            F(ev, "y", rl_int(e.button.y));
            F(ev, "clicks", rl_int(e.button.clicks));
            push_event(ev);
        } else if (e.type == SDL_MOUSEMOTION) {
            ev = event_new("mouse_move");
            F(ev, "x", rl_int(e.motion.x));
            F(ev, "y", rl_int(e.motion.y));
            F(ev, "dx", rl_int(e.motion.xrel));
            F(ev, "dy", rl_int(e.motion.yrel));
            push_event(ev);
        } else if (e.type == SDL_MOUSEWHEEL) {
            int dx = e.wheel.x, dy = e.wheel.y;
            if (e.wheel.direction == SDL_MOUSEWHEEL_FLIPPED) { dx = -dx; dy = -dy; }
            ev = event_new("wheel");
            F(ev, "dx", rl_int(dx));
            F(ev, "dy", rl_int(dy));
            push_event(ev);
        } else if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
            ev = event_new("resize");
            F(ev, "width", rl_int(e.window.data1));
            F(ev, "height", rl_int(e.window.data2));
            push_event(ev);
        }
    }
}

static int image_id(rl_ctx *cx, rl_value v, int *id) {
    if (whole(cx, v, "image", id)) return RL_ERR;
    if (*id < 0 || *id >= g_nimages || !g_images[*id].tex) return rl_fail(cx, "not an open image");
    return RL_OK;
}

static int add_image(SDL_Texture *t, int w, int h) {
    if (g_nimages == g_capimages) {
        g_capimages = g_capimages ? g_capimages * 2 : 8;
        g_images = (image *)realloc(g_images, sizeof(image) * (size_t)g_capimages);
        if (!g_images) abort();
    }
    g_images[g_nimages].tex = t;
    g_images[g_nimages].w = w;
    g_images[g_nimages].h = h;
    return g_nimages++;
}

#ifdef RL_HAVE_SDL_TTF
static int font_of(rl_ctx *cx, rl_value v, TTF_Font **f) {
    int id = 0;
    if (whole(cx, v, "font", &id)) return RL_ERR;
    if (id < 0 || id >= g_nfonts || !g_fonts[id]) return rl_fail(cx, "not an open font");
    *f = g_fonts[id];
    return RL_OK;
}
#endif

static void close_window(void) {
    int i;
    for (i = 0; i < g_nimages; ++i)
        if (g_images[i].tex) SDL_DestroyTexture(g_images[i].tex);
    free(g_images);
    g_images = NULL;
    g_nimages = g_capimages = 0;
#ifdef RL_HAVE_SDL_TTF
    for (i = 0; i < g_nfonts; ++i)
        if (g_fonts[i]) TTF_CloseFont(g_fonts[i]);
    free(g_fonts);
    g_fonts = NULL;
    g_nfonts = g_capfonts = 0;
#endif
    for (i = 0; i < g_nkeys; ++i) rl_release(g_keys[i]);
    g_nkeys = 0;
    for (i = 0; i < g_nevents; ++i) rl_release(g_events[i]);
    g_nevents = 0;
    g_quit = 0;
    if (g_renderer) SDL_DestroyRenderer(g_renderer);
    if (g_window) SDL_DestroyWindow(g_window);
    g_renderer = NULL;
    g_window = NULL;
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

/* ---- window and drawing ---- */

BI(g_true) { (void)cx; UNUSED; *out = rl_bool(1); return RL_OK; }

BI(g_open) {
    int w = 0, h = 0;
    UNUSED;
    *out = rl_none();
    if (whole(cx, a[0], "width", &w) || whole(cx, a[1], "height", &h)) return RL_ERR;
    if (a[2].k != RL_STR) return rl_fail(cx, "title must be a string, got %s", rl_type_name(a[2]));
    if (w <= 0 || h <= 0) return rl_fail(cx, "width and height must be above 0");
    if (g_window) close_window();
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) return rl_fail(cx, "cannot start video: %s", SDL_GetError());
    g_window = SDL_CreateWindow(S(a[2])->data, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, w, h,
                                SDL_WINDOW_RESIZABLE);
    if (!g_window) {
        int r = rl_fail(cx, "cannot open a window: %s", SDL_GetError());
        close_window();
        return r;
    }
    g_renderer = SDL_CreateRenderer(g_window, -1, 0);
    if (!g_renderer) g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_SOFTWARE);
    if (!g_renderer) {
        int r = rl_fail(cx, "cannot draw in the window: %s", SDL_GetError());
        close_window();
        return r;
    }
    SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_BLEND);
    SDL_StartTextInput();
    return RL_OK;
}

BI(g_close) { (void)cx; UNUSED; *out = rl_none(); close_window(); return RL_OK; }

BI(g_clear) {
    Uint8 r = 0, g = 0, b = 0;
    UNUSED;
    *out = rl_none();
    if (need_window(cx) || rgb(cx, a, &r, &g, &b)) return RL_ERR;
    SDL_SetRenderDrawColor(g_renderer, r, g, b, 255);
    SDL_RenderClear(g_renderer);
    return RL_OK;
}

BI(g_rect) {
    SDL_Rect rect;
    Uint8 r = 0, g = 0, b = 0;
    UNUSED;
    *out = rl_none();
    if (need_window(cx) || rect_of(cx, a, &rect) || rgb(cx, a + 4, &r, &g, &b)) return RL_ERR;
    SDL_SetRenderDrawColor(g_renderer, r, g, b, 255);
    SDL_RenderFillRect(g_renderer, &rect);
    return RL_OK;
}

BI(g_outline) {
    SDL_Rect rect;
    Uint8 r = 0, g = 0, b = 0;
    UNUSED;
    *out = rl_none();
    if (need_window(cx) || rect_of(cx, a, &rect) || rgb(cx, a + 4, &r, &g, &b)) return RL_ERR;
    SDL_SetRenderDrawColor(g_renderer, r, g, b, 255);
    SDL_RenderDrawRect(g_renderer, &rect);
    return RL_OK;
}

/* Rect with alpha 0 (clear) to 255 (solid), blended over what is there. */
BI(g_rect_alpha) {
    SDL_Rect rect;
    Uint8 r = 0, g = 0, b = 0, al = 0;
    UNUSED;
    *out = rl_none();
    if (need_window(cx) || rect_of(cx, a, &rect) || rgb(cx, a + 4, &r, &g, &b) ||
        channel(cx, a[7], "alpha", &al))
        return RL_ERR;
    SDL_SetRenderDrawColor(g_renderer, r, g, b, al);
    SDL_RenderFillRect(g_renderer, &rect);
    return RL_OK;
}

BI(g_pixel) {
    int x = 0, y = 0;
    Uint8 r = 0, g = 0, b = 0;
    UNUSED;
    *out = rl_none();
    if (need_window(cx) || rgb(cx, a + 2, &r, &g, &b)) return RL_ERR;
    if (whole(cx, a[0], "x", &x) || whole(cx, a[1], "y", &y)) return RL_ERR;
    SDL_SetRenderDrawColor(g_renderer, r, g, b, 255);
    SDL_RenderDrawPoint(g_renderer, x, y);
    return RL_OK;
}

BI(g_line) {
    int x1 = 0, y1 = 0, x2 = 0, y2 = 0;
    Uint8 r = 0, g = 0, b = 0;
    UNUSED;
    *out = rl_none();
    if (need_window(cx) || rgb(cx, a + 4, &r, &g, &b)) return RL_ERR;
    if (whole(cx, a[0], "x1", &x1) || whole(cx, a[1], "y1", &y1) || whole(cx, a[2], "x2", &x2) ||
        whole(cx, a[3], "y2", &y2))
        return RL_ERR;
    SDL_SetRenderDrawColor(g_renderer, r, g, b, 255);
    SDL_RenderDrawLine(g_renderer, x1, y1, x2, y2);
    return RL_OK;
}

/* Limits drawing to a rectangle; width or height 0 or less turns it off. */
BI(g_clip_rect) {
    SDL_Rect rect;
    UNUSED;
    *out = rl_none();
    if (need_window(cx) || rect_of(cx, a, &rect)) return RL_ERR;
    SDL_RenderSetClipRect(g_renderer, rect.w > 0 && rect.h > 0 ? &rect : NULL);
    return RL_OK;
}

BI(g_present) {
    UNUSED;
    *out = rl_none();
    if (need_window(cx)) return RL_ERR;
    SDL_RenderPresent(g_renderer);
    return RL_OK;
}

BI(g_size) {
    int w = 0, h = 0;
    UNUSED;
    *out = rl_none();
    if (need_window(cx)) return RL_ERR;
    SDL_GetRendererOutputSize(g_renderer, &w, &h);
    *out = rl_list_new(2);
    rl_list_push((rl_list *)out->u.o, rl_int(w));
    rl_list_push((rl_list *)out->u.o, rl_int(h));
    return RL_OK;
}

BI(g_wait) {
    int ms = 0;
    UNUSED;
    *out = rl_none();
    if (whole(cx, a[0], "milliseconds", &ms)) return RL_ERR;
    if (ms > 0) SDL_Delay((Uint32)ms);
    return RL_OK;
}

/* Built-in 8x12 font, ASCII only; other bytes draw as '?'. */
BI(g_text) {
    int x = 0, y = 0, sc = 0, cxp, row, col;
    Uint8 r = 0, g = 0, b = 0;
    size_t i;
    rl_str *t;
    UNUSED;
    *out = rl_none();
    if (need_window(cx)) return RL_ERR;
    if (whole(cx, a[0], "x", &x) || whole(cx, a[1], "y", &y)) return RL_ERR;
    if (rl_arg_str(cx, a[2], "text")) return RL_ERR;
    if (whole(cx, a[3], "scale", &sc)) return RL_ERR;
    if (sc < 1) return rl_fail(cx, "scale must be 1 or more");
    if (rgb(cx, a + 4, &r, &g, &b)) return RL_ERR;
    SDL_SetRenderDrawColor(g_renderer, r, g, b, 255);
    t = S(a[2]);
    cxp = x;
    for (i = 0; i < t->len; ++i) {
        unsigned char c = (unsigned char)t->data[i];
        const unsigned char *glyph;
        if (c == '\n') { cxp = x; y += 12 * sc; continue; }
        if (c < 32 || c > 126) c = '?';
        glyph = kFont8x12[c - 32];
        for (row = 0; row < 12; row++)
            for (col = 0; col < 8; col++)
                if (glyph[row] & (0x80 >> col)) {
                    SDL_Rect px;
                    px.x = cxp + col * sc;
                    px.y = y + row * sc;
                    px.w = sc;
                    px.h = sc;
                    SDL_RenderFillRect(g_renderer, &px);
                }
        cxp += 8 * sc;
    }
    return RL_OK;
}

/* ---- input ---- */

BI(g_quit_requested) {
    UNUSED;
    *out = rl_none();
    if (need_window(cx)) return RL_ERR;
    pump();
    *out = rl_bool(g_quit);
    return RL_OK;
}

BI(g_key_down) {
    SDL_Keycode k;
    const Uint8 *st;
    UNUSED;
    *out = rl_none();
    if (need_window(cx)) return RL_ERR;
    pump();
    if (rl_arg_str(cx, a[0], "key")) return RL_ERR;
    k = SDL_GetKeyFromName(S(a[0])->data);
    if (k == SDLK_UNKNOWN) return rl_fail(cx, "unknown key name: %s", S(a[0])->data);
    st = SDL_GetKeyboardState(NULL);
    *out = rl_bool(st[SDL_GetScancodeFromKey(k)] != 0);
    return RL_OK;
}

BI(g_next_key) {
    UNUSED;
    *out = rl_none();
    if (need_window(cx)) return RL_ERR;
    pump();
    if (g_nkeys == 0) return RL_OK;
    *out = g_keys[0];
    memmove(g_keys, g_keys + 1, sizeof(rl_value) * (size_t)(g_nkeys - 1));
    --g_nkeys;
    return RL_OK;
}

BI(g_next_event) {
    UNUSED;
    *out = rl_none();
    if (need_window(cx)) return RL_ERR;
    pump();
    if (g_nevents == 0) return RL_OK;
    *out = g_events[0];
    memmove(g_events, g_events + 1, sizeof(rl_value) * (size_t)(g_nevents - 1));
    --g_nevents;
    return RL_OK;
}

BI(g_mouse_x) {
    int x = 0;
    UNUSED;
    *out = rl_none();
    if (need_window(cx)) return RL_ERR;
    pump();
    SDL_GetMouseState(&x, NULL);
    *out = rl_int(x);
    return RL_OK;
}

BI(g_mouse_y) {
    int y = 0;
    UNUSED;
    *out = rl_none();
    if (need_window(cx)) return RL_ERR;
    pump();
    SDL_GetMouseState(NULL, &y);
    *out = rl_int(y);
    return RL_OK;
}

BI(g_mouse_down) {
    int b = 0;
    UNUSED;
    *out = rl_none();
    if (need_window(cx)) return RL_ERR;
    pump();
    if (whole(cx, a[0], "button", &b)) return RL_ERR;
    if (b < 1 || b > 3) return rl_fail(cx, "button must be 1 (left), 2 (middle) or 3 (right)");
    *out = rl_bool((SDL_GetMouseState(NULL, NULL) & SDL_BUTTON(b)) != 0);
    return RL_OK;
}

/* "shift", "ctrl", "alt" joined with +, or none. */
static int has_word(const char *s, const char *word) {
    size_t n = strlen(s), w = strlen(word);
    const char *p = strstr(s, word);   /* like the host, only the first match counts */
    size_t at, end;
    if (!p) return 0;
    at = (size_t)(p - s);
    end = at + w;
    return (at == 0 || s[at - 1] == '+' || s[at - 1] == ',' || s[at - 1] == ' ') &&
           (end >= n || s[end] == '+' || s[end] == ',' || s[end] == ' ');
}

static int parse_mods(rl_ctx *cx, rl_value v, Uint16 *mod) {
    char *s;
    size_t i, n;
    *mod = 0;
    if (v.k == RL_NONE) return RL_OK;
    if (rl_arg_str(cx, v, "mods")) return RL_ERR;
    n = S(v)->len;
    s = (char *)malloc(n + 1);
    if (!s) abort();
    for (i = 0; i < n; ++i) s[i] = (char)tolower((unsigned char)S(v)->data[i]);
    s[n] = 0;
    if (has_word(s, "shift")) *mod |= KMOD_SHIFT;
    if (has_word(s, "ctrl")) *mod |= KMOD_CTRL;
    if (has_word(s, "alt")) *mod |= KMOD_ALT;
    free(s);
    if (*mod == 0 && n > 0) return rl_fail(cx, "unknown modifier: %s", S(v)->data);
    return RL_OK;
}

static int button_event(rl_ctx *cx, Uint32 type, rl_value x, rl_value y) {
    SDL_Event e;
    int bx = 0, by = 0;
    if (whole(cx, x, "x", &bx) || whole(cx, y, "y", &by)) return RL_ERR;
    memset(&e, 0, sizeof e);
    e.type = type;
    e.button.button = SDL_BUTTON_LEFT;
    e.button.clicks = 1;
    e.button.x = bx;
    e.button.y = by;
    SDL_PushEvent(&e);
    return RL_OK;
}

/* Puts a made-up event in SDL's queue, for tests and scripted demos.
 * The kinds are listed in std/graphics.relic. */
BI(g_simulate) {
    SDL_Event e;
    const char *kind;
    UNUSED;
    *out = rl_none();
    if (need_window(cx)) return RL_ERR;
    if (rl_arg_str(cx, a[0], "kind")) return RL_ERR;
    kind = S(a[0])->data;
    memset(&e, 0, sizeof e);
    if (!strcmp(kind, "key") || !strcmp(kind, "keyup")) {
        Uint16 mod = 0;
        e.type = !strcmp(kind, "key") ? SDL_KEYDOWN : SDL_KEYUP;
        if (rl_arg_str(cx, a[1], "key")) return RL_ERR;
        e.key.keysym.sym = SDL_GetKeyFromName(S(a[1])->data);
        if (e.key.keysym.sym == SDLK_UNKNOWN) return rl_fail(cx, "unknown key name");
        if (parse_mods(cx, a[2], &mod)) return RL_ERR;
        e.key.keysym.mod = mod;
        SDL_PushEvent(&e);
    } else if (!strcmp(kind, "text")) {
        if (rl_arg_str(cx, a[1], "text")) return RL_ERR;
        if (S(a[1])->len >= sizeof(e.text.text)) return rl_fail(cx, "text is too long");
        e.type = SDL_TEXTINPUT;
        memcpy(e.text.text, S(a[1])->data, S(a[1])->len);
        SDL_PushEvent(&e);
    } else if (!strcmp(kind, "click")) {
        if (button_event(cx, SDL_MOUSEBUTTONDOWN, a[1], a[2])) return RL_ERR;
        if (button_event(cx, SDL_MOUSEBUTTONUP, a[1], a[2])) return RL_ERR;
    } else if (!strcmp(kind, "mousedown") || !strcmp(kind, "mouseup")) {
        if (button_event(cx, !strcmp(kind, "mousedown") ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP, a[1], a[2]))
            return RL_ERR;
    } else if (!strcmp(kind, "move")) {
        int x = 0, y = 0;
        if (whole(cx, a[1], "x", &x) || whole(cx, a[2], "y", &y)) return RL_ERR;
        e.type = SDL_MOUSEMOTION;
        e.motion.x = x;
        e.motion.y = y;
        SDL_PushEvent(&e);
    } else if (!strcmp(kind, "wheel")) {
        int dx = 0, dy = 0;
        if (whole(cx, a[1], "dx", &dx) || whole(cx, a[2], "dy", &dy)) return RL_ERR;
        e.type = SDL_MOUSEWHEEL;
        e.wheel.x = dx;
        e.wheel.y = dy;
        SDL_PushEvent(&e);
    } else if (!strcmp(kind, "resize")) {
        int w = 0, h = 0;
        if (whole(cx, a[1], "width", &w) || whole(cx, a[2], "height", &h)) return RL_ERR;
        e.type = SDL_WINDOWEVENT;
        e.window.event = SDL_WINDOWEVENT_SIZE_CHANGED;
        e.window.data1 = w;
        e.window.data2 = h;
        SDL_PushEvent(&e);
    } else if (!strcmp(kind, "quit")) {
        e.type = SDL_QUIT;
        SDL_PushEvent(&e);
    } else {
        return rl_fail(cx, "unknown event kind: %s", kind);
    }
    return RL_OK;
}

/* ---- images and canvases ---- */

#ifndef RL_HAVE_SDL_IMAGE
static int ends_with_ci(const char *s, size_t n, const char *ext) {
    size_t e = strlen(ext), i;
    if (n < e) return 0;
    for (i = 0; i < e; ++i)
        if (tolower((unsigned char)s[n - e + i]) != ext[i]) return 0;
    return 1;
}
#endif

BI(g_image_load) {
    SDL_Surface *surf;
    SDL_Texture *t;
    const char *path;
    int w, h;
    UNUSED;
    *out = rl_none();
    if (need_window(cx) || rl_arg_str(cx, a[0], "path")) return RL_ERR;
    path = S(a[0])->data;
#ifdef RL_HAVE_SDL_IMAGE
    surf = IMG_Load(path);
#else
    if (!ends_with_ci(path, S(a[0])->len, ".bmp"))
        return rl_fail(cx, "cannot load image %s: only BMP is built in; rebuild with SDL2_image installed for PNG", path);
    surf = SDL_LoadBMP(path);
#endif
    if (!surf) return rl_fail(cx, "cannot load image %s: %s", path, SDL_GetError());
    t = SDL_CreateTextureFromSurface(g_renderer, surf);
    w = surf->w;
    h = surf->h;
    SDL_FreeSurface(surf);
    if (!t) return rl_fail(cx, "cannot use image %s: %s", path, SDL_GetError());
    *out = rl_int(add_image(t, w, h));
    return RL_OK;
}

BI(g_image) {
    int id = 0;
    SDL_Rect dst;
    UNUSED;
    *out = rl_none();
    if (need_window(cx) || image_id(cx, a[0], &id)) return RL_ERR;
    if (whole(cx, a[1], "x", &dst.x) || whole(cx, a[2], "y", &dst.y)) return RL_ERR;
    dst.w = g_images[id].w;
    dst.h = g_images[id].h;
    SDL_RenderCopy(g_renderer, g_images[id].tex, NULL, &dst);
    return RL_OK;
}

BI(g_image_width) {
    int id = 0;
    UNUSED;
    *out = rl_none();
    if (need_window(cx) || image_id(cx, a[0], &id)) return RL_ERR;
    *out = rl_int(g_images[id].w);
    return RL_OK;
}

BI(g_image_height) {
    int id = 0;
    UNUSED;
    *out = rl_none();
    if (need_window(cx) || image_id(cx, a[0], &id)) return RL_ERR;
    *out = rl_int(g_images[id].h);
    return RL_OK;
}

/* Canvas: an image you can draw into; draw_to(id) sends drawing there,
 * draw_to(none) back to the window. */
BI(g_canvas) {
    int w = 0, h = 0;
    SDL_Texture *t;
    UNUSED;
    *out = rl_none();
    if (need_window(cx)) return RL_ERR;
    if (whole(cx, a[0], "width", &w) || whole(cx, a[1], "height", &h)) return RL_ERR;
    if (w <= 0 || h <= 0) return rl_fail(cx, "width and height must be above 0");
    t = SDL_CreateTexture(g_renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, w, h);
    if (!t) return rl_fail(cx, "cannot make a canvas: %s", SDL_GetError());
    SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
    *out = rl_int(add_image(t, w, h));
    return RL_OK;
}

BI(g_draw_to) {
    SDL_Texture *t = NULL;
    UNUSED;
    *out = rl_none();
    if (need_window(cx)) return RL_ERR;
    if (a[0].k != RL_NONE) {
        int id = 0;
        if (image_id(cx, a[0], &id)) return RL_ERR;
        t = g_images[id].tex;
    }
    if (SDL_SetRenderTarget(g_renderer, t) != 0) return rl_fail(cx, "cannot draw into that image: %s", SDL_GetError());
    return RL_OK;
}

/* ---- fonts ---- */

#ifdef RL_HAVE_SDL_TTF
BI(g_font_load) {
    int size = 0;
    const char *path;
    TTF_Font *f;
    UNUSED;
    *out = rl_none();
    if (need_window(cx) || rl_arg_str(cx, a[0], "path")) return RL_ERR;
    path = S(a[0])->data;
    if (whole(cx, a[1], "size", &size)) return RL_ERR;
    if (size < 1) return rl_fail(cx, "size must be 1 or more");
    if (!TTF_WasInit() && TTF_Init() != 0) return rl_fail(cx, "cannot start fonts: %s", TTF_GetError());
    f = TTF_OpenFont(path, size);
    if (!f) return rl_fail(cx, "cannot load font %s: %s", path, TTF_GetError());
    if (g_nfonts == g_capfonts) {
        g_capfonts = g_capfonts ? g_capfonts * 2 : 4;
        g_fonts = (TTF_Font **)realloc(g_fonts, sizeof(TTF_Font *) * (size_t)g_capfonts);
        if (!g_fonts) abort();
    }
    g_fonts[g_nfonts] = f;
    *out = rl_int(g_nfonts++);
    return RL_OK;
}

BI(g_font_text) {
    TTF_Font *f = NULL;
    int x = 0, y = 0;
    SDL_Color c = {0, 0, 0, 255};
    SDL_Surface *s;
    SDL_Texture *tex;
    SDL_Rect dst;
    UNUSED;
    *out = rl_none();
    if (need_window(cx) || font_of(cx, a[0], &f)) return RL_ERR;
    if (whole(cx, a[1], "x", &x) || whole(cx, a[2], "y", &y)) return RL_ERR;
    if (rl_arg_str(cx, a[3], "text")) return RL_ERR;
    if (rgb(cx, a + 4, &c.r, &c.g, &c.b)) return RL_ERR;
    c.a = 255;
    if (S(a[3])->len == 0) return RL_OK;
    s = TTF_RenderUTF8_Blended(f, S(a[3])->data, c);
    if (!s) return rl_fail(cx, "cannot draw text: %s", TTF_GetError());
    tex = SDL_CreateTextureFromSurface(g_renderer, s);
    dst.x = x;
    dst.y = y;
    dst.w = s->w;
    dst.h = s->h;
    SDL_FreeSurface(s);
    if (tex) {
        SDL_RenderCopy(g_renderer, tex, NULL, &dst);
        SDL_DestroyTexture(tex);
    }
    return RL_OK;
}

BI(g_font_size) {
    TTF_Font *f = NULL;
    int w = 0, h;
    UNUSED;
    *out = rl_none();
    if (need_window(cx) || font_of(cx, a[0], &f)) return RL_ERR;
    if (rl_arg_str(cx, a[1], "text")) return RL_ERR;
    h = TTF_FontHeight(f);
    if (S(a[1])->len && TTF_SizeUTF8(f, S(a[1])->data, &w, &h) != 0)
        return rl_fail(cx, "cannot measure text: %s", TTF_GetError());
    *out = rl_list_new(2);
    rl_list_push((rl_list *)out->u.o, rl_int(w));
    rl_list_push((rl_list *)out->u.o, rl_int(h));
    return RL_OK;
}
#else
BI(g_no_fonts) {
    UNUSED;
    *out = rl_none();
    return rl_fail(cx, "TrueType fonts are not built in; rebuild with SDL2_ttf installed");
}
#define g_font_load g_no_fonts
#define g_font_text g_no_fonts
#define g_font_size g_no_fonts
#endif

#ifdef RL_HAVE_SDL_IMAGE
#define HAS_PNG 1
#else
#define HAS_PNG 0
#endif
#ifdef RL_HAVE_SDL_TTF
#define HAS_FONTS 1
#else
#define HAS_FONTS 0
#endif
BI(g_has_png) { (void)cx; UNUSED; *out = rl_bool(HAS_PNG); return RL_OK; }
BI(g_has_fonts) { (void)cx; UNUSED; *out = rl_bool(HAS_FONTS); return RL_OK; }

/* ---- clipboard: the system one while a window is open, else kept here ---- */

BI(g_clip_set) {
    rl_str *s;
    UNUSED;
    *out = rl_none();
    if (a[0].k != RL_STR) return rl_fail(cx, "text must be a string, got %s", rl_type_name(a[0]));
    s = S(a[0]);
    free(g_clip);
    g_clip = (char *)malloc(s->len + 1);
    if (!g_clip) abort();
    memcpy(g_clip, s->data, s->len);
    g_clip[s->len] = 0;
    g_clip_len = s->len;
    if (g_window) SDL_SetClipboardText(g_clip);
    return RL_OK;
}

BI(g_clip_get) {
    (void)cx; UNUSED;
    if (g_window && SDL_HasClipboardText()) {
        char *t = SDL_GetClipboardText();
        *out = rl_str_c(t ? t : "");
        SDL_free(t);
        return RL_OK;
    }
    *out = rl_str_new(g_clip ? g_clip : "", g_clip_len);
    return RL_OK;
}

static const rl_builtin_def g_gfx[] = {
    {"host_gfx_available", 0, g_true}, {"host_gfx_has_png", 0, g_has_png}, {"host_gfx_has_fonts", 0, g_has_fonts},
    {"host_gfx_open", 3, g_open}, {"host_gfx_close", 0, g_close}, {"host_gfx_clear", 3, g_clear},
    {"host_gfx_rect", 7, g_rect}, {"host_gfx_pixel", 5, g_pixel}, {"host_gfx_present", 0, g_present},
    {"host_gfx_quit_requested", 0, g_quit_requested}, {"host_gfx_wait", 1, g_wait},
    {"host_gfx_key_down", 1, g_key_down}, {"host_gfx_next_key", 0, g_next_key},
    {"host_gfx_mouse_x", 0, g_mouse_x}, {"host_gfx_mouse_y", 0, g_mouse_y},
    {"host_gfx_mouse_down", 1, g_mouse_down}, {"host_gfx_text", 7, g_text},
    {"host_gfx_image_load", 1, g_image_load}, {"host_gfx_image", 3, g_image},
    {"host_gfx_image_width", 1, g_image_width}, {"host_gfx_image_height", 1, g_image_height},
    {"host_gfx_font_load", 2, g_font_load}, {"host_gfx_font_text", 7, g_font_text},
    {"host_gfx_font_size", 2, g_font_size}, {"host_gfx_next_event", 0, g_next_event},
    {"host_gfx_simulate", 3, g_simulate}, {"host_gfx_size", 0, g_size}, {"host_gfx_line", 7, g_line},
    {"host_gfx_outline", 7, g_outline}, {"host_gfx_rect_alpha", 8, g_rect_alpha},
    {"host_gfx_clip", 4, g_clip_rect}, {"host_gfx_canvas", 2, g_canvas}, {"host_gfx_draw_to", 1, g_draw_to},
    {"host_clipboard_set", 1, g_clip_set}, {"host_clipboard_get", 0, g_clip_get},
    {NULL, 0, NULL}};

const rl_builtin_def *rl_gfx_defs(void) { return g_gfx; }

/* At exit: close a window the program left open, so its queued events and
 * key names are freed before the leak check. */
void rl_gfx_release(void) {
    if (g_window || g_renderer) close_window();
    free(g_clip);
    g_clip = NULL;
    g_clip_len = 0;
}
