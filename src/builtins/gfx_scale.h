/* gfx_scale.h: high-DPI math shared by the host (src/builtins/graphics.cpp)
 * and the C module (runtime/c/rl_gfx_sdl.c), so both round the same way.
 * Drawing is in window units; the renderer is scaled by gfx_scale_of so a
 * unit covers that many pixels. Plain C99, also compiled as C++. */
#ifndef RELIC_GFX_SCALE_H
#define RELIC_GFX_SCALE_H

#include <math.h>

/* Pixels per window unit: drawable width over window width; 1 when either
 * is unknown. */
static inline double gfx_scale_of(int pixels, int units) {
    return pixels > 0 && units > 0 ? (double)pixels / (double)units : 1.0;
}

/* The pixel size to open a font of `size` window units at, so its glyphs
 * are drawn one texel per pixel. At least 1. */
static inline int gfx_font_pixels(int size, double scale) {
    int p = (int)floor((double)size * scale + 0.5);
    return p < 1 ? 1 : p;
}

/* Window units that cover `pixels` pixels, rounded up (text measurement). */
static inline int gfx_units_up(int pixels, double scale) {
    return (int)ceil((double)pixels / scale - 1e-9);
}

#endif
