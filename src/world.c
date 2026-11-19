#include "world.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

static uint32_t hash2d(int x, int y, uint32_t seed) {
    uint32_t h = (uint32_t)x * 374761393u + (uint32_t)y * 668265263u + seed * 1442695041u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

static float hash_f(int x, int y, uint32_t seed) {
    return (float)(hash2d(x, y, seed) & 0xFFFFu) / 65535.0f;
}

static float smoothstep(float t) {
    return t * t * (3.0f - 2.0f * t);
}

static float value_noise(float x, float y, uint32_t seed) {
    int ix = (int)floorf(x);
    int iy = (int)floorf(y);
    float fx = smoothstep(x - (float)ix);
    float fy = smoothstep(y - (float)iy);

    float v00 = hash_f(ix, iy, seed);
    float v10 = hash_f(ix + 1, iy, seed);
    float v01 = hash_f(ix, iy + 1, seed);
    float v11 = hash_f(ix + 1, iy + 1, seed);

    float vx0 = v00 + (v10 - v00) * fx;
    float vx1 = v01 + (v11 - v01) * fx;
    return vx0 + (vx1 - vx0) * fy;
}

static float fbm(float x, float y, uint32_t seed) {
    float val = 0.0f;
    float amp = 0.5f;
    float freq = 1.0f;
    for (int i = 0; i < 5; i++) {
        val += amp * value_noise(x * freq, y * freq, seed + (uint32_t)(i * 101));
        freq *= 2.05f;
        amp *= 0.5f;
    }
    return val;
}

static void w_pixel(World *w, int x, int y, uint8_t col) {
    if ((unsigned)x < WORLD_W && (unsigned)y < WORLD_H) {
        w->pixels[y * WORLD_W + x] = col;
    }
}

static void draw_palm_tree(World *w, int cx, int cy, uint32_t rng) {
    int lean = (int)(rng % 5) - 2;
    /* Trunk */
    for (int i = 0; i < 7; i++) {
        int px = cx + (lean * i) / 6;
        int py = cy - i;
        w_pixel(w, px, py, COL_BROWN_DARK);
        w_pixel(w, px + 1, py, COL_BROWN);
    }
    int tx = cx + lean;
    int ty = cy - 7;
    /* Fronds */
    static const int dx[6] = {-4, -3, 0, 3, 4, 0};
    static const int dy[6] = { 1, -2, -3, -2, 1, 2};
    for (int f = 0; f < 6; f++) {
        for (int s = 1; s <= 4; s++) {
            int fx = tx + (dx[f] * s) / 3;
            int fy = ty + (dy[f] * s) / 3 + (s == 4 ? 1 : 0);
            uint8_t col = (s <= 2) ? COL_GRASS_LIGHT : COL_GRASS_DARK;
            w_pixel(w, fx, fy, col);
        }
    }
    w_pixel(w, tx, ty, COL_GREEN_BRIGHT);
}

void world_init(World *w) {
    w->pixels = (uint8_t *)malloc((size_t)WORLD_W * (size_t)WORLD_H);
    w->seed = 1;
}

void world_free(World *w) {
    free(w->pixels);
    w->pixels = NULL;
}

typedef struct {
    float cx, cy, rx, ry;
} IslandBlob;

void world_generate(World *w, uint32_t seed) {
    w->seed = seed;

    /* Generate ~18 randomized island centers across the 5x10 world,
       avoiding the western carrier sea lane and top/bottom edges. */
    #define NUM_ISLANDS 20
    IslandBlob islands[NUM_ISLANDS];
    for (int i = 0; i < NUM_ISLANDS; i++) {
        uint32_t h1 = hash2d(i, 1, seed);
        uint32_t h2 = hash2d(i, 2, seed);
        uint32_t h3 = hash2d(i, 3, seed);
        uint32_t h4 = hash2d(i, 4, seed);

        /* Place islands mostly east of the carrier lane (x > 1050) */
        float x_norm = (float)(h1 & 0xFFFFu) / 65535.0f;
        float y_norm = (float)(h2 & 0xFFFFu) / 65535.0f;
        islands[i].cx = 1100.0f + x_norm * (float)(WORLD_W - 1450);
        islands[i].cy = 450.0f + y_norm * (float)(WORLD_H - 900);
        islands[i].rx = 260.0f + (float)(h3 % 340);
        islands[i].ry = 240.0f + (float)(h4 % 360);
    }

    static float tile_fbm[WORLD_TILES_H][WORLD_TILES_W];
    static float tile_elev[WORLD_TILES_H][WORLD_TILES_W];

    /* Compute per-tile elevation (sampled at tile corners for smooth bilinear rendering) */
    for (int ty = 0; ty < WORLD_TILES_H; ty++) {
        for (int tx = 0; tx < WORLD_TILES_W; tx++) {
            float wx = (float)(tx * TILE_SIZE);
            float wy = (float)(ty * TILE_SIZE);

            float blob = 0.0f;
            for (int i = 0; i < NUM_ISLANDS; i++) {
                float dx = (wx - islands[i].cx) / islands[i].rx;
                float dy = (wy - islands[i].cy) / islands[i].ry;
                float d2 = dx * dx + dy * dy;
                if (d2 < 1.6f) {
                    float contrib = 1.0f - d2 / 1.6f;
                    if (contrib > blob) blob = contrib;
                }
            }

            float n = fbm(wx * 0.0022f, wy * 0.0022f, seed);
            tile_fbm[ty][tx] = n;
            float elev = blob * 0.65f + n * 0.48f - 0.22f;

            /* Carve guaranteed carrier sea lane around CARRIER_LANE_X */
            float lane_dist = fabsf(wx - (float)CARRIER_LANE_X);
            if (lane_dist < 320.0f) {
                float carve = 1.0f - (lane_dist / 320.0f);
                elev -= carve * 0.8f;
            }
            /* Keep outer borders ocean */
            if (wx < 120.0f || wx > WORLD_W - 120.0f || wy < 120.0f || wy > WORLD_H - 120.0f) {
                elev -= 0.5f;
            }
            tile_elev[ty][tx] = elev;

            TileType t;
            if (elev < 0.24f)      t = TILE_DEEP_SEA;
            else if (elev < 0.34f) t = TILE_MID_SEA;
            else if (elev < 0.42f) t = TILE_SHALLOW_SEA;
            else if (elev < 0.47f) t = TILE_SAND;
            else if (elev < 0.60f) t = TILE_GRASS;
            else if (elev < 0.72f) t = TILE_JUNGLE;
            else                   t = TILE_ROCK;

            w->tiles[ty][tx] = (uint8_t)t;
        }
    }

    /* Render smooth, curved pixel-art terrain into w->pixels */
    for (int py = 0; py < WORLD_H; py++) {
        int ty = py / TILE_SIZE;
        int ty1 = (ty + 1 < WORLD_TILES_H) ? ty + 1 : ty;
        float fy = (float)(py % TILE_SIZE) / (float)TILE_SIZE;
        int cy0 = py >> 3;
        float mfy = (float)(py & 7) * 0.125f;

        for (int px = 0; px < WORLD_W; px++) {
            int tx = px / TILE_SIZE;
            int tx1 = (tx + 1 < WORLD_TILES_W) ? tx + 1 : tx;
            float fx = (float)(px % TILE_SIZE) / (float)TILE_SIZE;

            /* Bilinearly interpolate elevation & fbm across 16x16 tiles for smooth coastlines */
            float e00 = tile_elev[ty][tx];
            float e10 = tile_elev[ty][tx1];
            float e01 = tile_elev[ty1][tx];
            float e11 = tile_elev[ty1][tx1];
            float ex0 = e00 + (e10 - e00) * fx;
            float ex1 = e01 + (e11 - e01) * fx;
            float elev = ex0 + (ex1 - ex0) * fy;

            float n00 = tile_fbm[ty][tx];
            float n10 = tile_fbm[ty][tx1];
            float n01 = tile_fbm[ty1][tx];
            float n11 = tile_fbm[ty1][tx1];
            float nx0 = n00 + (n10 - n00) * fx;
            float nx1 = n01 + (n11 - n01) * fx;
            float n = nx0 + (nx1 - nx0) * fy;

            /* Fast 8x8 bilinear micro-perturbation for organic coves & ridges */
            int cx0 = px >> 3;
            float mfx = (float)(px & 7) * 0.125f;
            float m00 = hash_f(cx0,     cy0,     seed + 77);
            float m10 = hash_f(cx0 + 1, cy0,     seed + 77);
            float m01 = hash_f(cx0,     cy0 + 1, seed + 77);
            float m11 = hash_f(cx0 + 1, cy0 + 1, seed + 77);
            float mx0 = m00 + (m10 - m00) * mfx;
            float mx1 = m01 + (m11 - m01) * mfx;
            float micro = ((mx0 + (mx1 - mx0) * mfy) - 0.5f) * 0.036f;

            int dither = ((px ^ py) & 1);
            float pe = elev + micro + (dither ? 0.006f : -0.006f);

            uint8_t col = COL_DEEP_SEA;
            if (pe < 0.24f) {
                col = ((n + micro) > 0.38f && dither) ? COL_MID_SEA : COL_DEEP_SEA;
            } else if (pe < 0.34f) {
                col = ((n + micro) > 0.45f && dither) ? COL_SHALLOW_SEA : COL_MID_SEA;
            } else if (pe < 0.42f) {
                /* Curved shoreline surf foam */
                if (elev + micro > 0.409f && ((px + py) & 3) == 0) {
                    col = COL_FOAM;
                } else {
                    col = COL_SHALLOW_SEA;
                }
            } else if (pe < 0.47f) {
                col = ((hash2d(px, py, seed) & 7) == 0) ? COL_SAND_DARK : COL_SAND_LIGHT;
            } else if (pe < 0.60f) {
                col = ((n + micro) > 0.56f && dither) ? COL_GRASS_MID : COL_GRASS_LIGHT;
            } else if (pe < 0.72f) {
                col = ((hash2d(px >> 1, py >> 1, seed + 9) & 3) == 0) ? COL_JUNGLE_DARK : COL_GRASS_DARK;
            } else {
                col = ((n + micro) > 0.72f) ? COL_ROCK_LIGHT : COL_ROCK_DARK;
            }
            w->pixels[py * WORLD_W + px] = col;
        }
    }

    /* Scatter pixel-art palm trees on grass and jungle tiles */
    for (int ty = 2; ty < WORLD_TILES_H - 2; ty++) {
        for (int tx = 2; tx < WORLD_TILES_W - 2; tx++) {
            TileType t = (TileType)w->tiles[ty][tx];
            uint32_t h = hash2d(tx, ty, seed + 999);
            if ((t == TILE_JUNGLE && (h % 5) == 0) ||
                (t == TILE_GRASS  && (h % 14) == 0) ||
                (t == TILE_SAND   && (h % 28) == 0)) {
                int px = tx * TILE_SIZE + (int)((h >> 4) % 10) + 3;
                int py = ty * TILE_SIZE + (int)((h >> 8) % 10) + 3;
                draw_palm_tree(w, px, py, h);
            }
        }
    }
}

TileType world_get_tile(const World *w, float wx, float wy) {
    int tx = (int)(wx / TILE_SIZE);
    int ty = (int)(wy / TILE_SIZE);
    if (tx < 0) tx = 0;
    if (tx >= WORLD_TILES_W) tx = WORLD_TILES_W - 1;
    if (ty < 0) ty = 0;
    if (ty >= WORLD_TILES_H) ty = WORLD_TILES_H - 1;
    return (TileType)w->tiles[ty][tx];
}

int world_is_land(const World *w, float wx, float wy) {
    return world_get_tile(w, wx, wy) >= TILE_SAND;
}

int world_is_coast(const World *w, int tx, int ty) {
    if (tx <= 1 || tx >= WORLD_TILES_W - 2 || ty <= 1 || ty >= WORLD_TILES_H - 2) return 0;
    if (w->tiles[ty][tx] < TILE_SAND) return 0;
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            if (w->tiles[ty + dy][tx + dx] <= TILE_SHALLOW_SEA) return 1;
        }
    }
    return 0;
}

