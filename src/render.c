#include "render.h"
#include "audio.h"
#include "version.h"
#include <math.h>
#include <stdio.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static void draw_carrier_friendly(const Game *g, Framebuffer *f, int cam_x, int cam_y) {
    int sx = (int)(g->carrier.x - cam_x);
    int sy = (int)(g->carrier.y - cam_y);
    if (sx < -160 || sx > SCREEN_W + 160 || sy < -220 || sy > SCREEN_H + 220) return;

    /* Wake behind carrier (south of carrier since it travels north) */
    int wake_len = 56 + (int)(sinf(g->time_sec * 8.0f) * 6.0f);
    for (int i = 0; i < wake_len; i += 5) {
        int spread = 16 + i / 3;
        gfx_pixel(f, sx - spread, sy + 90 + i, COL_FOAM);
        gfx_pixel(f, sx + spread, sy + 90 + i, COL_FOAM);
        if ((i & 7) == 0) {
            gfx_line(f, sx - spread + 6, sy + 90 + i, sx + spread - 6, sy + 90 + i, COL_SHALLOW_SEA);
        }
    }

    /* Hull */
    gfx_rect(f, sx - 38, sy - 88, 76, 176, COL_GRAY_DARK);
    gfx_triangle(f, sx - 38, sy - 88, sx + 38, sy - 88, sx, sy - 120, COL_GRAY_DARK);
    /* Flight deck */
    gfx_rect(f, sx - 32, sy - 82, 64, 164, COL_GRAY_MID);
    gfx_triangle(f, sx - 30, sy - 82, sx + 30, sy - 82, sx, sy - 110, COL_GRAY_MID);
    gfx_rect_outline(f, sx - 32, sy - 82, 64, 164, COL_GRAY_LIGHT);

    /* Runway stripes & helipad circle */
    for (int y = -70; y < 74; y += 14) {
        gfx_rect(f, sx - 2, sy + y, 4, 8, COL_WHITE);
    }
    gfx_circle_outline(f, sx, sy + 30, 18, COL_YELLOW);
    gfx_text(f, sx - 5, sy + 24, 2, COL_YELLOW, "H");

    /* Control island (starboard side) */
    gfx_rect(f, sx + 18, sy - 26, 16, 46, COL_GRAY_LIGHT);
    gfx_rect(f, sx + 21, sy - 18, 9, 12, COL_CYAN);
    gfx_rect_outline(f, sx + 18, sy - 26, 16, 46, COL_BLACK);

    /* HP bar above carrier */
    int bw = 68;
    int fill = (g->carrier.hp * bw) / g->carrier.max_hp;
    gfx_rect(f, sx - bw / 2, sy - 130, bw, 6, COL_BLACK);
    gfx_rect(f, sx - bw / 2 + 1, sy - 129, fill > 2 ? fill - 2 : 0, 4,
             g->carrier.hp > g->carrier.max_hp / 3 ? COL_GREEN_BRIGHT : COL_RED);
}

static void gfx_quad_i(Framebuffer *f, int x0, int y0, int x1, int y1,
                       int x2, int y2, int x3, int y3, uint8_t col) {
    gfx_triangle(f, x0, y0, x1, y1, x2, y2, col);
    gfx_triangle(f, x0, y0, x2, y2, x3, y3, col);
}

/* SimCity-style 2.5D oblique box: shows roof + front (south) + left (west) faces */
static int oblique_lw(int h) {
    int lw = (h * 2 + 1) / 3;
    return (lw < 3) ? 3 : lw;
}

static void gfx_box_3d(Framebuffer *f, int rx, int ry, int rw, int rd, int h,
                       uint8_t roof_col, uint8_t front_col, uint8_t left_col, uint8_t edge_col) {
    int lw = oblique_lw(h);
    /* Ground drop shadow on bottom-left */
    gfx_quad_i(f, rx - lw, ry + h + 2, rx - lw, ry + rd + h,
               rx - lw - 3, ry + rd + h + 2, rx - lw - 3, ry + h + 4, COL_BLACK);
    gfx_quad_i(f, rx - lw, ry + rd + h, rx + rw - lw, ry + rd + h,
               rx + rw - lw - 3, ry + rd + h + 2, rx - lw - 3, ry + rd + h + 2, COL_BLACK);
    /* Left (west) wall */
    gfx_quad_i(f, rx, ry, rx, ry + rd, rx - lw, ry + rd + h, rx - lw, ry + h, left_col);
    /* Front (south) wall */
    gfx_quad_i(f, rx, ry + rd, rx + rw, ry + rd, rx + rw - lw, ry + rd + h, rx - lw, ry + rd + h, front_col);
    /* Roof + crisp top-right bevel */
    gfx_rect(f, rx, ry, rw, rd, roof_col);
    gfx_rect_outline(f, rx, ry, rw, rd, edge_col);
    /* Crisp outer silhouette & corner edges */
    gfx_line(f, rx, ry, rx - lw, ry + h, edge_col);
    gfx_line(f, rx - lw, ry + h, rx - lw, ry + rd + h, edge_col);
    gfx_line(f, rx, ry + rd, rx - lw, ry + rd + h, edge_col);
    gfx_line(f, rx - lw, ry + rd + h, rx + rw - lw, ry + rd + h, edge_col);
    gfx_line(f, rx + rw, ry + rd, rx + rw - lw, ry + rd + h, edge_col);
}

/* 2.5D oblique cylinder for silos, oil tanks, cooling towers, and bunkers */
static void gfx_cyl_3d(Framebuffer *f, int rx, int ry, int r, int h,
                       uint8_t top_col, uint8_t side_col, uint8_t edge_col) {
    int lw = oblique_lw(h);
    /* Ground shadow */
    gfx_circle(f, rx - lw - 2, ry + h + 2, r, COL_BLACK);
    /* Extruded cylinder body */
    int steps = (h > lw) ? h : lw;
    if (steps < 1) steps = 1;
    for (int i = steps; i >= 1; i--) {
        int cx = rx - (lw * i) / steps;
        int cy = ry + (h * i) / steps;
        gfx_circle(f, cx, cy, r, (i == steps) ? edge_col : side_col);
    }
    gfx_circle(f, rx, ry, r, top_col);
    gfx_circle_outline(f, rx, ry, r, edge_col);
}

/* Perspective-aligned windows on both front and left walls of a 2.5D box */
static void gfx_windows_3d(Framebuffer *f, int rx, int ry, int rw, int rd, int h,
                           int cols, int rows, uint8_t front_win, uint8_t left_win) {
    int lw = oblique_lw(h);
    int win_h = (h / (rows + 1) >= 4) ? 2 : 1;
    int win_w = (rw - 6) / (cols * 2);
    if (win_w < 2) win_w = 2;
    for (int r = 0; r < rows; r++) {
        int wy = 2 + (r * (h - 3)) / rows;
        int shift = (wy * lw) / h;
        /* Front wall windows */
        for (int c = 0; c < cols; c++) {
            int wx = rx + 3 + (c * (rw - 5)) / cols - shift;
            gfx_rect(f, wx, ry + rd + wy, win_w, win_h, front_win);
        }
        /* Left wall slanted window band */
        if (lw >= 5) {
            int lx = rx - shift - 1;
            int ly = ry + wy + 2;
            int l_len = (rd > 6) ? (rd - 4) : (rd - 2);
            if (l_len > 1) {
                gfx_line(f, lx, ly, lx, ly + l_len, left_win);
            }
        }
    }
}

/* 2.5D gabled house/cottage for residential bungalows */
static void gfx_house_3d(Framebuffer *f, int rx, int ry, int rw, int rd, int h,
                         uint8_t roof_col, uint8_t ridge_col, uint8_t front_col, uint8_t left_col,
                         uint8_t door_col, uint8_t win_col) {
    gfx_box_3d(f, rx, ry, rw, rd, h, roof_col, front_col, left_col, COL_BROWN_DARK);
    /* Pitched roof ridge + sunlit front slope */
    int mid_y = ry + rd / 2;
    gfx_rect(f, rx + 1, mid_y, rw - 2, rd / 2, ridge_col);
    gfx_line(f, rx, mid_y, rx + rw - 1, mid_y, COL_BROWN_DARK);
    /* Front door & window aligned to front wall */
    int lw = oblique_lw(h);
    int fx = rx - lw / 2;
    int fy = ry + rd + h / 2;
    gfx_rect(f, fx + 3, fy - 1, 3, h - h / 2, door_col);
    gfx_rect(f, fx + rw - 6, fy, 3, 2, win_col);
}

static void draw_village_house(Framebuffer *f, int cx, int cy, int style, int damaged) {
    const uint8_t roofs[5]  = {COL_RED_DARK,   COL_SAND_DARK,  COL_DEEP_SEA,  COL_BROWN_DARK, COL_GRASS_DARK};
    const uint8_t ridges[5] = {COL_ORANGE,     COL_YELLOW,     COL_CYAN,      COL_RED,        COL_GRASS_MID};
    const uint8_t fronts[5] = {COL_WHITE,      COL_SAND_LIGHT, COL_WHITE,     COL_SAND_LIGHT, COL_WHITE};
    const uint8_t lefts[5]  = {COL_GRAY_MID,   COL_BROWN,      COL_GRAY_MID,  COL_BROWN_DARK, COL_GRAY_MID};
    static const int widths[5]     = {15,             14,             16,            15,             21};
    static const int depths[5]     = {8,              8,              9,             9,              10};
    static const int heights[5]    = {6,              5,              6,             10,             6};
    int k = style % 5;
    int rw = widths[k], rd = depths[k], h = heights[k];
    gfx_house_3d(f, cx - rw / 2, cy - rd / 2, rw, rd, h,
                 damaged ? COL_BROWN_DARK : roofs[k],
                 damaged ? COL_ROCK_DARK  : ridges[k],
                 damaged ? COL_GRAY_DARK  : fronts[k],
                 damaged ? COL_BLACK      : lefts[k],
                 COL_BROWN_DARK, damaged ? COL_BLACK : COL_CYAN);
    if (k == 3) {
        /* Extra upper-story window row for two-story townhouse */
        int lw = oblique_lw(h);
        gfx_rect(f, cx - rw / 2 + 3 - lw / 4, cy + rd / 2 + 2, 3, 2, damaged ? COL_BLACK : COL_YELLOW);
        gfx_rect(f, cx + rw / 2 - 6 - lw / 4, cy + rd / 2 + 2, 3, 2, damaged ? COL_BLACK : COL_YELLOW);
    }
}

static uint32_t village_hash(uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}

/* Little SimCity-style orthogonal road tiles connecting two village block centers */
static void draw_village_road_segment(Framebuffer *f, int x0, int y0, int x1, int y1, int is_paved) {
    uint8_t curb = is_paved ? COL_GRAY_DARK : COL_BROWN_DARK;
    uint8_t surf = is_paved ? COL_GRAY_MID  : COL_SAND_DARK;
    uint8_t mark = is_paved ? COL_YELLOW    : COL_SAND_LIGHT;
    int rx = (x0 < x1) ? x0 : x1;
    int rw = (x0 < x1 ? x1 - x0 : x0 - x1) + 1;
    int ry = (y0 < y1) ? y0 : y1;
    int rh = (y0 < y1 ? y1 - y0 : y0 - y1) + 1;
    /* Horizontal leg along y0 */
    if (rw > 2) {
        gfx_rect(f, rx - 3, y0 - 3, rw + 6, 7, curb);
        gfx_rect(f, rx - 2, y0 - 2, rw + 4, 5, surf);
        for (int dx = 2; dx < rw - 2; dx += 6) {
            gfx_rect(f, rx + dx, y0, 2, 1, mark);
        }
    }
    /* Vertical leg along x1 */
    if (rh > 2) {
        gfx_rect(f, x1 - 3, ry - 3, 7, rh + 6, curb);
        gfx_rect(f, x1 - 2, ry - 2, 5, rh + 4, surf);
        for (int dy = 2; dy < rh - 2; dy += 6) {
            gfx_rect(f, x1, ry + dy, 1, 2, mark);
        }
    }
}

