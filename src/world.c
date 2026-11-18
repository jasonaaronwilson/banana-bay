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

    /* Compute per-tile and per-pixel elevation */
    for (int ty = 0; ty < WORLD_TILES_H; ty++) {
        for (int tx = 0; tx < WORLD_TILES_W; tx++) {
            float wx = (float)(tx * TILE_SIZE + TILE_SIZE / 2);
            float wy = (float)(ty * TILE_SIZE + TILE_SIZE / 2);

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

    /* Render pixel-art terrain into w->pixels */
    for (int py = 0; py < WORLD_H; py++) {
        int ty = py / TILE_SIZE;
        for (int px = 0; px < WORLD_W; px++) {
            int tx = px / TILE_SIZE;
            /* Perturb lookup slightly with high-frequency noise for organic coastlines */
            float micro = (hash_f(px >> 2, py >> 2, seed + 77) - 0.5f) * 0.045f;
            float n = fbm((float)px * 0.0022f, (float)py * 0.0022f, seed);

            /* Sample tile + micro dither */
            int dither = ((px ^ py) & 1);
            TileType base = (TileType)w->tiles[ty][tx];

            /* Check neighbor for coastline foam / dither */
            int nx = (px + (dither ? 4 : -4)) / TILE_SIZE;
            int ny = (py + (dither ? 4 : -4)) / TILE_SIZE;
            if (nx < 0) nx = 0;
            if (nx >= WORLD_TILES_W) nx = WORLD_TILES_W - 1;
            if (ny < 0) ny = 0;
            if (ny >= WORLD_TILES_H) ny = WORLD_TILES_H - 1;
            TileType nb = (TileType)w->tiles[ny][nx];

            TileType eff = ((hash2d(px >> 1, py >> 1, seed) & 3) == 0) ? nb : base;
            uint8_t col = COL_DEEP_SEA;

            switch (eff) {
                case TILE_DEEP_SEA:
                    col = ((n + micro) > 0.38f && dither) ? COL_MID_SEA : COL_DEEP_SEA;
                    break;
                case TILE_MID_SEA:
                    col = ((n + micro) > 0.45f && dither) ? COL_SHALLOW_SEA : COL_MID_SEA;
                    break;
                case TILE_SHALLOW_SEA:
                    if (nb == TILE_SAND && ((px + py) & 3) == 0) {
                        col = COL_FOAM;
                    } else {
                        col = COL_SHALLOW_SEA;
                    }
                    break;
                case TILE_SAND:
                    col = ((hash2d(px, py, seed) & 7) == 0) ? COL_SAND_DARK : COL_SAND_LIGHT;
                    break;
                case TILE_GRASS:
                    col = ((n + micro) > 0.56f && dither) ? COL_GRASS_MID : COL_GRASS_LIGHT;
                    break;
                case TILE_JUNGLE:
                    col = ((hash2d(px >> 1, py >> 1, seed + 9) & 3) == 0) ? COL_JUNGLE_DARK : COL_GRASS_DARK;
                    break;
                case TILE_ROCK:
                    col = ((n + micro) > 0.72f) ? COL_ROCK_LIGHT : COL_ROCK_DARK;
                    break;
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

    /* Subtle animated water glints on sea pixels */
    int phase = (int)(time_sec * 6.0f);
    for (int y = (phase & 3); y < SCREEN_H; y += 24) {
        int wy = cam_y + y;
        for (int x = ((phase * 7 + y) % 32); x < SCREEN_W; x += 32) {
            int wx = cam_x + x;
            uint8_t c = f->fb[y * SCREEN_W + x];
            if (c == COL_DEEP_SEA || c == COL_MID_SEA || c == COL_SHALLOW_SEA) {
                if (((wx + wy + phase * 3) & 31) < 3) {
                    gfx_pixel(f, x, y, COL_FOAM);
                    gfx_pixel(f, x + 1, y, COL_SHALLOW_SEA);
                }
            }
        }
    }
}