static void draw_ortho_segment(World *w, int x0, int y0, int x1, int y1) {
    int dx = (x1 > x0) ? 1 : (x1 < x0 ? -1 : 0);
    int dy = (y1 > y0) ? 1 : (y1 < y0 ? -1 : 0);
    int steps = abs(x1 - x0) + abs(y1 - y0);
    int px = x0, py = y0;
    const int hw = 4;

    for (int i = 0; i <= steps; i++) {
        int is_land = world_is_land(w, (float)px, (float)py);
        for (int oy = -hw; oy <= hw; oy++) {
            int wy = py + oy;
            if ((unsigned)wy >= WORLD_H) continue;
            for (int ox = -hw; ox <= hw; ox++) {
                int wx = px + ox;
                if ((unsigned)wx >= WORLD_W) continue;
                int edge = (abs(ox) == hw || abs(oy) == hw);
                int center = (dx != 0) ? (oy == 0) : (ox == 0);
                uint8_t col;
                if (is_land) {
                    if (center && ((i / 8) & 1) == 0) {
                        col = COL_YELLOW;
                    } else if (edge) {
                        col = COL_ROCK_DARK;
                    } else {
                        col = COL_GRAY_DARK;
                    }
                } else {
                    col = (edge || (i & 3) == 0) ? COL_BROWN_DARK : COL_BROWN;
                }
                w->pixels[wy * WORLD_W + wx] = col;
            }
        }
        px += dx;
        py += dy;
    }
}