/* Modular ~34x28px village block sprites assembled into diverse neighborhoods */
static void draw_village_block(Framebuffer *f, int cx, int cy, int block_type,
                               uint32_t sub_seed, int damaged) {
    int s0 = (int)(sub_seed % 5U);
    int s1 = (int)((sub_seed >> 3) % 5U);
    switch (block_type % 14) {
        case 0: /* Cottage pair */
            draw_village_house(f, cx - 7, cy - 6, (s0 == 3) ? 0 : s0, damaged);
            draw_village_house(f, cx + 8, cy + 5, (s1 == 3 || s1 == 4) ? 1 : s1, damaged);
            break;
        case 1: /* Two-story townhouse + courtyard */
            gfx_rect(f, cx - 12, cy - 8, 24, 18, damaged ? COL_ROCK_DARK : COL_GRASS_MID);
            draw_village_house(f, cx, cy - 2, 3, damaged);
            break;
        case 2: /* Tropical market longhouse + awning */
            draw_village_house(f, cx, cy - 3, 4, damaged);
            gfx_rect(f, cx - 8, cy + 8, 16, 4, damaged ? COL_GRAY_DARK : COL_ORANGE);
            gfx_rect(f, cx - 6, cy + 8, 4, 4, damaged ? COL_ROCK_DARK : COL_YELLOW);
            gfx_rect(f, cx + 2, cy + 8, 4, 4, damaged ? COL_ROCK_DARK : COL_RED);
            break;
        case 3: /* Cobble fountain plaza */
            gfx_rect(f, cx - 14, cy - 11, 28, 22, damaged ? COL_ROCK_DARK : COL_GRAY_MID);
            gfx_rect_outline(f, cx - 14, cy - 11, 28, 22, COL_GRAY_DARK);
            gfx_cyl_3d(f, cx + 1, cy - 1, 6, 4,
                       damaged ? COL_ROCK_DARK : COL_SHALLOW_SEA,
                       damaged ? COL_GRAY_DARK : COL_WHITE, COL_GRAY_DARK);
            break;
        case 4: /* Villa + swimming pool */
            gfx_rect(f, cx - 14, cy + 2, 14, 10, COL_WHITE);
            gfx_rect(f, cx - 12, cy + 4, 10, 6, damaged ? COL_ROCK_DARK : COL_SHALLOW_SEA);
            draw_village_house(f, cx + 5, cy - 5, (s0 % 3), damaged);
            break;
        case 5: /* Corner cafe / shop with striped awning */
            gfx_box_3d(f, cx - 8, cy - 9, 18, 12, 7,
                       damaged ? COL_BROWN_DARK : COL_SAND_LIGHT,
                       damaged ? COL_GRAY_DARK : COL_WHITE,
                       damaged ? COL_BLACK : COL_GRAY_MID, COL_BLACK);
            gfx_rect(f, cx - 10, cy + 3, 18, 3, damaged ? COL_GRAY_DARK : COL_RED);
            gfx_rect(f, cx - 6,  cy + 3, 4, 3, damaged ? COL_ROCK_DARK : COL_WHITE);
            gfx_rect(f, cx + 2,  cy + 3, 4, 3, damaged ? COL_ROCK_DARK : COL_WHITE);
            break;
        case 6: /* Tropical palm park */
            gfx_rect(f, cx - 14, cy - 11, 28, 22, damaged ? COL_ROCK_DARK : COL_GRASS_MID);
            gfx_rect(f, cx - 14, cy - 2, 28, 4, COL_SAND_DARK);
            for (int p = -1; p <= 1; p += 2) {
                int px = cx + p * 7;
                int py = cy + p * 4;
                gfx_line(f, px, py + 4, px + 1, py - 4, COL_BROWN);
                gfx_circle(f, px + 1, py - 5, 4, damaged ? COL_BROWN_DARK : COL_GREEN_BRIGHT);
            }
            break;
        case 7: /* Compact 2.5D glass resort mid-rise */
            gfx_box_3d(f, cx - 9, cy - 14, 20, 13, 14,
                       damaged ? COL_GRAY_DARK : COL_WHITE,
                       damaged ? COL_GRAY_DARK : web_rgb(1, 3, 4),
                       damaged ? COL_BLACK : web_rgb(0, 2, 3), COL_BLACK);
            gfx_windows_3d(f, cx - 9, cy - 14, 20, 13, 14, 4, 3,
                           damaged ? COL_BLACK : COL_CYAN,
                           damaged ? COL_BLACK : COL_SHALLOW_SEA);
            break;
        case 8: /* L-shaped village inn / manor */
            gfx_house_3d(f, cx - 11, cy - 8, 19, 9, 7,
                         damaged ? COL_BROWN_DARK : COL_RED_DARK,
                         damaged ? COL_ROCK_DARK : COL_ORANGE,
                         damaged ? COL_GRAY_DARK : COL_SAND_LIGHT,
                         damaged ? COL_BLACK : COL_BROWN,
                         COL_BROWN_DARK, damaged ? COL_BLACK : COL_YELLOW);
            gfx_house_3d(f, cx + 1, cy, 11, 9, 6,
                         damaged ? COL_BROWN_DARK : COL_RED_DARK,
                         damaged ? COL_ROCK_DARK : COL_ORANGE,
                         damaged ? COL_GRAY_DARK : COL_WHITE,
                         damaged ? COL_BLACK : COL_GRAY_MID,
                         COL_BROWN_DARK, damaged ? COL_BLACK : COL_CYAN);
            break;
        case 9: /* Whitewashed village chapel with steeple */
            gfx_house_3d(f, cx - 8, cy - 5, 18, 10, 7,
                         damaged ? COL_BROWN_DARK : COL_RED_DARK,
                         damaged ? COL_ROCK_DARK : COL_RED,
                         damaged ? COL_GRAY_DARK : COL_WHITE,
                         damaged ? COL_BLACK : COL_GRAY_MID,
                         COL_BROWN_DARK, damaged ? COL_BLACK : COL_CYAN);
            gfx_box_3d(f, cx - 2, cy - 14, 7, 7, 11,
                       damaged ? COL_GRAY_DARK : COL_YELLOW,
                       damaged ? COL_GRAY_DARK : COL_WHITE,
                       damaged ? COL_BLACK : COL_GRAY_MID, COL_BLACK);
            gfx_line(f, cx + 1, cy - 14, cx + 1, cy - 18, COL_YELLOW);
            break;
        case 10: /* Brick town hall with clock tower */
            gfx_box_3d(f, cx - 10, cy - 7, 22, 12, 8,
                       damaged ? COL_BROWN_DARK : COL_SAND_LIGHT,
                       damaged ? COL_GRAY_DARK : COL_BROWN,
                       damaged ? COL_BLACK : COL_BROWN_DARK, COL_BLACK);
            gfx_box_3d(f, cx - 2, cy - 13, 8, 7, 9,
                       damaged ? COL_GRAY_DARK : COL_GRAY_LIGHT,
                       damaged ? COL_ROCK_DARK : COL_WHITE,
                       damaged ? COL_BLACK : COL_GRAY_MID, COL_BLACK);
            gfx_rect(f, cx - 2, cy - 3, 3, 3, damaged ? COL_BLACK : COL_CYAN);
            break;
        case 11: /* Beach tiki bar + sun umbrellas */
            gfx_rect(f, cx - 13, cy - 7, 26, 16, damaged ? COL_ROCK_DARK : COL_BROWN);
            gfx_house_3d(f, cx - 9, cy - 8, 16, 8, 5,
                         damaged ? COL_BROWN_DARK : COL_SAND_DARK,
                         damaged ? COL_ROCK_DARK : COL_YELLOW,
                         damaged ? COL_GRAY_DARK : COL_BROWN,
                         damaged ? COL_BLACK : COL_BROWN_DARK,
                         COL_BLACK, damaged ? COL_BLACK : COL_ORANGE);
            gfx_circle(f, cx + 8, cy + 4, 4, damaged ? COL_GRAY_DARK : COL_CYAN);
            gfx_circle(f, cx - 7, cy + 5, 3, damaged ? COL_GRAY_DARK : COL_RED);
            break;
        case 12: /* Village green / tennis court */
            gfx_rect(f, cx - 13, cy - 10, 26, 20, damaged ? COL_ROCK_DARK : COL_GRASS_DARK);
            gfx_rect_outline(f, cx - 11, cy - 8, 22, 16, damaged ? COL_GRAY_DARK : COL_WHITE);
            gfx_line(f, cx, cy - 8, cx, cy + 7, damaged ? COL_GRAY_MID : COL_WHITE);
            break;
        case 13: /* Row of 3 colorful narrow rowhouses */
            for (int r = 0; r < 3; r++) {
                int rx = cx - 9 + r * 9;
                draw_village_house(f, rx, cy - 2 + (r & 1) * 2, (s0 + r) % 3, damaged);
            }
            break;
    }
}

static int village_spot_on_land(const World *w, float wx, float wy) {
    if (!w) return 1;
    return world_is_land(w, (int)wx, (int)wy) &&
           world_is_land(w, (int)(wx - 10.0f), (int)(wy - 8.0f)) &&
           world_is_land(w, (int)(wx + 10.0f), (int)(wy - 8.0f)) &&
           world_is_land(w, (int)(wx - 10.0f), (int)(wy + 8.0f)) &&
           world_is_land(w, (int)(wx + 10.0f), (int)(wy + 8.0f));
}

