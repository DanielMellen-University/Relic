/* rl_gfx_stub.c: graphics in a build without the optional SDL2 module.
 *
 * Graphics is an optional C module over SDL2 (decided Oct 20 2026) and is
 * not part of C1. This file gives the same answers as the C++ host built
 * without -DRELIC_SDL=ON: host_gfx_available, host_gfx_has_png and
 * host_gfx_has_fonts give false, every other host_gfx_* fails with the
 * same message, and the clipboard is kept in this process. The SDL2
 * module is rl_gfx_sdl.c (-DRELIC_C_SDL=ON); see docs/C-RUNTIME.md,
 * "Graphics". */
#include "relic.h"
#include "rl_internal.h"

#include <stdlib.h>
#include <string.h>

#define BI(name) static int name(rl_ctx *cx, rl_fun *self, int argc, const rl_value *a, rl_value *out)

BI(g_false) { (void)cx; (void)self; (void)argc; (void)a; *out = rl_bool(0); return RL_OK; }

BI(g_missing) {
    (void)self; (void)argc; (void)a;
    *out = rl_none();
    return rl_fail(cx, "graphics are not built in; rebuild with cmake -DRELIC_SDL=ON (needs SDL2)");
}

static char *g_clip = NULL;
static size_t g_clip_len = 0;

BI(g_clip_set) {
    rl_str *s;
    (void)self; (void)argc;
    *out = rl_none();
    if (a[0].k != RL_STR) return rl_fail(cx, "text must be a string, got %s", rl_type_name(a[0]));
    s = (rl_str *)a[0].u.o;
    free(g_clip);
    g_clip = (char *)malloc(s->len ? s->len : 1);
    if (!g_clip) abort();
    memcpy(g_clip, s->data, s->len);
    g_clip_len = s->len;
    return RL_OK;
}

BI(g_clip_get) {
    (void)cx; (void)self; (void)argc; (void)a;
    *out = rl_str_new(g_clip ? g_clip : "", g_clip_len);
    return RL_OK;
}

static const rl_builtin_def g_gfx[] = {
    {"host_gfx_available", 0, g_false}, {"host_gfx_has_png", 0, g_false}, {"host_gfx_has_fonts", 0, g_false},
    {"host_gfx_open", 3, g_missing}, {"host_gfx_close", 0, g_missing}, {"host_gfx_clear", 3, g_missing},
    {"host_gfx_rect", 7, g_missing}, {"host_gfx_pixel", 5, g_missing}, {"host_gfx_present", 0, g_missing},
    {"host_gfx_quit_requested", 0, g_missing}, {"host_gfx_wait", 1, g_missing},
    {"host_gfx_key_down", 1, g_missing}, {"host_gfx_next_key", 0, g_missing},
    {"host_gfx_mouse_x", 0, g_missing}, {"host_gfx_mouse_y", 0, g_missing},
    {"host_gfx_mouse_down", 1, g_missing}, {"host_gfx_text", 7, g_missing},
    {"host_gfx_image_load", 1, g_missing}, {"host_gfx_image", 3, g_missing},
    {"host_gfx_image_width", 1, g_missing}, {"host_gfx_image_height", 1, g_missing},
    {"host_gfx_font_load", 2, g_missing}, {"host_gfx_font_text", 7, g_missing},
    {"host_gfx_font_size", 2, g_missing}, {"host_gfx_next_event", 0, g_missing},
    {"host_gfx_simulate", 3, g_missing}, {"host_gfx_size", 0, g_missing}, {"host_gfx_line", 7, g_missing},
    {"host_gfx_outline", 7, g_missing}, {"host_gfx_rect_alpha", 8, g_missing},
    {"host_gfx_clip", 4, g_missing}, {"host_gfx_canvas", 2, g_missing}, {"host_gfx_draw_to", 1, g_missing},
    {"host_clipboard_set", 1, g_clip_set}, {"host_clipboard_get", 0, g_clip_get},
    {NULL, 0, NULL}};

const rl_builtin_def *rl_gfx_defs(void) { return g_gfx; }

void rl_gfx_release(void) {
    free(g_clip);
    g_clip = NULL;
    g_clip_len = 0;
}