static int check_ortho_segment(const World *w, int x0, int y0, int x1, int y1, int *land_cnt, int *total_cnt) {
    int dx = (x1 > x0) ? 1 : (x1 < x0 ? -1 : 0);
    int dy = (y1 > y0) ? 1 : (y1 < y0 ? -1 : 0);
    int steps = abs(x1 - x0) + abs(y1 - y0);
    for (int i = 0; i <= steps; i += 16) {
        float px = (float)(x0 + dx * i);
        float py = (float)(y0 + dy * i);
        TileType tile = world_get_tile(w, px, py);
        (*total_cnt)++;
        if (tile >= TILE_SAND) (*land_cnt)++;
        else if (tile < TILE_SHALLOW_SEA) return 0;
    }
    return 1;
}

void world_draw_road(World *w, int x0, int y0, int x1, int y1) {
    if (abs(x1 - x0) + abs(y1 - y0) < 24) return;

    /* SimCity-style orthogonal road: L-shaped or 2-turn Manhattan route */
    uint32_t h = hash2d(x0, y0, w->seed + 313u);
    int mode = (int)(h % 3u);
    int land_cnt = 0, total_cnt = 0;

    if (mode == 0) {
        /* Horizontal first, then vertical */
        if (!check_ortho_segment(w, x0, y0, x1, y0, &land_cnt, &total_cnt)) return;
        if (!check_ortho_segment(w, x1, y0, x1, y1, &land_cnt, &total_cnt)) return;
        if (total_cnt == 0 || land_cnt * 10 < total_cnt * 8) return;
        draw_ortho_segment(w, x0, y0, x1, y0);
        draw_ortho_segment(w, x1, y0, x1, y1);
    } else if (mode == 1) {
        /* Vertical first, then horizontal */
        if (!check_ortho_segment(w, x0, y0, x0, y1, &land_cnt, &total_cnt)) return;
        if (!check_ortho_segment(w, x0, y1, x1, y1, &land_cnt, &total_cnt)) return;
        if (total_cnt == 0 || land_cnt * 10 < total_cnt * 8) return;
        draw_ortho_segment(w, x0, y0, x0, y1);
        draw_ortho_segment(w, x0, y1, x1, y1);
    } else {
        /* Stepped Z-route via midpoint X */
        int mx = (x0 + x1) / 2;
        if (!check_ortho_segment(w, x0, y0, mx, y0, &land_cnt, &total_cnt)) return;
        if (!check_ortho_segment(w, mx, y0, mx, y1, &land_cnt, &total_cnt)) return;
        if (!check_ortho_segment(w, mx, y1, x1, y1, &land_cnt, &total_cnt)) return;
        if (total_cnt == 0 || land_cnt * 10 < total_cnt * 8) return;
        draw_ortho_segment(w, x0, y0, mx, y0);
        draw_ortho_segment(w, mx, y0, mx, y1);
        draw_ortho_segment(w, mx, y1, x1, y1);
    }
}