static void draw_structure(const Structure *s, Framebuffer *f, const World *w,
                           int cam_x, int cam_y, float time_sec) {
    int sx = (int)(s->x - cam_x);
    int sy = (int)(s->y - cam_y);
    if (sx < -160 || sx > SCREEN_W + 160 || sy < -160 || sy > SCREEN_H + 160) return;

    switch (s->type) {
        case STRUCT_FACTORY: {
            int hw = (s->size == 0) ? 24 : (s->size == 1 ? 32 : 56);
            int hh = (s->size == 0) ? 16 : (s->size == 1 ? 24 : 40);

            if (s->size < 2) {
                /* Small (48x32) and Medium (64x48) 2.5D factories */
                int rx = sx - hw + 2, ry = sy - hh - 4, rw = hw * 2 - 4, rd = hh * 2 - 4;
                gfx_box_3d(f, rx, ry, rw, rd, 9, COL_GRAY_MID, COL_GRAY_DARK, COL_ROCK_DARK, COL_BLACK);
                /* Front bay doors aligned to oblique base */
                for (int dx = 8; dx < rw - 10; dx += 14) {
                    gfx_rect(f, rx + dx - 5, ry + rd + 3, 8, 6, COL_BLACK);
                }
                int bays = s->size + 3;
                int bay_w = (rw - 8) / bays;
                for (int b = 0; b < bays; b++) {
                    int bx = rx + 4 + b * bay_w;
                    gfx_rect(f, bx, ry + 4, bay_w - 2, rd - 8, COL_GRAY_DARK);
                    gfx_line(f, bx, ry + 4, bx + bay_w - 2, ry + 4, COL_GRAY_LIGHT);
                }
                /* 3D Cylindrical Smokestacks */
                gfx_cyl_3d(f, rx + 8, ry - 6, 4, 9, COL_GRAY_LIGHT, COL_RED_DARK, COL_BLACK);
                gfx_cyl_3d(f, rx + rw - 8, ry - 6, 4, 9, COL_GRAY_LIGHT, COL_RED_DARK, COL_BLACK);
                if (s->size == 1) {
                    gfx_box_3d(f, rx - 8, ry + rd / 2 - 6, 8, 14, 6, COL_GRAY_MID, COL_ROCK_DARK, COL_BROWN_DARK, COL_BLACK);
                }
            } else {
                /* Large multi-building 2.5D industrial complex (112x80) */
                gfx_rect(f, sx - hw, sy - hh, hw * 2, hh * 2, COL_ROCK_DARK);
                gfx_rect_outline(f, sx - hw, sy - hh, hw * 2, hh * 2, COL_BROWN_DARK);

                /* Main manufacturing hall (top-left, tall 2.5D extrusion) */
                int mx = sx - 46, my = sy - 38, mw = 62, md = 36;
                gfx_box_3d(f, mx, my, mw, md, 12, COL_GRAY_MID, COL_GRAY_DARK, COL_BROWN_DARK, COL_BLACK);
                for (int b = 0; b < 4; b++) {
                    int rx = mx + 4 + b * 14;
                    gfx_rect(f, rx, my + 4, 12, md - 8, COL_GRAY_DARK);
                    gfx_line(f, rx, my + 4, rx + 12, my + 4, COL_GRAY_LIGHT);
                }
                for (int dx = 10; dx < mw - 10; dx += 14) {
                    gfx_rect(f, mx + dx - 7, my + md + 4, 9, 8, COL_BLACK);
                }
                /* 3 tall 3D cylindrical smokestacks on main hall */
                gfx_cyl_3d(f, mx + 10, my - 8, 4, 12, COL_GRAY_LIGHT, COL_RED_DARK, COL_BLACK);
                gfx_cyl_3d(f, mx + 30, my - 8, 4, 12, COL_GRAY_LIGHT, COL_RED_DARK, COL_BLACK);
                gfx_cyl_3d(f, mx + 50, my - 8, 4, 12, COL_GRAY_LIGHT, COL_RED_DARK, COL_BLACK);

                /* Secondary assembly wing (bottom-right, 2.5D) */
                gfx_box_3d(f, sx + 6, sy + 2, 44, 24, 9, COL_GRAY_MID, COL_GRAY_DARK, COL_BROWN_DARK, COL_BLACK);
                gfx_rect(f, sx + 12, sy + 7, 32, 8, COL_GRAY_DARK);

                /* Overhead pipework / conveyor connecting halls */
                gfx_rect(f, sx + 16, sy - 18, 26, 6, COL_ORANGE);
                gfx_rect(f, sx + 22, sy - 18, 6, 20, COL_ORANGE);

                /* Twin 3D storage silos (bottom-left) */
                gfx_cyl_3d(f, sx - 34, sy + 18, 8, 9, COL_GRAY_LIGHT, COL_GRAY_DARK, COL_BLACK);
                gfx_cyl_3d(f, sx - 14, sy + 18, 8, 9, COL_GRAY_LIGHT, COL_GRAY_DARK, COL_BLACK);
            }

            /* Level stars / status */
            for (int l = 0; l < s->level; l++) {
                gfx_rect(f, sx - (s->level * 9) / 2 + l * 9, sy + hh - 6, 6, 4, COL_ORANGE);
            }
            /* Visual damage: scorch marks and flickering fires (smoke is rendered in its own layer) */
            if (s->hp < s->max_hp) {
                int heavy = (s->hp * 2 <= s->max_hp);
                int fl = ((int)(time_sec * 11.0f)) & 1;
                if (s->size < 2) {
                    gfx_rect(f, sx - hw / 2, sy - hh / 2, 10, 7, COL_BLACK);
                    gfx_circle(f, sx - 4, sy - 2, fl ? 4 : 3, fl ? COL_ORANGE : COL_RED);
                    if (heavy) {
                        gfx_circle(f, sx + 6, sy + 2, fl ? 3 : 4, COL_YELLOW);
                    }
                } else {
                    /* Large complex: scorched main hall & assembly wing fires */
                    gfx_rect(f, sx - 36, sy - 24, 16, 12, COL_BLACK);
                    gfx_rect(f, sx + 14, sy + 8, 14, 10, COL_BLACK);
                    gfx_circle(f, sx - 28, sy - 16, fl ? 5 : 4, fl ? COL_ORANGE : COL_RED);
                    gfx_circle(f, sx + 22, sy + 12, fl ? 4 : 5, fl ? COL_RED : COL_ORANGE);
                    if (heavy) {
                        gfx_circle(f, sx - 8, sy - 8, fl ? 5 : 4, COL_YELLOW);
                    }
                }
            }
            /* Build progress bar */
            if (s->level < 3) {
                int bar_w = hw * 2 - 8;
                int pw = (int)(s->build_progress * (float)bar_w);
                gfx_rect(f, sx - bar_w / 2, sy + hh + 2, bar_w, 3, COL_BLACK);
                gfx_rect(f, sx - bar_w / 2, sy + hh + 2, pw, 3, COL_YELLOW);
            }
            break;
        }
        case STRUCT_SHIPYARD: {
            /* 2.5D Drydock / slipway */
            gfx_rect(f, sx - 24, sy - 22, 48, 44, COL_GRAY_DARK);
            gfx_rect(f, sx - 16, sy - 18, 32, 36, COL_SHALLOW_SEA);
            /* Extruded 2.5D concrete piers */
            gfx_box_3d(f, sx - 24, sy - 24, 10, 44, 6, COL_GRAY_MID, COL_GRAY_DARK, COL_ROCK_DARK, COL_BLACK);
            gfx_box_3d(f, sx + 14, sy - 24, 10, 44, 6, COL_GRAY_MID, COL_GRAY_DARK, COL_ROCK_DARK, COL_BLACK);
            /* 2.5D Gantry cranes */
            gfx_box_3d(f, sx - 22, sy - 10, 14, 4, 8, COL_YELLOW, COL_ORANGE, COL_BROWN, COL_BLACK);
            gfx_box_3d(f, sx + 8, sy + 2, 14, 4, 8, COL_YELLOW, COL_ORANGE, COL_BROWN, COL_BLACK);
            if (s->building_carrier && !s->carrier_launched) {
                /* 2.5D Hull under construction */
                int hull_h = (int)(6.0f + s->carrier_progress * 28.0f);
                gfx_box_3d(f, sx - 7, sy - hull_h / 2 - 2, 14, hull_h, 5,
                           COL_RED_DARK, COL_BROWN_DARK, COL_BLACK, COL_ORANGE);
                int pw = (int)(s->carrier_progress * 40.0f);
                gfx_rect(f, sx - 20, sy + 24, 40, 4, COL_BLACK);
                gfx_rect(f, sx - 20, sy + 24, pw, 4, COL_RED);
            }
            break;
        }
        case STRUCT_AA: {
            /* 2.5D Concrete bunker + rotating dual barrel */
            gfx_cyl_3d(f, sx + 2, sy - 3, 12, 6, COL_GRAY_MID, COL_GRAY_DARK, COL_BLACK);
            gfx_circle_outline(f, sx + 2, sy - 3, 11, COL_GRAY_LIGHT);
            gfx_cyl_3d(f, sx + 3, sy - 5, 6, 3, COL_ROCK_DARK, COL_BLACK, COL_GRAY_DARK);
            float ang = time_sec * 1.4f + s->x * 0.01f;
            int bx = (int)(cosf(ang) * 13.0f);
            int by = (int)(sinf(ang) * 13.0f);
            gfx_line(f, sx + 3, sy - 5, sx + 3 + bx, sy - 5 + by, COL_YELLOW);
            break;
        }
        case STRUCT_BARRACKS: {
            /* 2.5D military huts */
            gfx_rect_outline(f, sx - 20, sy - 14, 40, 32, COL_ROCK_DARK);
            gfx_box_3d(f, sx - 15, sy - 14, 14, 9, 5, COL_GRASS_MID, COL_GRASS_DARK, COL_BROWN_DARK, COL_BLACK);
            gfx_box_3d(f, sx + 3,  sy - 14, 14, 9, 5, COL_GRASS_MID, COL_GRASS_DARK, COL_BROWN_DARK, COL_BLACK);
            gfx_box_3d(f, sx - 15, sy + 1,  14, 9, 5, COL_BROWN, COL_BROWN_DARK, COL_ROCK_DARK, COL_BLACK);
            gfx_box_3d(f, sx + 3,  sy + 1,  14, 9, 5, COL_BROWN, COL_BROWN_DARK, COL_ROCK_DARK, COL_BLACK);
            break;
        }
        case STRUCT_RADAR: {
            /* 2.5D concrete bunker base */
            gfx_box_3d(f, sx - 9, sy - 3, 19, 13, 6, COL_GRAY_MID, COL_GRAY_DARK, COL_ROCK_DARK, COL_BLACK);
            /* Small SAM rail on right side of bunker */
            gfx_line(f, sx + 7, sy + 6, sx + 12, sy - 1, COL_GRASS_MID);
            gfx_line(f, sx + 8, sy + 6, sx + 13, sy - 1, COL_GRASS_DARK);
            gfx_pixel(f, sx + 12, sy - 1, COL_RED);
            /* Central rotating pedestal */
            gfx_cyl_3d(f, sx + 1, sy - 4, 4, 5, COL_GRAY_DARK, COL_ROCK_DARK, COL_BLACK);
            /* Tilted parabolic satellite dish with subtle scan pan */
            float pan = sinf(time_sec * 1.2f + s->x * 0.01f) * 0.55f;
            int dx = sx + 2 + (int)(sinf(pan) * 2.0f);
            int dy = sy - 11;
            /* Convex underside / back shell */
            gfx_circle(f, dx - 2, dy + 2, 10, COL_GRAY_DARK);
            /* Concave white dish face + shaded interior */
            gfx_circle(f, dx, dy, 10, COL_WHITE);
            gfx_circle(f, dx - 1, dy + 1, 7, COL_GRAY_LIGHT);
            gfx_circle_outline(f, dx, dy, 10, COL_BLACK);
            /* Feed-horn tripod struts & glowing receiver tip */
            int hx = dx + (int)(sinf(pan) * 5.0f) + 2;
            int hy = dy - 4;
            gfx_line(f, dx - 6, dy - 4, hx, hy, COL_GRAY_DARK);
            gfx_line(f, dx + 6, dy - 4, hx, hy, COL_GRAY_DARK);
            gfx_line(f, dx,     dy + 7, hx, hy, COL_GRAY_DARK);
            gfx_circle(f, hx, hy, 2, COL_BLACK);
            gfx_pixel(f, hx, hy, (((int)(time_sec * 4.0f)) & 1) ? COL_CYAN : COL_GREEN_BRIGHT);
            break;
        }
        case STRUCT_OIL_FIELD: {
            int count = s->size + 1; /* 1, 2, or 3 large round oil storage tanks */
            int pw = (count == 1) ? 32 : 56;
            int ph = (count <= 2) ? 32 : 52;
            gfx_rect(f, sx - pw / 2, sy - ph / 2, pw, ph, COL_ROCK_DARK);
            gfx_rect_outline(f, sx - pw / 2, sy - ph / 2, pw, ph, COL_BROWN_DARK);

            int tx[3], ty[3];
            if (count == 1) {
                tx[0] = sx; ty[0] = sy;
            } else if (count == 2) {
                tx[0] = sx - 13; ty[0] = sy;
                tx[1] = sx + 13; ty[1] = sy;
                gfx_line(f, tx[0], ty[0], tx[1], ty[1], COL_ORANGE);
            } else {
                tx[0] = sx - 13; ty[0] = sy - 10;
                tx[1] = sx + 13; ty[1] = sy - 10;
                tx[2] = sx;      ty[2] = sy + 11;
                gfx_line(f, tx[0], ty[0], tx[1], ty[1], COL_ORANGE);
                gfx_line(f, tx[0], ty[0], tx[2], ty[2], COL_ORANGE);
                gfx_line(f, tx[1], ty[1], tx[2], ty[2], COL_ORANGE);
            }

            for (int t = 0; t < count; t++) {
                gfx_cyl_3d(f, tx[t] + 2, ty[t] - 3, 11, 8, COL_GRAY_LIGHT, COL_GRAY_DARK, COL_BLACK);
                gfx_circle_outline(f, tx[t] + 2, ty[t] - 3, 8, COL_GRAY_MID);
                gfx_circle(f, tx[t] + 2, ty[t] - 3, 3, COL_RED_DARK);
                gfx_pixel(f, tx[t] + 2, ty[t] - 3, COL_ORANGE);
            }
            break;
        }
        case STRUCT_BUNGALOW: {
            int damaged = (s->hp < s->max_hp);
            int heavy = (s->hp * 2 <= s->max_hp);
            uint32_t vseed = village_hash((uint32_t)((int)s->x * 73856093) ^ (uint32_t)((int)s->y * 19349663) ^ (uint32_t)s->size);
            int is_resort = (s->size >= 3);

            /* Detect nearby coastline direction (average vector toward water within 130px) */
            float water_dx = 0.0f, water_dy = 0.0f;
            int water_hits = 0;
            if (w) {
                for (int a = 0; a < 12; a++) {
                    float ang = (float)a * (6.283185f / 12.0f);
                    float ca = cosf(ang), sa = sinf(ang);
                    for (int r = 40; r <= 120; r += 40) {
                        if (!world_is_land(w, (int)(s->x + ca * (float)r), (int)(s->y + sa * (float)r))) {
                            float wt = (r == 40) ? 1.5f : (r == 80 ? 1.0f : 0.6f);
                            water_dx += ca * wt;
                            water_dy += sa * wt;
                            water_hits++;
                            break;
                        }
                    }
                }
            }

            typedef struct { int x, y, type; uint32_t sub; } VBlock;
            VBlock blocks[24];
            int nb = 0;

            static const int hamlet_pool[8]  = {0, 1, 2, 5, 6, 8, 11, 13};
            static const int village_pool[13] = {0, 1, 2, 3, 4, 5, 6, 8, 9, 10, 11, 12, 13};
            static const int resort_pool[11]  = {4, 7, 5, 3, 6, 1, 7, 8, 11, 12, 13};

            float wlen = sqrtf(water_dx * water_dx + water_dy * water_dy);
            if (w && water_hits >= 2 && wlen > 0.2f) {
                /* Coastal village: hug the curved shoreline and extend 2-6 tiles inland */
                float ix = -water_dx / wlen, iy = -water_dy / wlen;
                float tx = -iy, ty = ix;
                int u_span = (s->size == 0) ? 1 : (s->size == 1 ? 2 : 3);
                int max_depth = (s->size == 0) ? 2 : (s->size == 1 ? 3 : 5);
                for (int u = -u_span; u <= u_span && nb < 24; u++) {
                    uint32_t col_h = village_hash(vseed + (uint32_t)(u + 10) * 193U);
                    /* Taper inland depth toward the ends so the village tapers naturally */
                    int edge_trim = (u < 0 ? -u : u);
                    int col_depth = max_depth - (edge_trim > 1 ? 1 : 0) - (int)(col_h % 2U);
                    if (col_depth < 1) col_depth = 1;
                    /* Find shoreline along this column starting from the water side */
                    float start_x = s->x + tx * ((float)u * 32.0f) - ix * 48.0f;
                    float start_y = s->y + ty * ((float)u * 32.0f) - iy * 48.0f;
                    float shore_x = 0.0f, shore_y = 0.0f;
                    int found_shore = 0;
                    for (int step = 0; step < 12; step++) {
                        float px = start_x + ix * ((float)step * 14.0f);
                        float py = start_y + iy * ((float)step * 14.0f);
                        if (village_spot_on_land(w, px, py)) {
                            shore_x = px;
                            shore_y = py;
                            found_shore = 1;
                            break;
                        }
                    }
                    if (!found_shore) continue;
                    for (int d = 0; d < col_depth && nb < 24; d++) {
                        uint32_t h = village_hash(col_h + (uint32_t)d * 131U);
                        int jx = (int)(h % 11U) - 5;
                        int jy = (int)((h >> 4) % 11U) - 5;
                        float wx = shore_x + ix * ((float)d * 29.0f) + (float)jx;
                        float wy = shore_y + iy * ((float)d * 29.0f) + (float)jy;
                        if (!village_spot_on_land(w, wx, wy)) break;
                        int btype = (s->size == 0) ? hamlet_pool[h % 8U]
                                  : (is_resort ? resort_pool[h % 11U] : village_pool[h % 13U]);
                        blocks[nb++] = (VBlock){(int)(wx - cam_x), (int)(wy - cam_y), btype, h >> 8};
                    }
                }
            }

            if (nb == 0) {
                /* Inland (or gallery) village: organic, roughly roundish cluster */
                int n_inner = (s->size == 0) ? 2 : (s->size == 1 ? 4 : 5);
                int n_outer = (s->size >= 2) ? (4 + (int)(vseed % 3U)) : 0;
                if (s->size >= 1) {
                    uint32_t hc = village_hash(vseed + 17U);
                    int b0 = is_resort ? resort_pool[hc % 11U] : ((hc & 1U) ? 3 : village_pool[hc % 13U]);
                    blocks[nb++] = (VBlock){sx + (int)(hc % 7U) - 3, sy + (int)((hc >> 3) % 7U) - 3, b0, hc >> 6};
                }
                float base_ang = (float)(vseed % 360U) * 0.017453f;
                for (int i = 0; i < n_inner && nb < 24; i++) {
                    uint32_t h = village_hash(vseed + (uint32_t)(i + 1) * 97U);
                    float ang = base_ang + (float)i * (6.283185f / (float)n_inner) + ((float)(h % 40U) - 20.0f) * 0.014f;
                    float rad = (s->size == 0 ? 22.0f : 33.0f) + (float)(h % 11U) - 5.0f;
                    float wx = s->x + cosf(ang) * rad;
                    float wy = s->y + sinf(ang) * rad * 0.86f;
                    if (!village_spot_on_land(w, wx, wy)) continue;
                    int btype = (s->size == 0) ? hamlet_pool[h % 8U]
                              : (is_resort ? resort_pool[h % 11U] : village_pool[h % 13U]);
                    blocks[nb++] = (VBlock){(int)(wx - cam_x), (int)(wy - cam_y), btype, h >> 8};
                }
                for (int i = 0; i < n_outer && nb < 24; i++) {
                    uint32_t h = village_hash(vseed + (uint32_t)(i + 20) * 151U);
                    if ((h % 7U) == 0U) continue; /* Keep perimeter organically irregular */
                    float ang = base_ang + 0.4f + (float)i * (6.283185f / (float)n_outer) + ((float)(h % 36U) - 18.0f) * 0.015f;
                    float rad = 60.0f + (float)(h % 17U) - 6.0f;
                    float wx = s->x + cosf(ang) * rad;
                    float wy = s->y + sinf(ang) * rad * 0.85f;
                    if (!village_spot_on_land(w, wx, wy)) continue;
                    int btype = is_resort ? resort_pool[h % 11U] : village_pool[h % 13U];
                    blocks[nb++] = (VBlock){(int)(wx - cam_x), (int)(wy - cam_y), btype, h >> 8};
                }
            }

            /* Draw little SimCity-style orthogonal road tiles between nearby blocks */
            for (int i = 0; i < nb; i++) {
                for (int j = i + 1; j < nb; j++) {
                    int dx = blocks[j].x - blocks[i].x;
                    int dy = blocks[j].y - blocks[i].y;
                    if (dx * dx + dy * dy <= 48 * 48) {
                        draw_village_road_segment(f, blocks[i].x, blocks[i].y + 4,
                                                  blocks[j].x, blocks[j].y + 4, is_resort);
                    }
                }
            }

            /* Sort back-to-front by Y so 2.5D roofs overlap cleanly */
            for (int i = 0; i < nb - 1; i++) {
                for (int j = i + 1; j < nb; j++) {
                    if (blocks[j].y < blocks[i].y) {
                        VBlock tmp = blocks[i];
                        blocks[i] = blocks[j];
                        blocks[j] = tmp;
                    }
                }
            }
            int scorch_r2 = heavy ? (30 * 30) : (22 * 22);
            for (int i = 0; i < nb; i++) {
                int block_damaged = 0;
                if (damaged) {
                    if (s->num_hits > 0) {
                        for (int k = 0; k < s->num_hits && k < 6; k++) {
                            int hx = (int)s->hit_x[k] - cam_x;
                            int hy = (int)s->hit_y[k] - cam_y;
                            int dx = blocks[i].x - hx;
                            int dy = blocks[i].y - hy;
                            if (dx * dx + dy * dy <= scorch_r2) {
                                block_damaged = 1;
                                break;
                            }
                        }
                    } else if (i == 0) {
                        block_damaged = 1;
                    }
                }
                draw_village_block(f, blocks[i].x, blocks[i].y, blocks[i].type, blocks[i].sub, block_damaged);
            }

            if (damaged) {
                int fl = ((int)(time_sec * 11.0f)) & 1;
                if (s->num_hits > 0) {
                    for (int k = 0; k < s->num_hits && k < 6; k++) {
                        int hx = (int)s->hit_x[k] - cam_x;
                        int hy = (int)s->hit_y[k] - cam_y;
                        gfx_circle(f, hx, hy, 4, COL_BLACK);
                        gfx_circle(f, hx, hy - 1, (fl ^ (k & 1)) ? 3 : 2, (fl ^ (k & 1)) ? COL_ORANGE : COL_RED);
                    }
                } else if (nb > 0) {
                    gfx_circle(f, blocks[0].x, blocks[0].y - 2, fl ? 4 : 3, fl ? COL_ORANGE : COL_RED);
                }
            }
            break;
        }
        case STRUCT_TOWER: {
            int damaged = (s->hp < s->max_hp);
            int heavy = (s->hp * 2 <= s->max_hp);
            if (s->size == 0) {
                /* Variant 0: Classic white concrete apartment tower with rooftop pool */
                int tw = 24, td = 14, th = 18;
                int rx = sx - tw / 2 + 4, ry = sy - td / 2 - th / 2;
                gfx_box_3d(f, rx, ry, tw, td, th,
                           damaged ? COL_GRAY_MID : COL_GRAY_LIGHT,
                           damaged ? COL_GRAY_DARK : COL_WHITE,
                           damaged ? COL_BLACK : COL_GRAY_MID, COL_BLACK);
                gfx_windows_3d(f, rx, ry, tw, td, th, 4, 4,
                               damaged ? COL_BLACK : COL_CYAN,
                               damaged ? COL_BLACK : COL_SHALLOW_SEA);
                gfx_rect_outline(f, rx + 2, ry + 2, tw - 4, td - 4, COL_GRAY_MID);
                gfx_rect(f, rx + 5, ry + 4, tw - 10, td - 8, damaged ? COL_ROCK_DARK : COL_SHALLOW_SEA);
            } else if (s->size == 1) {
                /* Variant 1: Terracotta / brick condo high-rise with warm windows & penthouse */
                int tw = 28, td = 16, th = 20;
                int rx = sx - tw / 2 + 4, ry = sy - td / 2 - th / 2;
                gfx_box_3d(f, rx, ry, tw, td, th,
                           damaged ? COL_BROWN_DARK : COL_SAND_LIGHT,
                           damaged ? COL_GRAY_DARK : COL_BROWN,
                           damaged ? COL_BLACK : COL_BROWN_DARK, COL_BLACK);
                gfx_windows_3d(f, rx, ry, tw, td, th, 4, 5,
                               damaged ? COL_BLACK : COL_YELLOW,
                               damaged ? COL_BLACK : COL_ORANGE);
                gfx_box_3d(f, rx + 8, ry + 3, 12, 8, 5,
                           damaged ? COL_GRAY_DARK : COL_RED_DARK,
                           damaged ? COL_ROCK_DARK : COL_WHITE,
                           damaged ? COL_BLACK : COL_GRAY_MID, COL_BLACK);
            } else if (s->size == 2) {
                /* Variant 2: Twin-tier stepped skyscraper (slate podium + upper glass tower) */
                int rx1 = sx - 12, ry1 = sy - 14;
                gfx_box_3d(f, rx1, ry1, 32, 18, 12,
                           damaged ? COL_GRAY_DARK : COL_GRAY_MID,
                           damaged ? COL_ROCK_DARK : COL_GRAY_DARK,
                           damaged ? COL_BLACK : COL_ROCK_DARK, COL_BLACK);
                gfx_windows_3d(f, rx1, ry1, 32, 18, 12, 5, 3,
                               damaged ? COL_BLACK : COL_CYAN,
                               damaged ? COL_BLACK : COL_SHALLOW_SEA);
                int rx2 = rx1 + 7, ry2 = ry1 - 12;
                gfx_box_3d(f, rx2, ry2, 20, 12, 16,
                           damaged ? COL_GRAY_MID : COL_WHITE,
                           damaged ? COL_GRAY_DARK : web_rgb(1, 3, 4),
                           damaged ? COL_BLACK : web_rgb(0, 2, 3), COL_BLACK);
                gfx_windows_3d(f, rx2, ry2, 20, 12, 16, 4, 4,
                               damaged ? COL_BLACK : COL_CYAN,
                               damaged ? COL_BLACK : COL_SHALLOW_SEA);
                gfx_rect_outline(f, rx2 + 5, ry2 + 3, 10, 6, COL_YELLOW);
            } else {
                /* Variant 3: Tall teal glass resort high-rise with white crown */
                int tw = 24, td = 16, th = 25;
                int rx = sx - tw / 2 + 5, ry = sy - td / 2 - th / 2;
                gfx_box_3d(f, rx, ry, tw, td, th,
                           damaged ? COL_GRAY_MID : COL_WHITE,
                           damaged ? COL_GRAY_DARK : web_rgb(0, 3, 4),
                           damaged ? COL_BLACK : web_rgb(0, 2, 3), COL_BLACK);
                gfx_windows_3d(f, rx, ry, tw, td, th, 4, 6,
                               damaged ? COL_BLACK : COL_CYAN,
                               damaged ? COL_BLACK : COL_SHALLOW_SEA);
                gfx_box_3d(f, rx + 4, ry + 3, 6, 5, 3, COL_GRAY_LIGHT, COL_GRAY_MID, COL_GRAY_DARK, COL_BLACK);
                gfx_box_3d(f, rx + 14, ry + 3, 6, 5, 3, COL_GRAY_LIGHT, COL_GRAY_MID, COL_GRAY_DARK, COL_BLACK);
            }
            if (damaged) {
                int fl = ((int)(time_sec * 10.0f)) & 1;
                gfx_circle(f, sx - 2, sy - 4, fl ? 4 : 3, fl ? COL_ORANGE : COL_RED);
                if (heavy) {
                    gfx_circle(f, sx + 5, sy + 2, fl ? 3 : 4, COL_YELLOW);
                }
            }
            break;
        }
        case STRUCT_NUCLEAR: {
            /* SimCity-style 2.5D nuclear power plant (64x64 compound) */
            gfx_rect(f, sx - 32, sy - 32, 64, 64, COL_ROCK_DARK);
            gfx_rect_outline(f, sx - 32, sy - 32, 64, 64, COL_YELLOW);
            gfx_rect(f, sx - 28, sy - 28, 56, 56, COL_GRAY_MID);

            /* 2.5D Turbine hall (bottom-left) */
            gfx_box_3d(f, sx - 24, sy + 2, 24, 14, 9, COL_GRAY_MID, COL_GRAY_DARK, COL_ROCK_DARK, COL_BLACK);

            /* 2.5D Domed containment reactor (top-left) */
            gfx_cyl_3d(f, sx - 12, sy - 16, 12, 9, COL_GRAY_LIGHT, COL_GRAY_DARK, COL_BLACK);
            gfx_circle(f, sx - 12, sy - 16, 5, COL_CYAN);
            gfx_circle(f, sx - 12, sy - 16, 2, COL_YELLOW);

            /* Two iconic 2.5D cooling towers (right side) */
            int cty[2] = {sy - 18, sy + 10};
            for (int t = 0; t < 2; t++) {
                gfx_cyl_3d(f, sx + 17, cty[t], 12, 12, COL_WHITE, COL_GRAY_MID, COL_BLACK);
                gfx_circle(f, sx + 17, cty[t], 7, COL_GRAY_DARK);
                gfx_circle(f, sx + 17, cty[t], 4, COL_BLACK);
                /* Subtle steam puff */
                int puff = (int)(sinf(time_sec * 3.0f + (float)t * 2.0f) * 2.0f);
                gfx_circle(f, sx + 17 + puff, cty[t] - 2, 3, COL_GRAY_LIGHT);
            }
            break;
        }
    }

    /* Health bar if damaged (omitted for civilian targets) */
    if (s->hp < s->max_hp && s->type != STRUCT_BUNGALOW && s->type != STRUCT_TOWER) {
        int bw = (s->type == STRUCT_FACTORY) ? (40 + s->size * 24) : 32;
        int by = (s->type == STRUCT_FACTORY) ? (sy - (24 + s->size * 14)) : (sy - 28);
        int fill = (s->hp * bw) / s->max_hp;
        gfx_rect(f, sx - bw / 2, by, bw, 4, COL_BLACK);
        gfx_rect(f, sx - bw / 2, by, fill, 4, COL_RED);
    }
}


