#ifndef PALETTE_H
#define PALETTE_H

#include <stdint.h>

#define SCREEN_W 1024
#define SCREEN_H 768

typedef struct {
    uint8_t fb[SCREEN_W * SCREEN_H];
    uint32_t rgba[256];
} Framebuffer;

/* Initialize the 256-color web palette */
void palette_init(Framebuffer *f);

/* Return exact web-safe cube index for (r,g,b) in 0..5 */
static inline uint8_t web_rgb(int r, int g, int b) {
    return (uint8_t)(r * 36 + g * 6 + b);
}

/* Find nearest palette index for 8-bit RGB */
uint8_t palette_nearest(const Framebuffer *f, uint8_t r, uint8_t g, uint8_t b);

/* Semantic color constants initialized by palette_init */
extern uint8_t COL_BLACK;
extern uint8_t COL_WHITE;
extern uint8_t COL_GRAY_DARK;
extern uint8_t COL_GRAY_MID;
extern uint8_t COL_GRAY_LIGHT;
extern uint8_t COL_DEEP_SEA;
extern uint8_t COL_MID_SEA;
extern uint8_t COL_SHALLOW_SEA;
extern uint8_t COL_FOAM;
extern uint8_t COL_SAND_LIGHT;
extern uint8_t COL_SAND_DARK;
extern uint8_t COL_GRASS_LIGHT;
extern uint8_t COL_GRASS_MID;
extern uint8_t COL_GRASS_DARK;
extern uint8_t COL_JUNGLE_DARK;
extern uint8_t COL_ROCK_LIGHT;
extern uint8_t COL_ROCK_DARK;
extern uint8_t COL_RED;
extern uint8_t COL_RED_DARK;
extern uint8_t COL_ORANGE;
extern uint8_t COL_YELLOW;
extern uint8_t COL_CYAN;
extern uint8_t COL_BLUE;
extern uint8_t COL_BLUE_DARK;
extern uint8_t COL_GREEN_BRIGHT;
extern uint8_t COL_BROWN;
extern uint8_t COL_BROWN_DARK;

/* Drawing primitives */
void gfx_clear(Framebuffer *f, uint8_t col);
void gfx_pixel(Framebuffer *f, int x, int y, uint8_t col);
void gfx_rect(Framebuffer *f, int x, int y, int w, int h, uint8_t col);
void gfx_rect_outline(Framebuffer *f, int x, int y, int w, int h, uint8_t col);
void gfx_circle(Framebuffer *f, int cx, int cy, int r, uint8_t col);
void gfx_circle_outline(Framebuffer *f, int cx, int cy, int r, uint8_t col);
void gfx_line(Framebuffer *f, int x0, int y0, int x1, int y1, uint8_t col);
void gfx_triangle(Framebuffer *f, int x0, int y0, int x1, int y1, int x2, int y2, uint8_t col);

/* Crisp 5x7 bitmap text rendering */
void gfx_text(Framebuffer *f, int x, int y, int scale, uint8_t col, const char *fmt, ...);
int gfx_text_width(int scale, const char *text);

/* Convert indexed framebuffer to ARGB8888 texture buffer */
void gfx_present_argb8888(const Framebuffer *f, uint32_t *dst_pixels, int dst_pitch_bytes);

#endif /* PALETTE_H */
