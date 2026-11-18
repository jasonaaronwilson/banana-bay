#include "palette.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

uint8_t COL_BLACK;
uint8_t COL_WHITE;
uint8_t COL_GRAY_DARK;
uint8_t COL_GRAY_MID;
uint8_t COL_GRAY_LIGHT;
uint8_t COL_DEEP_SEA;
uint8_t COL_MID_SEA;
uint8_t COL_SHALLOW_SEA;
uint8_t COL_FOAM;
uint8_t COL_SAND_LIGHT;
uint8_t COL_SAND_DARK;
uint8_t COL_GRASS_LIGHT;
uint8_t COL_GRASS_MID;
uint8_t COL_GRASS_DARK;
uint8_t COL_JUNGLE_DARK;
uint8_t COL_ROCK_LIGHT;
uint8_t COL_ROCK_DARK;
uint8_t COL_RED;
uint8_t COL_RED_DARK;
uint8_t COL_ORANGE;
uint8_t COL_YELLOW;
uint8_t COL_CYAN;
uint8_t COL_BLUE;
uint8_t COL_BLUE_DARK;
uint8_t COL_GREEN_BRIGHT;
uint8_t COL_BROWN;
uint8_t COL_BROWN_DARK;

static uint32_t pack_argb(uint8_t r, uint8_t g, uint8_t b) {
    return 0xFF000000u | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

void palette_init(Framebuffer *f) {
    /* 0..215: Standard 6x6x6 Web-Safe Palette */
    for (int r = 0; r < 6; r++) {
        for (int g = 0; g < 6; g++) {
            for (int b = 0; b < 6; b++) {
                int idx = r * 36 + g * 6 + b;
                f->rgba[idx] = pack_argb((uint8_t)(r * 51), (uint8_t)(g * 51), (uint8_t)(b * 51));
            }
        }
    }

    /* 216..239: 24-step grayscale ramp (standard 256-color web/xterm extension) */
    for (int i = 0; i < 24; i++) {
        uint8_t v = (uint8_t)(8 + i * 10);
        f->rgba[216 + i] = pack_argb(v, v, v);
    }

    /* 240..255: 16 curated tropical/tactical web-palette accent tones */
    static const uint8_t extras[16][3] = {
        {0x00, 0x22, 0x44}, /* 240: deep abyss */
        {0x00, 0x44, 0x77}, /* 241: ocean mid */
        {0x11, 0x66, 0x88}, /* 242: coastal */
        {0x22, 0x88, 0x99}, /* 243: shallow lagoon */
        {0xEE, 0xDD, 0x99}, /* 244: warm beach sand */
        {0xCC, 0xBB, 0x77}, /* 245: wet sand */
        {0x44, 0x99, 0x33}, /* 246: lush grass */
        {0x22, 0x77, 0x22}, /* 247: tropical foliage */
        {0x11, 0x55, 0x11}, /* 248: deep jungle */
        {0x77, 0x66, 0x55}, /* 249: highland rock */
        {0x55, 0x44, 0x33}, /* 250: dark cliff */
        {0x55, 0x66, 0x44}, /* 251: military olive */
        {0x88, 0x33, 0x11}, /* 252: rust / brick */
        {0xFF, 0x77, 0x00}, /* 253: bright flame */
        {0xFF, 0xEE, 0x55}, /* 254: tracer gold */
        {0x88, 0xCC, 0xFF}, /* 255: sky glint */
    };
    for (int i = 0; i < 16; i++) {
        f->rgba[240 + i] = pack_argb(extras[i][0], extras[i][1], extras[i][2]);
    }

    COL_BLACK        = web_rgb(0, 0, 0);
    COL_WHITE        = web_rgb(5, 5, 5);
    COL_GRAY_DARK    = 220;
    COL_GRAY_MID     = 228;
    COL_GRAY_LIGHT   = 235;
    COL_DEEP_SEA     = 240;
    COL_MID_SEA      = 241;
    COL_SHALLOW_SEA  = 243;
    COL_FOAM         = web_rgb(4, 5, 5);
    COL_SAND_LIGHT   = 244;
    COL_SAND_DARK    = 245;
    COL_GRASS_LIGHT  = 246;
    COL_GRASS_MID    = 247;
    COL_GRASS_DARK   = 248;
    COL_JUNGLE_DARK  = web_rgb(0, 1, 0);
    COL_ROCK_LIGHT   = 249;
    COL_ROCK_DARK    = 250;
    COL_RED          = web_rgb(5, 0, 0);
    COL_RED_DARK     = web_rgb(3, 0, 0);
    COL_ORANGE       = 253;
    COL_YELLOW       = web_rgb(5, 5, 0);
    COL_CYAN         = web_rgb(0, 5, 5);
    COL_BLUE         = web_rgb(0, 2, 5);
    COL_BLUE_DARK    = web_rgb(0, 0, 3);
    COL_GREEN_BRIGHT = web_rgb(0, 5, 1);
    COL_BROWN        = 252;
    COL_BROWN_DARK   = web_rgb(2, 1, 0);

    memset(f->fb, COL_BLACK, sizeof(f->fb));
}

uint8_t palette_nearest(const Framebuffer *f, uint8_t r, uint8_t g, uint8_t b) {
    uint8_t best = 0;
    int best_dist = 0x7FFFFFFF;
    for (int i = 0; i < 256; i++) {
        uint32_t c = f->rgba[i];
        int pr = (int)((c >> 16) & 0xFF);
        int pg = (int)((c >> 8) & 0xFF);
        int pb = (int)(c & 0xFF);
        int dr = pr - (int)r;
        int dg = pg - (int)g;
        int db = pb - (int)b;
        int d = dr * dr + dg * dg + db * db;
        if (d < best_dist) {
            best_dist = d;
            best = (uint8_t)i;
        }
    }
    return best;
}

void gfx_clear(Framebuffer *f, uint8_t col) {
    memset(f->fb, col, sizeof(f->fb));
}

void gfx_pixel(Framebuffer *f, int x, int y, uint8_t col) {
    if ((unsigned)x < SCREEN_W && (unsigned)y < SCREEN_H) {
        f->fb[y * SCREEN_W + x] = col;
    }
}

void gfx_rect(Framebuffer *f, int x, int y, int w, int h, uint8_t col) {
    if (w <= 0 || h <= 0) return;
    int x0 = x < 0 ? 0 : x;
    int y0 = y < 0 ? 0 : y;
    int x1 = x + w > SCREEN_W ? SCREEN_W : x + w;
    int y1 = y + h > SCREEN_H ? SCREEN_H : y + h;
    if (x0 >= x1 || y0 >= y1) return;
    int span = x1 - x0;
    for (int row = y0; row < y1; row++) {
        memset(&f->fb[row * SCREEN_W + x0], col, (size_t)span);
    }
}

void gfx_rect_outline(Framebuffer *f, int x, int y, int w, int h, uint8_t col) {
    if (w <= 0 || h <= 0) return;
    gfx_rect(f, x, y, w, 1, col);
    gfx_rect(f, x, y + h - 1, w, 1, col);
    gfx_rect(f, x, y, 1, h, col);
    gfx_rect(f, x + w - 1, y, 1, h, col);
}

void gfx_circle(Framebuffer *f, int cx, int cy, int r, uint8_t col) {
    if (r <= 0) {
        gfx_pixel(f, cx, cy, col);
        return;
    }
    int r2 = r * r;
    for (int dy = -r; dy <= r; dy++) {
        int py = cy + dy;
        if ((unsigned)py >= SCREEN_H) continue;
        int dx_max = 0;
        while ((dx_max + 1) * (dx_max + 1) + dy * dy <= r2) dx_max++;
        int x0 = cx - dx_max;
        int x1 = cx + dx_max;
        if (x0 < 0) x0 = 0;
        if (x1 >= SCREEN_W) x1 = SCREEN_W - 1;
        if (x0 <= x1) {
            memset(&f->fb[py * SCREEN_W + x0], col, (size_t)(x1 - x0 + 1));
        }
    }
}

void gfx_circle_outline(Framebuffer *f, int cx, int cy, int r, uint8_t col) {
    int x = r, y = 0;
    int err = 1 - x;
    while (x >= y) {
        gfx_pixel(f, cx + x, cy + y, col);
        gfx_pixel(f, cx + y, cy + x, col);
        gfx_pixel(f, cx - y, cy + x, col);
        gfx_pixel(f, cx - x, cy + y, col);
        gfx_pixel(f, cx - x, cy - y, col);
        gfx_pixel(f, cx - y, cy - x, col);
        gfx_pixel(f, cx + y, cy - x, col);
        gfx_pixel(f, cx + x, cy - y, col);
        y++;
        if (err < 0) {
            err += 2 * y + 1;
        } else {
            x--;
            err += 2 * (y - x + 1);
        }
    }
}

void gfx_line(Framebuffer *f, int x0, int y0, int x1, int y1, uint8_t col) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        gfx_pixel(f, x0, y0, col);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

static void swap_int(int *a, int *b) { int t = *a; *a = *b; *b = t; }

void gfx_triangle(Framebuffer *f, int x0, int y0, int x1, int y1, int x2, int y2, uint8_t col) {
    if (y0 > y1) { swap_int(&y0, &y1); swap_int(&x0, &x1); }
    if (y1 > y2) { swap_int(&y1, &y2); swap_int(&x1, &x2); }
    if (y0 > y1) { swap_int(&y0, &y1); swap_int(&x0, &x1); }
    int total_h = y2 - y0;
    if (total_h == 0) return;
    for (int i = 0; i < total_h; i++) {
        int second_half = i > y1 - y0 || y1 == y0;
        int seg_h = second_half ? y2 - y1 : y1 - y0;
        if (seg_h == 0) seg_h = 1;
        float alpha = (float)i / (float)total_h;
        float beta  = (float)(i - (second_half ? y1 - y0 : 0)) / (float)seg_h;
        int ax = x0 + (int)((x2 - x0) * alpha);
        int bx = second_half ? x1 + (int)((x2 - x1) * beta) : x0 + (int)((x1 - x0) * beta);
        if (ax > bx) swap_int(&ax, &bx);
        int py = y0 + i;
        if ((unsigned)py < SCREEN_H) {
            int sx = ax < 0 ? 0 : ax;
            int ex = bx >= SCREEN_W ? SCREEN_W - 1 : bx;
            if (sx <= ex) {
                memset(&f->fb[py * SCREEN_W + sx], col, (size_t)(ex - sx + 1));
            }
        }
    }
}

/* Compact 5x7 ASCII font (32..126) */
static const uint8_t font5x7[][5] = {
    {0x00,0x00,0x00,0x00,0x00}, /* ' ' */
    {0x00,0x00,0x5F,0x00,0x00}, /* '!' */
    {0x00,0x07,0x00,0x07,0x00}, /* '"' */
    {0x14,0x7F,0x14,0x7F,0x14}, /* '#' */
    {0x24,0x2A,0x7F,0x2A,0x12}, /* '$' */
    {0x23,0x13,0x08,0x64,0x62}, /* '%' */
    {0x36,0x49,0x55,0x22,0x50}, /* '&' */
    {0x00,0x05,0x03,0x00,0x00}, /* ''' */
    {0x00,0x1C,0x22,0x41,0x00}, /* '(' */
    {0x00,0x41,0x22,0x1C,0x00}, /* ')' */
    {0x14,0x08,0x3E,0x08,0x14}, /* '*' */
    {0x08,0x08,0x3E,0x08,0x08}, /* '+' */
    {0x00,0x50,0x30,0x00,0x00}, /* ',' */
    {0x08,0x08,0x08,0x08,0x08}, /* '-' */
    {0x00,0x60,0x60,0x00,0x00}, /* '.' */
    {0x20,0x10,0x08,0x04,0x02}, /* '/' */
    {0x3E,0x51,0x49,0x45,0x3E}, /* '0' */
    {0x00,0x42,0x7F,0x40,0x00}, /* '1' */
    {0x42,0x61,0x51,0x49,0x46}, /* '2' */
    {0x21,0x41,0x45,0x4B,0x31}, /* '3' */
    {0x18,0x14,0x12,0x7F,0x10}, /* '4' */
    {0x27,0x45,0x45,0x45,0x39}, /* '5' */
    {0x3C,0x4A,0x49,0x49,0x30}, /* '6' */
    {0x01,0x71,0x09,0x05,0x03}, /* '7' */
    {0x36,0x49,0x49,0x49,0x36}, /* '8' */
    {0x06,0x49,0x49,0x29,0x1E}, /* '9' */
    {0x00,0x36,0x36,0x00,0x00}, /* ':' */
    {0x00,0x56,0x36,0x00,0x00}, /* ';' */
    {0x08,0x14,0x22,0x41,0x00}, /* '<' */
    {0x14,0x14,0x14,0x14,0x14}, /* '=' */
    {0x00,0x41,0x22,0x14,0x08}, /* '>' */
    {0x02,0x01,0x51,0x09,0x06}, /* '?' */
    {0x32,0x49,0x79,0x41,0x3E}, /* '@' */
    {0x7E,0x11,0x11,0x11,0x7E}, /* 'A' */
    {0x7F,0x49,0x49,0x49,0x36}, /* 'B' */
    {0x3E,0x41,0x41,0x41,0x22}, /* 'C' */
    {0x7F,0x41,0x41,0x22,0x1C}, /* 'D' */
    {0x7F,0x49,0x49,0x49,0x41}, /* 'E' */
    {0x7F,0x09,0x09,0x09,0x01}, /* 'F' */
    {0x3E,0x41,0x49,0x49,0x7A}, /* 'G' */
    {0x7F,0x08,0x08,0x08,0x7F}, /* 'H' */
    {0x00,0x41,0x7F,0x41,0x00}, /* 'I' */
    {0x20,0x40,0x41,0x3F,0x01}, /* 'J' */
    {0x7F,0x08,0x14,0x22,0x41}, /* 'K' */
    {0x7F,0x40,0x40,0x40,0x40}, /* 'L' */
    {0x7F,0x02,0x0C,0x02,0x7F}, /* 'M' */
    {0x7F,0x04,0x08,0x10,0x7F}, /* 'N' */
    {0x3E,0x41,0x41,0x41,0x3E}, /* 'O' */
    {0x7F,0x09,0x09,0x09,0x06}, /* 'P' */
    {0x3E,0x41,0x51,0x21,0x5E}, /* 'Q' */
    {0x7F,0x09,0x19,0x29,0x46}, /* 'R' */
    {0x46,0x49,0x49,0x49,0x31}, /* 'S' */
    {0x01,0x01,0x7F,0x01,0x01}, /* 'T' */
    {0x3F,0x40,0x40,0x40,0x3F}, /* 'U' */
    {0x1F,0x20,0x40,0x20,0x1F}, /* 'V' */
    {0x3F,0x40,0x38,0x40,0x3F}, /* 'W' */
    {0x63,0x14,0x08,0x14,0x63}, /* 'X' */
    {0x07,0x08,0x70,0x08,0x07}, /* 'Y' */
    {0x61,0x51,0x49,0x45,0x43}, /* 'Z' */
    {0x00,0x7F,0x41,0x41,0x00}, /* '[' */
    {0x02,0x04,0x08,0x10,0x20}, /* '\' */
    {0x00,0x41,0x41,0x7F,0x00}, /* ']' */
    {0x04,0x02,0x01,0x02,0x04}, /* '^' */
    {0x40,0x40,0x40,0x40,0x40}, /* '_' */
    {0x00,0x01,0x02,0x04,0x00}, /* '`' */
    {0x20,0x54,0x54,0x54,0x78}, /* 'a' */
    {0x7F,0x48,0x44,0x44,0x38}, /* 'b' */
    {0x38,0x44,0x44,0x44,0x20}, /* 'c' */
    {0x38,0x44,0x44,0x48,0x7F}, /* 'd' */
    {0x38,0x54,0x54,0x54,0x18}, /* 'e' */
    {0x08,0x7E,0x09,0x01,0x02}, /* 'f' */
    {0x0C,0x52,0x52,0x52,0x3E}, /* 'g' */
    {0x7F,0x08,0x04,0x04,0x78}, /* 'h' */
    {0x00,0x44,0x7D,0x40,0x00}, /* 'i' */
    {0x20,0x40,0x44,0x3D,0x00}, /* 'j' */
    {0x7F,0x10,0x28,0x44,0x00}, /* 'k' */
    {0x00,0x41,0x7F,0x40,0x00}, /* 'l' */
    {0x7C,0x04,0x18,0x04,0x78}, /* 'm' */
    {0x7C,0x08,0x04,0x04,0x78}, /* 'n' */
    {0x38,0x44,0x44,0x44,0x38}, /* 'o' */
    {0x7C,0x14,0x14,0x14,0x08}, /* 'p' */
    {0x08,0x14,0x14,0x18,0x7C}, /* 'q' */
    {0x7C,0x08,0x04,0x04,0x08}, /* 'r' */
    {0x48,0x54,0x54,0x54,0x20}, /* 's' */
    {0x04,0x3F,0x44,0x40,0x20}, /* 't' */
    {0x3C,0x40,0x40,0x20,0x7C}, /* 'u' */
    {0x1C,0x20,0x40,0x20,0x1C}, /* 'v' */
    {0x3C,0x40,0x30,0x40,0x3C}, /* 'w' */
    {0x44,0x28,0x10,0x28,0x44}, /* 'x' */
    {0x0C,0x50,0x50,0x50,0x3C}, /* 'y' */
    {0x44,0x64,0x54,0x4C,0x44}, /* 'z' */
    {0x00,0x08,0x36,0x41,0x00}, /* '{' */
    {0x00,0x00,0x7F,0x00,0x00}, /* '|' */
    {0x00,0x41,0x36,0x08,0x00}, /* '}' */
    {0x08,0x04,0x08,0x10,0x08}, /* '~' */
};

static void draw_char(Framebuffer *f, int x, int y, int scale, uint8_t col, char c) {
    if (c < 32 || c > 126) c = '?';
    const uint8_t *glyph = font5x7[c - 32];
    for (int col_i = 0; col_i < 5; col_i++) {
        uint8_t bits = glyph[col_i];
        for (int row = 0; row < 7; row++) {
            if (bits & (1 << row)) {
                if (scale == 1) {
                    gfx_pixel(f, x + col_i, y + row, col);
                } else {
                    gfx_rect(f, x + col_i * scale, y + row * scale, scale, scale, col);
                }
            }
        }
    }
}

void gfx_text(Framebuffer *f, int x, int y, int scale, uint8_t col, const char *fmt, ...) {
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    int cx = x;
    for (int i = 0; buf[i]; i++) {
        if (buf[i] == '\n') {
            cx = x;
            y += 9 * scale;
            continue;
        }
        draw_char(f, cx, y, scale, col, buf[i]);
        cx += 6 * scale;
    }
}

int gfx_text_width(int scale, const char *text) {
    int len = (int)strlen(text);
    if (len == 0) return 0;
    return len * 6 * scale - scale;
}

void gfx_present_argb8888(const Framebuffer *f, uint32_t *dst_pixels, int dst_pitch_bytes) {
    int pitch_u32 = dst_pitch_bytes / 4;
    const uint32_t *pal = f->rgba;
    for (int y = 0; y < SCREEN_H; y++) {
        const uint8_t *src = &f->fb[y * SCREEN_W];
        uint32_t *dst = &dst_pixels[y * pitch_u32];
        for (int x = 0; x < SCREEN_W; x++) {
            dst[x] = pal[src[x]];
        }
    }
}