static void heli_pt(int sx, int sy, float c, float s, float fwd, float right, int *ox, int *oy) {
    *ox = sx + (int)(c * fwd - s * right);
    *oy = sy + (int)(s * fwd + c * right);
}

static void heli_quad(Framebuffer *f, int sx, int sy, float c, float s,
                      float f0, float r0, float f1, float r1,
                      float f2, float r2, float f3, float r3, uint8_t col) {
    int x0, y0, x1, y1, x2, y2, x3, y3;
    heli_pt(sx, sy, c, s, f0, r0, &x0, &y0);
    heli_pt(sx, sy, c, s, f1, r1, &x1, &y1);
    heli_pt(sx, sy, c, s, f2, r2, &x2, &y2);
    heli_pt(sx, sy, c, s, f3, r3, &x3, &y3);
    gfx_triangle(f, x0, y0, x1, y1, x2, y2, col);
    gfx_triangle(f, x0, y0, x2, y2, x3, y3, col);
}

static void draw_enemy(const EnemyUnit *e, Framebuffer *f, int cam_x, int cam_y) {
    int sx = (int)(e->x - cam_x);
    int sy = (int)(e->y - cam_y);
    if (sx < -140 || sx > SCREEN_W + 140 || sy < -140 || sy > SCREEN_H + 140) return;

    float c = cosf(e->angle);
    float s = sinf(e->angle);

    switch (e->type) {
        case ENEMY_FIGHTER: {
            /* Shadow */
            gfx_circle(f, sx + 12, sy + 16, 9, COL_BLACK);
            /* Delta wing fighter (military grey) */
            int nx = sx + (int)(c * 25.0f);
            int ny = sy + (int)(s * 25.0f);
            int lx = sx + (int)(-c * 16.0f - s * 19.0f);
            int ly = sy + (int)(-s * 16.0f + c * 19.0f);
            int rx = sx + (int)(-c * 16.0f + s * 19.0f);
            int ry = sy + (int)(-s * 16.0f - c * 19.0f);
            gfx_triangle(f, nx, ny, lx, ly, rx, ry, COL_GRAY_MID);
            gfx_line(f, nx, ny, lx, ly, COL_GRAY_LIGHT);
            gfx_line(f, nx, ny, rx, ry, COL_GRAY_LIGHT);
            gfx_line(f, nx, ny, sx - (int)(c * 16.0f), sy - (int)(s * 16.0f), COL_GRAY_DARK);
            gfx_circle(f, sx + (int)(c * 7.0f), sy + (int)(s * 7.0f), 3, web_rgb(1, 2, 3));
            break;
        }
        case ENEMY_BOMBER: {
            gfx_circle(f, sx + 14, sy + 18, 12, COL_BLACK);
            int nx = sx + (int)(c * 27.0f);
            int ny = sy + (int)(s * 27.0f);
            int lx = sx + (int)(-c * 14.0f - s * 29.0f);
            int ly = sy + (int)(-s * 14.0f + c * 29.0f);
            int rx = sx + (int)(-c * 14.0f + s * 29.0f);
            int ry = sy + (int)(-s * 14.0f - c * 29.0f);
            gfx_triangle(f, nx, ny, lx, ly, rx, ry, COL_GRAY_DARK);
            gfx_line(f, nx, ny, lx, ly, COL_GRAY_MID);
            gfx_line(f, nx, ny, rx, ry, COL_GRAY_MID);
            gfx_circle(f, sx, sy, 7, COL_GRAY_MID);
            gfx_circle(f, sx + (int)(c * 9.0f), sy + (int)(s * 9.0f), 4, web_rgb(1, 2, 3));
            /* Engine nacelles */
            gfx_circle(f, sx + (int)(-c * 5.0f - s * 12.0f), sy + (int)(-s * 5.0f + c * 12.0f), 3, COL_GRAY_LIGHT);
            gfx_circle(f, sx + (int)(-c * 5.0f + s * 12.0f), sy + (int)(-s * 5.0f - c * 12.0f), 3, COL_GRAY_LIGHT);
            break;
        }
        case ENEMY_BOAT: {
            /* Patrol gunboat with pointed bow, wake, deck, wheelhouse, and bow turret */
            int w0x, w0y, w1x, w1y, w2x, w2y, w3x, w3y;
            heli_pt(sx, sy, c, s, -14.0f, -3.0f, &w0x, &w0y);
            heli_pt(sx, sy, c, s, -24.0f, -9.0f, &w1x, &w1y);
            heli_pt(sx, sy, c, s, -14.0f,  3.0f, &w2x, &w2y);
            heli_pt(sx, sy, c, s, -24.0f,  9.0f, &w3x, &w3y);
            gfx_line(f, w0x, w0y, w1x, w1y, COL_FOAM);
            gfx_line(f, w2x, w2y, w3x, w3y, COL_FOAM);
            int stx, sty;
            heli_pt(sx, sy, c, s, -17.0f, 0.0f, &stx, &sty);
            gfx_circle(f, stx, sty, 2, COL_FOAM);

            /* Naval hull (stern to midship + pointed bow) */
            int bx, by, px, py, sx1, sy1;
            heli_pt(sx, sy, c, s, 18.0f,  0.0f, &bx, &by);
            heli_pt(sx, sy, c, s,  6.0f, -7.0f, &px, &py);
            heli_pt(sx, sy, c, s,  6.0f,  7.0f, &sx1, &sy1);
            gfx_triangle(f, bx, by, px, py, sx1, sy1, COL_ROCK_DARK);
            heli_quad(f, sx, sy, c, s, 6.0f, -7.0f, 6.0f, 7.0f, -15.0f, 5.0f, -15.0f, -5.0f, COL_ROCK_DARK);

            /* Inner gray deck */
            int dbx, dby, dpx, dpy, dsx, dsy;
            heli_pt(sx, sy, c, s, 14.0f,  0.0f, &dbx, &dby);
            heli_pt(sx, sy, c, s,  5.0f, -5.0f, &dpx, &dpy);
            heli_pt(sx, sy, c, s,  5.0f,  5.0f, &dsx, &dsy);
            gfx_triangle(f, dbx, dby, dpx, dpy, dsx, dsy, COL_GRAY_MID);
            heli_quad(f, sx, sy, c, s, 5.0f, -5.0f, 5.0f, 5.0f, -13.0f, 3.5f, -13.0f, -3.5f, COL_GRAY_MID);

            /* Crisp gunwale outline */
            int slx, sly, srx, sry;
            heli_pt(sx, sy, c, s, -15.0f, -5.0f, &slx, &sly);
            heli_pt(sx, sy, c, s, -15.0f,  5.0f, &srx, &sry);
            gfx_line(f, bx, by, px, py, COL_GRAY_LIGHT);
            gfx_line(f, bx, by, sx1, sy1, COL_GRAY_LIGHT);
            gfx_line(f, px, py, slx, sly, COL_GRAY_LIGHT);
            gfx_line(f, sx1, sy1, srx, sry, COL_GRAY_LIGHT);
            gfx_line(f, slx, sly, srx, sry, COL_GRAY_LIGHT);

            /* Raised wheelhouse / bridge */
            heli_quad(f, sx, sy, c, s, 1.0f, -3.5f, 1.0f, 3.5f, -8.0f, 3.5f, -8.0f, -3.5f, COL_WHITE);
            heli_quad(f, sx, sy, c, s, 2.0f, -2.8f, 2.0f, 2.8f,  0.0f, 2.8f,  0.0f, -2.8f, COL_CYAN);

            /* Bow gun turret & forward barrel */
            int gx, gy, gbx, gby;
            heli_pt(sx, sy, c, s, 8.0f, 0.0f, &gx, &gy);
            heli_pt(sx, sy, c, s, 15.0f, 0.0f, &gbx, &gby);
            gfx_circle(f, gx, gy, 3, COL_RED_DARK);
            gfx_line(f, gx, gy, gbx, gby, COL_YELLOW);

            /* Stern flag / marker */
            int fx, fy;
            heli_pt(sx, sy, c, s, -11.0f, 0.0f, &fx, &fy);
            gfx_pixel(f, fx, fy, COL_RED);
            break;
        }
        case ENEMY_CARRIER: {
            /* Enemy flat-top carrier (scaled up) */
            gfx_rect(f, sx - 34, sy - 80, 68, 160, COL_RED_DARK);
            gfx_triangle(f, sx - 34, sy - 80, sx + 34, sy - 80, sx, sy - 108, COL_RED_DARK);
            gfx_rect(f, sx - 28, sy - 72, 56, 144, COL_GRAY_DARK);
            gfx_triangle(f, sx - 26, sy - 72, sx + 26, sy - 72, sx, sy - 98, COL_GRAY_DARK);
            gfx_rect_outline(f, sx - 28, sy - 72, 56, 144, COL_ORANGE);
            gfx_rect(f, sx - 30, sy - 24, 14, 42, COL_RED);
            for (int y = -60; y < 64; y += 14) {
                gfx_rect(f, sx - 2, sy + y, 4, 8, COL_YELLOW);
            }
            int bw = 64;
            int fill = (e->hp * bw) / e->max_hp;
            gfx_rect(f, sx - bw / 2, sy - 116, bw, 6, COL_BLACK);
            gfx_rect(f, sx - bw / 2 + 1, sy - 115, fill > 2 ? fill - 2 : 0, 4, COL_ORANGE);
            break;
        }
    }
}


