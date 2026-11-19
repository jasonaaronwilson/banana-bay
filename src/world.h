#ifndef BANANA_WORLD_H
#define BANANA_WORLD_H

#include "palette.h"
#include <stdint.h>

/* Easily configurable world size in screens */
#define WORLD_SCREENS_W 5
#define WORLD_SCREENS_H 10

#define WORLD_W (WORLD_SCREENS_W * SCREEN_W)
#define WORLD_H (WORLD_SCREENS_H * SCREEN_H)

#define TILE_SIZE 16
#define WORLD_TILES_W (WORLD_W / TILE_SIZE)
#define WORLD_TILES_H (WORLD_H / TILE_SIZE)

/* X coordinate of the friendly carrier's south-to-north sea lane */
#define CARRIER_LANE_X 640

typedef enum {
    TILE_DEEP_SEA = 0,
    TILE_MID_SEA,
    TILE_SHALLOW_SEA,
    TILE_SAND,
    TILE_GRASS,
    TILE_JUNGLE,
    TILE_ROCK
} TileType;

typedef struct {
    uint32_t seed;
    uint8_t *pixels; /* WORLD_W * WORLD_H indexed color pixels */
    uint8_t tiles[WORLD_TILES_H][WORLD_TILES_W];
} World;

void world_init(World *w);
void world_free(World *w);
void world_generate(World *w, uint32_t seed);

TileType world_get_tile(const World *w, float wx, float wy);
int world_is_land(const World *w, float wx, float wy);
int world_is_coast(const World *w, int tx, int ty);

/* Stamp a blast crater / scorch mark into the pixel-art terrain */
void world_stamp_crater(World *w, int wx, int wy, int radius);

/* Draw a pixel-art road (with optional shallow-water bridge) between two points */
void world_draw_road(World *w, int x0, int y0, int x1, int y1);

/* Copy visible viewport from world pixels to framebuffer + animate water */
void world_render(const World *w, Framebuffer *f, int cam_x, int cam_y, float time_sec);

#endif /* BANANA_WORLD_H */