void world_stamp_crater(World *w, int wx, int wy, int radius) {
    int r2 = radius * radius;
    for (int dy = -radius; dy <= radius; dy++) {
        int py = wy + dy;
        if ((unsigned)py >= WORLD_H) continue;
        for (int dx = -radius; dx <= radius; dx++) {
            int px = wx + dx;
            if ((unsigned)px >= WORLD_W) continue;
            int d2 = dx * dx + dy * dy;
            if (d2 <= r2) {
                TileType t = world_get_tile(w, (float)px, (float)py);
                if (t >= TILE_SAND) {
                    if (d2 < r2 / 3) {
                        w->pixels[py * WORLD_W + px] = COL_BLACK;
                    } else if (d2 < (r2 * 2) / 3) {
                        w->pixels[py * WORLD_W + px] = COL_BROWN_DARK;
                    } else if (((px ^ py) & 1) == 0) {
                        w->pixels[py * WORLD_W + px] = COL_ROCK_DARK;
                    }
                }
            }
        }
    }
}

void world_render(const World *w, Framebuffer *f, int cam_x, int cam_y, float time_sec) {
    if (cam_x < 0) cam_x = 0;
    if (cam_y < 0) cam_y = 0;
    if (cam_x > WORLD_W - SCREEN_W) cam_x = WORLD_W - SCREEN_W;
    if (cam_y > WORLD_H - SCREEN_H) cam_y = WORLD_H - SCREEN_H;

    for (int y = 0; y < SCREEN_H; y++) {
        memcpy(&f->fb[y * SCREEN_W],
               &w->pixels[(cam_y + y) * WORLD_W + cam_x],
               SCREEN_W);
    }

    /* Organic world-space water glints (no regular screen-space grid) */
    int phase = (int)(time_sec * 5.0f);
    const int cell_w = 36;
    const int cell_h = 28;
    int first_cx = cam_x / cell_w;
    int first_cy = cam_y / cell_h;
    int last_cx = (cam_x + SCREEN_W) / cell_w;
    int last_cy = (cam_y + SCREEN_H) / cell_h;

    for (int cy = first_cy; cy <= last_cy; cy++) {
        for (int cx = first_cx; cx <= last_cx; cx++) {
            uint32_t h = hash2d(cx, cy, w->seed + 555u);
            /* Independent per-cell phase (~31% active at a time) */
            int local_phase = (int)((h >> 16) + (uint32_t)phase) & 15;
            if (local_phase >= 5) continue;

            int wx = cx * cell_w + (int)(h % (uint32_t)(cell_w - 4));
            int wy = cy * cell_h + (int)((h >> 8) % (uint32_t)(cell_h - 2));
            int sx = wx - cam_x;
            int sy = wy - cam_y;
            if ((unsigned)sx >= (unsigned)(SCREEN_W - 3) || (unsigned)sy >= (unsigned)SCREEN_H) continue;

            uint8_t c = f->fb[sy * SCREEN_W + sx];
            if (c == COL_DEEP_SEA || c == COL_MID_SEA || c == COL_SHALLOW_SEA) {
                uint8_t glint = (c == COL_DEEP_SEA) ? COL_MID_SEA :
                                (c == COL_MID_SEA ? COL_SHALLOW_SEA : COL_FOAM);
                gfx_pixel(f, sx, sy, glint);
                if (local_phase >= 1 && local_phase <= 3) {
                    gfx_pixel(f, sx + 1, sy, glint);
                }
            }
        }
    }
}