static void draw_heli_at(Framebuffer *f, int sx, int sy, float angle, float rotor_phase) {
    float c = cosf(angle);
    float s = sinf(angle);

    /* Bell 222 / Airwolf ground shadow */
    int shx = sx + 13, shy = sy + 17;
    heli_quad(f, shx, shy, c, s, 17.0f, -4.0f, 17.0f, 4.0f, -12.0f, 6.0f, -12.0f, -6.0f, COL_BLACK);
    heli_quad(f, shx, shy, c, s, 2.0f, -13.0f, 2.0f, 13.0f, -8.0f, 13.0f, -8.0f, -13.0f, COL_BLACK);
    heli_quad(f, shx, shy, c, s, -12.0f, -2.5f, -12.0f, 2.5f, -35.0f, 1.0f, -35.0f, -1.0f, COL_BLACK);

    /* Horizontal tail stabilizers (near rear of tail boom) */
    heli_quad(f, sx, sy, c, s, -27.0f, -8.0f, -27.0f, 8.0f, -32.0f, 7.0f, -32.0f, -7.0f, COL_GRAY_MID);

    /* Long, straight, slender tail boom (40-45% of length) */
    heli_quad(f, sx, sy, c, s, -11.0f, -3.0f, -11.0f, 3.0f, -36.0f, 1.5f, -36.0f, -1.5f, COL_GRAY_DARK);
    /* Tail fin & port-side tail rotor */
    int tx0, ty0, tx1, ty1;
    heli_pt(sx, sy, c, s, -35.0f, -1.5f, &tx0, &ty0);
    heli_pt(sx, sy, c, s, -35.0f, -6.5f, &tx1, &ty1);
    gfx_line(f, tx0, ty0, tx1, ty1, COL_GRAY_LIGHT);

    /* Defining Bell 222 / Airwolf amidships sponsons (wide, nearly straight with slight sweepback) */
    heli_quad(f, sx, sy, c, s,  3.0f, -14.0f,  3.0f,  14.0f, -8.0f,  13.0f, -8.0f, -13.0f, COL_GRAY_LIGHT);
    heli_quad(f, sx, sy, c, s,  1.5f, -13.0f,  1.5f,  13.0f, -6.5f,  12.0f, -6.5f, -12.0f, COL_GRAY_DARK);
    /* Sponson intake / weapon pod tips */
    heli_quad(f, sx, sy, c, s,  3.5f, -14.5f,  3.5f, -11.5f, -8.5f, -11.5f, -8.5f, -14.5f, COL_GRAY_MID);
    heli_quad(f, sx, sy, c, s,  3.5f,  11.5f,  3.5f,  14.5f, -8.5f,  14.5f, -8.5f,  11.5f, COL_GRAY_MID);

    /* Main fuselage: rounded nosecone -> wide cabin -> wasp-waist taper */
    heli_quad(f, sx, sy, c, s, 18.0f, -3.0f, 18.0f,  3.0f, 12.0f,  6.0f, 12.0f, -6.0f, COL_GRAY_LIGHT);
    heli_quad(f, sx, sy, c, s, 12.0f, -6.0f, 12.0f,  6.0f, -8.0f,  6.0f, -8.0f, -6.0f, COL_GRAY_LIGHT);
    heli_quad(f, sx, sy, c, s, -8.0f, -6.0f, -8.0f,  6.0f, -13.0f, 3.0f, -13.0f, -3.0f, COL_GRAY_LIGHT);

    /* Glossy jet-black / charcoal upper hull */
    heli_quad(f, sx, sy, c, s, 17.0f, -2.5f, 17.0f,  2.5f, 11.5f,  5.0f, 11.5f, -5.0f, COL_BLACK);
    heli_quad(f, sx, sy, c, s, 11.5f, -5.0f, 11.5f,  5.0f, -8.0f,  5.0f, -8.0f, -5.0f, COL_BLACK);
    heli_quad(f, sx, sy, c, s, -8.0f, -5.0f, -8.0f,  5.0f, -12.5f, 2.5f, -12.5f, -2.5f, COL_BLACK);

    /* Twin centerline turboshaft engine fairing spine */
    heli_quad(f, sx, sy, c, s,  3.0f, -3.0f,  3.0f,  3.0f, -12.0f,  2.5f, -12.0f, -2.5f, COL_GRAY_DARK);

    /* Smoked tinted cockpit windshield with subtle glint & center strut */
    uint8_t glass_dark = web_rgb(1, 2, 3);  /* #336699 smoked steel-blue */
    uint8_t glass_glint = web_rgb(2, 3, 4); /* #6699CC subtle sky reflection */
    heli_quad(f, sx, sy, c, s, 14.5f, -3.0f, 14.5f,  3.0f,  8.0f,  4.5f,  8.0f, -4.5f, glass_dark);
    heli_quad(f, sx, sy, c, s, 14.0f, -2.5f, 14.0f, -0.5f, 10.0f, -1.0f, 10.0f, -3.5f, glass_glint);
    int cx0, cy0, cx1, cy1;
    heli_pt(sx, sy, c, s, 14.5f, 0.0f, &cx0, &cy0);
    heli_pt(sx, sy, c, s,  8.0f, 0.0f, &cx1, &cy1);
    gfx_line(f, cx0, cy0, cx1, cy1, COL_BLACK);

    /* Spinning main rotor (rotor diameter slightly wider than fuselage) */
    for (int b = 0; b < 2; b++) {
        float ra = rotor_phase + (float)b * ((float)M_PI * 0.5f);
        int rx = (int)(cosf(ra) * 31.0f);
        int ry = (int)(sinf(ra) * 31.0f);
        gfx_line(f, sx - rx, sy - ry, sx + rx, sy + ry, COL_WHITE);
    }
    gfx_rect(f, sx - 1, sy - 1, 3, 3, COL_RED);
}

static void draw_player_heli(const Game *g, Framebuffer *f, int cam_x, int cam_y) {
    const PlayerHeli *p = &g->player;
    if (p->invuln_timer > 0.0f && ((int)(p->invuln_timer * 16.0f) & 1)) {
        return; /* Blink when invulnerable */
    }
    draw_heli_at(f, (int)(p->x - cam_x), (int)(p->y - cam_y), p->angle, p->rotor_phase);
}

static void draw_carrier_arrow(const Game *g, Framebuffer *f) {
    int cx = (int)(g->carrier.x - g->cam_x);
    int cy = (int)(g->carrier.y - g->cam_y);
    if (cx >= 20 && cx <= SCREEN_W - 20 && cy >= 40 && cy <= SCREEN_H - 20) {
        return; /* Carrier is visible on screen */
    }

    float dx = g->carrier.x - g->player.x;
    float dy = g->carrier.y - g->player.y;
    float dist = sqrtf(dx * dx + dy * dy);
    float ang = atan2f(dy, dx);

    /* Clamp arrow onto a margin box inside the screen */
    float mx = SCREEN_W * 0.5f + cosf(ang) * (SCREEN_W * 0.44f);
    float my = SCREEN_H * 0.5f + sinf(ang) * (SCREEN_H * 0.41f);
    if (mx < 36.0f) mx = 36.0f;
    if (mx > SCREEN_W - 36.0f) mx = SCREEN_W - 36.0f;
    if (my < 64.0f) my = 64.0f;
    if (my > SCREEN_H - 36.0f) my = SCREEN_H - 36.0f;

    int ax = (int)mx;
    int ay = (int)my;
    float ca = cosf(ang), sa = sinf(ang);
    int tip_x = ax + (int)(ca * 14.0f);
    int tip_y = ay + (int)(sa * 14.0f);
    int l_x = ax + (int)(-ca * 8.0f - sa * 9.0f);
    int l_y = ay + (int)(-sa * 8.0f + ca * 9.0f);
    int r_x = ax + (int)(-ca * 8.0f + sa * 9.0f);
    int r_y = ay + (int)(-sa * 8.0f - ca * 9.0f);

    gfx_triangle(f, tip_x, tip_y, l_x, l_y, r_x, r_y, COL_CYAN);
    gfx_line(f, tip_x, tip_y, l_x, l_y, COL_WHITE);
    gfx_line(f, tip_x, tip_y, r_x, r_y, COL_WHITE);

    gfx_text(f, ax - 20, ay + 16, 1, COL_CYAN, "CV %dm", (int)(dist * 0.5f));
}

static void draw_minimap(const Game *g, Framebuffer *f) {
    const int mw = 110;
    const int mh = 170;
    const int mx = SCREEN_W - mw - 14;
    const int my = 48;

    gfx_rect(f, mx - 2, my - 2, mw + 4, mh + 4, COL_BLACK);
    gfx_rect_outline(f, mx - 2, my - 2, mw + 4, mh + 4, COL_GRAY_LIGHT);

    /* Sample terrain */
    for (int py = 0; py < mh; py++) {
        int ty = (py * WORLD_TILES_H) / mh;
        for (int px = 0; px < mw; px++) {
            int tx = (px * WORLD_TILES_W) / mw;
            TileType t = (TileType)g->world.tiles[ty][tx];
            uint8_t col = COL_DEEP_SEA;
            if (t == TILE_MID_SEA || t == TILE_SHALLOW_SEA) col = COL_MID_SEA;
            else if (t == TILE_SAND) col = COL_SAND_LIGHT;
            else if (t >= TILE_GRASS) col = COL_GRASS_MID;
            gfx_pixel(f, mx + px, my + py, col);
        }
    }

    /* Structures */
    for (int i = 0; i < MAX_STRUCTURES; i++) {
        const Structure *s = &g->structures[i];
        if (!s->active) continue;
        int sx = mx + (int)((s->x / (float)WORLD_W) * mw);
        int sy = my + (int)((s->y / (float)WORLD_H) * mh);
        if (s->type == STRUCT_BUNGALOW || s->type == STRUCT_TOWER) {
            gfx_rect(f, sx, sy, 2, 2, COL_GRAY_LIGHT);
            continue;
        }
        uint8_t col = COL_ORANGE;
        if (s->type == STRUCT_FACTORY) {
            col = COL_RED;
            if (s->size == 2) {
                gfx_rect(f, sx - 2, sy - 2, 4, 4, col);
                continue;
            }
        } else if (s->type == STRUCT_SHIPYARD) col = COL_YELLOW;
        else if (s->type == STRUCT_NUCLEAR) {
            gfx_rect(f, sx - 2, sy - 2, 4, 4, COL_GREEN_BRIGHT);
            gfx_pixel(f, sx, sy, COL_YELLOW);
            continue;
        }
        gfx_rect(f, sx - 1, sy - 1, 3, 3, col);
    }

    /* Enemy carrier */
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (g->enemies[i].active && g->enemies[i].type == ENEMY_CARRIER) {
            int ex = mx + (int)((g->enemies[i].x / (float)WORLD_W) * mw);
            int ey = my + (int)((g->enemies[i].y / (float)WORLD_H) * mh);
            gfx_rect(f, ex - 2, ey - 2, 4, 4, COL_RED);
        }
    }

    /* Friendly carrier */
    int cx = mx + (int)((g->carrier.x / (float)WORLD_W) * mw);
    int cy = my + (int)((g->carrier.y / (float)WORLD_H) * mh);
    gfx_rect(f, cx - 2, cy - 2, 5, 5, COL_CYAN);

    /* Player blip */
    int px = mx + (int)((g->player.x / (float)WORLD_W) * mw);
    int py = my + (int)((g->player.y / (float)WORLD_H) * mh);
    if (((int)(g->time_sec * 6.0f)) & 1) {
        gfx_rect(f, px - 1, py - 1, 3, 3, COL_WHITE);
    }
}

static void draw_hud(const Game *g, Framebuffer *f) {
    /* Top tactical bar */
    gfx_rect(f, 0, 0, SCREEN_W, 36, COL_BLACK);
    gfx_rect(f, 0, 36, SCREEN_W, 2, COL_GRAY_MID);

    /* HP */
    gfx_text(f, 12, 11, 2, COL_WHITE, "HP");
    gfx_rect(f, 44, 10, 110, 16, COL_GRAY_DARK);
    int hp_w = (g->player.hp * 106) / g->player.max_hp;
    gfx_rect(f, 46, 12, hp_w, 12, g->player.hp > 35 ? COL_GREEN_BRIGHT : COL_RED);

    /* Bombs */
    gfx_text(f, 174, 11, 2, COL_YELLOW, "BOMBS:%d/%d", g->player.bombs, MAX_BOMBS_CARRIED);

    /* Lives */
    gfx_text(f, 350, 11, 2, COL_CYAN, "HELI:%d", g->player.lives);

    /* Factories remaining */
    gfx_text(f, 468, 11, 2, COL_ORANGE, "FACTORIES:%d", g->factories_alive);

    /* Carrier HP */
    gfx_text(f, 675, 11, 2, COL_CYAN, "CV:%d%%", (g->carrier.hp * 100) / g->carrier.max_hp);

    /* Score */
    gfx_text(f, 825, 11, 2, COL_WHITE, "SCORE:%05d", g->score);

    /* Mute indicator */
    if (audio_is_muted()) {
        gfx_text(f, SCREEN_W - 110, SCREEN_H - 20, 1, COL_RED, "[MUTED - M]");
    }

    /* Status banner */
    if (g->status_timer > 0.0f && g->status_msg[0]) {
        uint8_t scol = g->status_is_alert ? COL_RED : COL_YELLOW;
        int tw = gfx_text_width(2, g->status_msg);
        int bx = (SCREEN_W - tw) / 2;
        gfx_rect(f, bx - 10, 48, tw + 20, 24, COL_BLACK);
        gfx_rect_outline(f, bx - 10, 48, tw + 20, 24, scol);
        gfx_text(f, bx, 53, 2, scol, "%s", g->status_msg);
    }

    /* Lower-left indicator of structure under helicopter */
    const char *over_label = NULL;
    if (fabsf(g->player.x - g->carrier.x) < 42.0f && fabsf(g->player.y - g->carrier.y) < 92.0f) {
        over_label = "FRIENDLY CARRIER";
    } else {
        for (int i = 0; i < MAX_STRUCTURES; i++) {
            const Structure *s = &g->structures[i];
            if (!s->active) continue;
            float hw = 22.0f, hh = 22.0f;
            if (s->type == STRUCT_FACTORY) {
                hw = (s->size == 0) ? 26.0f : (s->size == 1 ? 34.0f : 58.0f);
                hh = (s->size == 0) ? 18.0f : (s->size == 1 ? 26.0f : 42.0f);
            } else if (s->type == STRUCT_SHIPYARD) {
                hw = 28.0f; hh = 26.0f;
            } else if (s->type == STRUCT_OIL_FIELD) {
                hw = (s->size == 0) ? 18.0f : 30.0f;
                hh = (s->size <= 1) ? 18.0f : 28.0f;
            } else if (s->type == STRUCT_NUCLEAR) {
                hw = 34.0f; hh = 34.0f;
            } else if (s->type == STRUCT_BUNGALOW) {
                hw = (s->size == 2) ? 48.0f : 34.0f;
                hh = (s->size == 2) ? 36.0f : 26.0f;
            }
            if (fabsf(g->player.x - s->x) < hw && fabsf(g->player.y - s->y) < hh) {
                switch (s->type) {
                    case STRUCT_FACTORY:
                        over_label = (s->size == 2) ? "LARGE FACTORY COMPLEX" :
                                     (s->size == 1 ? "MEDIUM FACTORY" : "SMALL FACTORY");
                        break;
                    case STRUCT_SHIPYARD:
                        over_label = s->building_carrier ? "CARRIER SHIPYARD" : "ENEMY SHIPYARD";
                        break;
                    case STRUCT_AA:        over_label = "AA BATTERY"; break;
                    case STRUCT_BARRACKS:  over_label = "TROOP BARRACKS"; break;
                    case STRUCT_RADAR:     over_label = "RADAR / SAM SITE"; break;
                    case STRUCT_OIL_FIELD: over_label = "OIL STORAGE TANKS"; break;
                    case STRUCT_BUNGALOW:
                        over_label = (s->size == 2) ? "UPSCALE BEACH VILLAGE (CIVILIAN)"
                                                    : "COASTAL VILLAGE (CIVILIAN)";
                        break;
                    case STRUCT_TOWER:     over_label = "RESIDENTIAL TOWER (CIVILIAN)"; break;
                    case STRUCT_NUCLEAR:   over_label = "NUCLEAR REACTOR (DO NOT BOMB!)"; break;
                }
                break;
            }
        }
    }
    if (over_label) {
        int lw = gfx_text_width(1, over_label) + 40;
        gfx_rect(f, 10, SCREEN_H - 26, lw + 12, 16, COL_BLACK);
        gfx_rect_outline(f, 10, SCREEN_H - 26, lw + 12, 16, COL_GRAY_MID);
        gfx_text(f, 16, SCREEN_H - 21, 1, COL_WHITE, "OVER: %s", over_label);
    }

    draw_minimap(g, f);
    draw_carrier_arrow(g, f);
}

static void draw_title_screen(const Game *g, Framebuffer *f) {
    /* Draw lush island background behind title */
    world_render(&g->world, f, 900, 1200, g->time_sec);

    /* Dark overlay box */
    gfx_rect(f, 142, 90, 740, 588, COL_BLACK);
    gfx_rect_outline(f, 142, 90, 740, 588, COL_YELLOW);
    gfx_rect_outline(f, 146, 94, 732, 580, COL_ORANGE);

    const char *title = "BANANA BAY";
    int tw = gfx_text_width(6, title);
    gfx_text(f, (SCREEN_W - tw) / 2 + 3, 133, 6, COL_RED_DARK, "%s", title);
    gfx_text(f, (SCREEN_W - tw) / 2,     130, 6, COL_YELLOW, "%s", title);

    const char *sub = "OPERATION: TROPICAL THUNDER";
    gfx_text(f, (SCREEN_W - gfx_text_width(2, sub)) / 2, 195, 2, COL_CYAN, "%s", sub);

    /* Difficulty selector */
    gfx_text(f, 340, 260, 2, COL_WHITE, "SELECT DIFFICULTY:");
    static const char *diff_names[3] = {"1. EASY   (RELAXED)", "2. NORMAL (ARCADE)", "3. HARD   (VETERAN)"};
    for (int i = 0; i < 3; i++) {
        int sel = ((int)g->difficulty == i);
        uint8_t col = sel ? COL_YELLOW : COL_GRAY_LIGHT;
        if (sel) {
            gfx_rect(f, 330, 294 + i * 32, 364, 24, COL_BLUE_DARK);
            gfx_rect_outline(f, 330, 294 + i * 32, 364, 24, COL_YELLOW);
        }
        gfx_text(f, 348, 299 + i * 32, 2, col, "%s%s", sel ? "> " : "  ", diff_names[i]);
    }

    /* Controls summary */
    gfx_text(f, 210, 415, 2, COL_GREEN_BRIGHT, "CONTROLS:");
    gfx_text(f, 210, 445, 2, COL_GRAY_LIGHT, "MOVE:   WASD / ARROWS / LEFT STICK / D-PAD");
    gfx_text(f, 210, 472, 2, COL_GRAY_LIGHT, "GUN:    SPACE / Z / J / GAMEPAD A");
    gfx_text(f, 210, 499, 2, COL_GRAY_LIGHT, "BOMB:   X / K / SHIFT / GAMEPAD B or X");
    gfx_text(f, 190, 526, 2, COL_GRAY_LIGHT, "PAUSE: P/ESC  MUTE: M/ESC  GALLERY: F1  QUIT: Q");

    gfx_text(f, 210, 570, 2, COL_ORANGE, "DESTROY ALL ENEMY FACTORIES. PROTECT YOUR CARRIER!");
    gfx_text(f, 210, 595, 2, COL_CYAN,   "FLY OVER YOUR CARRIER TO REARM BOMBS & REPAIR.");

    if (((int)(g->time_sec * 3.0f)) & 1) {
        const char *prompt = "PRESS ENTER OR GAMEPAD START/A TO LAUNCH";
        gfx_text(f, (SCREEN_W - gfx_text_width(2, prompt)) / 2, 636, 2, COL_YELLOW, "%s", prompt);
    }
    gfx_text(f, 158, 660, 1, COL_GRAY_MID, "v%s", BANANA_BAY_VERSION);
}

static void draw_quit_prompt(Framebuffer *f) {
    gfx_rect(f, 272, 304, 480, 148, COL_BLACK);
    gfx_rect_outline(f, 272, 304, 480, 148, COL_RED);
    gfx_rect_outline(f, 276, 308, 472, 140, COL_YELLOW);

    const char *hdr = "QUIT BANANA BAY?";
    gfx_text(f, (SCREEN_W - gfx_text_width(3, hdr)) / 2, 332, 3, COL_YELLOW, "%s", hdr);

    const char *opt = "[Y / ENTER] YES    [N / ESC] NO";
    gfx_text(f, (SCREEN_W - gfx_text_width(2, opt)) / 2, 396, 2, COL_WHITE, "%s", opt);
}

static void draw_gallery_card(Framebuffer *f, int x, int y, int w, int h, const char *label) {
    gfx_rect(f, x, y, w, h, web_rgb(0, 0, 1));
    gfx_rect_outline(f, x, y, w, h, COL_GRAY_DARK);
    int lw = gfx_text_width(1, label);
    gfx_text(f, x + (w - lw) / 2, y + h - 12, 1, COL_GRAY_LIGHT, "%s", label);
}

static void draw_gallery_screen(const Game *g, Framebuffer *f) {
    gfx_clear(f, COL_BLACK);

    /* Header */
    gfx_rect(f, 0, 0, SCREEN_W, 38, web_rgb(0, 0, 2));
    gfx_rect(f, 0, 38, SCREEN_W, 2, COL_YELLOW);
    gfx_text(f, 16, 11, 2, COL_YELLOW, "BANANA BAY - SPRITES, STRUCTURES & TILES");
    gfx_text(f, SCREEN_W - 248, 13, 1, COL_CYAN, "PRESS F1 / ESC / ENTER TO RETURN");

    /* Section 1: Terrain Tiles */
    gfx_text(f, 16, 48, 1, COL_GREEN_BRIGHT, "TERRAIN TILES (16x16 BASE / PIXEL SWATCHES):");
    static const char *tile_names[9] = {
        "DEEP SEA", "MID SEA", "SHALLOWS", "BEACH SAND",
        "GRASS", "JUNGLE", "ROCK", "ASPHALT ROAD", "BRIDGE"
    };
    uint8_t tile_cols[9];
    tile_cols[0] = COL_DEEP_SEA;
    tile_cols[1] = COL_MID_SEA;
    tile_cols[2] = COL_SHALLOW_SEA;
    tile_cols[3] = COL_SAND_LIGHT;
    tile_cols[4] = COL_GRASS_LIGHT;
    tile_cols[5] = COL_GRASS_DARK;
    tile_cols[6] = COL_ROCK_LIGHT;
    tile_cols[7] = COL_GRAY_DARK;
    tile_cols[8] = COL_BROWN;

    for (int i = 0; i < 9; i++) {
        int x = 16 + i * 111;
        int y = 62;
        draw_gallery_card(f, x, y, 105, 56, tile_names[i]);
        int sx = x + 36, sy = y + 8;
        gfx_rect(f, sx, sy, 32, 24, tile_cols[i]);
        if (i == 2) {
            gfx_line(f, sx, sy + 20, sx + 31, sy + 20, COL_FOAM);
        } else if (i == 5) {
            gfx_rect(f, sx + 6, sy + 6, 6, 6, COL_JUNGLE_DARK);
            gfx_rect(f, sx + 18, sy + 12, 6, 6, COL_JUNGLE_DARK);
        } else if (i == 7) {
            gfx_rect(f, sx, sy, 32, 2, COL_ROCK_DARK);
            gfx_rect(f, sx, sy + 22, 32, 2, COL_ROCK_DARK);
            gfx_rect(f, sx + 4, sy + 11, 8, 2, COL_YELLOW);
            gfx_rect(f, sx + 20, sy + 11, 8, 2, COL_YELLOW);
        } else if (i == 8) {
            gfx_rect_outline(f, sx, sy, 32, 24, COL_BROWN_DARK);
            for (int px = 4; px < 32; px += 6) {
                gfx_line(f, sx + px, sy, sx + px, sy + 23, COL_BROWN_DARK);
            }
        }
    }

    /* Section 2: Aircraft & Naval Units */
    gfx_text(f, 16, 130, 1, COL_GREEN_BRIGHT, "AIRCRAFT & NAVAL UNITS:");
    draw_gallery_card(f, 16, 144, 175, 118, "AIRWOLF (PLAYER)");
    draw_heli_at(f, 104, 196, -(float)M_PI * 0.5f, g->time_sec * 18.0f);

    static const char *unit_names[3] = {"FIGHTER", "BOMBER", "PATROL BOAT"};
    static const EnemyType unit_types[3] = {ENEMY_FIGHTER, ENEMY_BOMBER, ENEMY_BOAT};
    for (int i = 0; i < 3; i++) {
        int x = 199 + i * 175;
        draw_gallery_card(f, x, 144, 167, 118, unit_names[i]);
        EnemyUnit u = {0};
        u.active = 1;
        u.type = unit_types[i];
        u.x = (float)(x + 83);
        u.y = 196.0f;
        u.angle = -0.55f;
        u.hp = 100;
        u.max_hp = 100;
        draw_enemy(&u, f, 0, 0);
    }

    /* Tall carrier cards on the right */
    draw_gallery_card(f, 728, 144, 136, 268, "FRIENDLY CV-1");
    {
        Game tmp = *g;
        tmp.carrier.x = 796.0f;
        tmp.carrier.y = 282.0f;
        tmp.carrier.hp = tmp.carrier.max_hp = 100;
        draw_carrier_friendly(&tmp, f, 0, 0);
    }
    draw_gallery_card(f, 872, 144, 136, 268, "ENEMY CARRIER");
    {
        EnemyUnit ec = {0};
        ec.active = 1;
        ec.type = ENEMY_CARRIER;
        ec.x = 940.0f;
        ec.y = 282.0f;
        ec.angle = 0.0f;
        ec.hp = ec.max_hp = 100;
        draw_enemy(&ec, f, 0, 0);
    }

    /* Section 3: Island Structures */
    gfx_text(f, 16, 274, 1, COL_GREEN_BRIGHT, "MILITARY & CIVILIAN STRUCTURES:");
    typedef struct {
        StructureType type;
        int size;
        const char *name;
    } StructCard;

    static const StructCard row1[5] = {
        {STRUCT_FACTORY, 0, "SMALL FACTORY"},
        {STRUCT_FACTORY, 1, "MEDIUM FACTORY"},
        {STRUCT_FACTORY, 2, "LARGE FACTORY COMPLEX"},
        {STRUCT_SHIPYARD, 0, "SHIPYARD (CV)"},
        {STRUCT_NUCLEAR, 0, "NUCLEAR REACTOR"}
    };
    int r1_w[5] = {120, 132, 184, 128, 128};
    int cx = 16;
    for (int i = 0; i < 5; i++) {
        int w = r1_w[i];
        draw_gallery_card(f, cx, 288, w, 124, row1[i].name);
        Structure st = {0};
        st.active = 1;
        st.type = row1[i].type;
        st.size = row1[i].size;
        st.x = (float)(cx + w / 2);
        st.y = 344.0f;
        st.hp = st.max_hp = 100;
        st.level = 1;
        st.build_progress = 0.5f;
        st.building_carrier = (row1[i].type == STRUCT_SHIPYARD);
        st.carrier_progress = 0.65f;
        st.carrier_launched = 0;
        draw_structure(&st, f, NULL, 0, 0, g->time_sec);
        cx += w + 6;
    }

    static const StructCard row2[7] = {
        {STRUCT_AA, 0, "AA BATTERY"},
        {STRUCT_BARRACKS, 0, "BARRACKS"},
        {STRUCT_RADAR, 0, "RADAR / SAM"},
        {STRUCT_OIL_FIELD, 2, "OIL TANKS (1-3)"},
        {STRUCT_BUNGALOW, 0, "COASTAL VILLAGE"},
        {STRUCT_BUNGALOW, 2, "LARGE VILLAGE"},
        {STRUCT_TOWER, 2, "STEPPED TOWER"}
    };
    for (int i = 0; i < 7; i++) {
        int x = 16 + i * 142;
        draw_gallery_card(f, x, 424, 136, 118, row2[i].name);
        Structure st = {0};
        st.active = 1;
        st.type = row2[i].type;
        st.size = row2[i].size;
        st.x = (float)(x + 68);
        st.y = 476.0f;
        st.hp = st.max_hp = 100;
        st.level = 1;
        draw_structure(&st, f, NULL, 0, 0, g->time_sec);
    }

    /* Section 4: Projectiles & FX */
    gfx_text(f, 16, 554, 1, COL_GREEN_BRIGHT, "PROJECTILES & EFFECTS:");
    draw_gallery_card(f, 16, 568, 160, 74, "CANNON TRACER");
    gfx_circle(f, 96, 598, 3, COL_YELLOW);

    draw_gallery_card(f, 184, 568, 160, 74, "FALLING BOMB");
    gfx_circle(f, 268, 608, 3, COL_BLACK);
    gfx_circle(f, 264, 594, 4, COL_YELLOW);
    gfx_circle(f, 264, 594, 2, COL_RED);

    draw_gallery_card(f, 352, 568, 160, 74, "EXPLOSION");
    gfx_circle(f, 432, 598, 10, COL_RED);
    gfx_circle(f, 432, 598, 6, COL_ORANGE);
    gfx_circle(f, 432, 598, 3, COL_YELLOW);

    draw_gallery_card(f, 520, 568, 160, 74, "DARK SMOKE");
    gfx_circle(f, 594, 604, 4, COL_BLACK);
    gfx_circle(f, 600, 598, 5, COL_GRAY_DARK);
    gfx_circle(f, 607, 592, 6, COL_GRAY_MID);

    draw_gallery_card(f, 688, 568, 160, 74, "CRATER");
    gfx_circle(f, 768, 598, 12, COL_ROCK_DARK);
    gfx_circle(f, 768, 598, 8, COL_BROWN_DARK);
    gfx_circle(f, 768, 598, 4, COL_BLACK);

    draw_gallery_card(f, 856, 568, 152, 74, "CARRIER ARROW");
    gfx_triangle(f, 944, 598, 922, 589, 922, 607, COL_CYAN);
    gfx_line(f, 944, 598, 922, 589, COL_WHITE);
    gfx_line(f, 944, 598, 922, 607, COL_WHITE);
}

void render_game(const Game *g, Framebuffer *f) {
    if (g->state == STATE_GALLERY) {
        draw_gallery_screen(g, f);
        if (g->quit_prompt) draw_quit_prompt(f);
        return;
    }
    if (g->state == STATE_TITLE) {
        draw_title_screen(g, f);
        if (g->quit_prompt) draw_quit_prompt(f);
        return;
    }

    int cam_x = (int)g->cam_x;
    int cam_y = (int)g->cam_y;

    world_render(&g->world, f, cam_x, cam_y, g->time_sec);

    /* Structures */
    for (int i = 0; i < MAX_STRUCTURES; i++) {
        if (g->structures[i].active) {
            draw_structure(&g->structures[i], f, &g->world, cam_x, cam_y, g->time_sec);
        }
    }

    /* Friendly carrier */
    draw_carrier_friendly(g, f, cam_x, cam_y);

    /* Surface/Sea enemies first, then air enemies */
    for (int pass = 0; pass < 2; pass++) {
        for (int i = 0; i < MAX_ENEMIES; i++) {
            const EnemyUnit *e = &g->enemies[i];
            if (!e->active) continue;
            int is_air = (e->type == ENEMY_FIGHTER || e->type == ENEMY_BOMBER);
            if ((pass == 0 && !is_air) || (pass == 1 && is_air)) {
                draw_enemy(e, f, cam_x, cam_y);
            }
        }
    }

    /* Falling bombs + shadows */
    for (int i = 0; i < MAX_BOMBS; i++) {
        const Bomb *b = &g->bombs[i];
        if (!b->active) continue;
        int sx = (int)(b->x - cam_x);
        int sy = (int)(b->y - cam_y);
        int offset = (int)(b->z * 16.0f);
        gfx_circle(f, sx + offset / 2, sy + offset, 3, COL_BLACK);
        gfx_circle(f, sx, sy, 4, COL_YELLOW);
        gfx_circle(f, sx, sy, 2, COL_RED);
    }

    /* Bullets */
    for (int i = 0; i < MAX_BULLETS; i++) {
        const Bullet *b = &g->bullets[i];
        if (!b->active) continue;
        int sx = (int)(b->x - cam_x);
        int sy = (int)(b->y - cam_y);
        gfx_circle(f, sx, sy, b->from_player ? 2 : 3, b->col);
    }

    /* Dedicated drifting smoke layer (above structures/units, below player helicopter) */
    for (int i = 0; i < MAX_PARTICLES; i++) {
        const Particle *pt = &g->particles[i];
        if (!pt->active || pt->kind != 1) continue;
        int sx = (int)(pt->x - cam_x);
        int sy = (int)(pt->y - cam_y);
        gfx_circle(f, sx, sy, (int)pt->size, pt->col);
    }

    /* Player helicopter */
    draw_player_heli(g, f, cam_x, cam_y);

    /* Explosion & spark particles */
    for (int i = 0; i < MAX_PARTICLES; i++) {
        const Particle *pt = &g->particles[i];
        if (!pt->active || pt->kind != 0) continue;
        int sx = (int)(pt->x - cam_x);
        int sy = (int)(pt->y - cam_y);
        gfx_circle(f, sx, sy, (int)pt->size, pt->col);
    }

    draw_hud(g, f);

    if (g->state == STATE_PAUSED) {
        gfx_rect(f, 342, 320, 340, 120, COL_BLACK);
        gfx_rect_outline(f, 342, 320, 340, 120, COL_YELLOW);
        const char *p = "PAUSED";
        gfx_text(f, (SCREEN_W - gfx_text_width(4, p)) / 2, 348, 4, COL_YELLOW, "%s", p);
        const char *r = "PRESS P/ESC TO RESUME";
        gfx_text(f, (SCREEN_W - gfx_text_width(2, r)) / 2, 400, 2, COL_WHITE, "%s", r);
    } else if (g->state == STATE_VICTORY || g->state == STATE_GAMEOVER) {
        gfx_rect(f, 242, 260, 540, 220, COL_BLACK);
        gfx_rect_outline(f, 242, 260, 540, 220, g->state == STATE_VICTORY ? COL_GREEN_BRIGHT : COL_RED);
        const char *hdr = (g->state == STATE_VICTORY) ? "MISSION ACCOMPLISHED!" : "MISSION FAILED";
        gfx_text(f, (SCREEN_W - gfx_text_width(3, hdr)) / 2, 290, 3,
                 g->state == STATE_VICTORY ? COL_GREEN_BRIGHT : COL_RED, "%s", hdr);
        if (g->state == STATE_GAMEOVER && g->gameover_reason[0]) {
            gfx_text(f, (SCREEN_W - gfx_text_width(2, g->gameover_reason)) / 2, 326, 2,
                     COL_YELLOW, "%s", g->gameover_reason);
        }

        gfx_text(f, 330, 355, 2, COL_WHITE, "FINAL SCORE: %d", g->score);
        gfx_text(f, 330, 385, 2, COL_YELLOW, "FACTORIES DESTROYED: %d", g->factories_destroyed);

        if (g->end_screen_timer <= 0.0f) {
            const char *again = "PRESS ENTER OR START/A TO CONTINUE";
            gfx_text(f, (SCREEN_W - gfx_text_width(2, again)) / 2, 435, 2, COL_CYAN, "%s", again);
        }
    }

    if (g->quit_prompt) {
        draw_quit_prompt(f);
    }
}
