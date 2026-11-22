#include "game.h"
#include "audio.h"
#include "render.h"
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static uint32_t g_rng = 0xCAFEBABEu;
static float randf(void) {
    g_rng = g_rng * 1664525u + 1013904223u;
    return (float)((g_rng >> 8) & 0xFFFFFFu) / 16777215.0f;
}

static void set_status(Game *g, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(g->status_msg, sizeof(g->status_msg), fmt, ap);
    va_end(ap);
    g->status_timer = 3.2f;
    g->status_is_alert = 0;
}

static void set_alert_status(Game *g, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(g->status_msg, sizeof(g->status_msg), fmt, ap);
    va_end(ap);
    g->status_timer = 3.5f;
    g->status_is_alert = 1;
}

static void spawn_particle(Game *g, float x, float y, float vx, float vy,
                           float life, float size, uint8_t col) {
    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (!g->particles[i].active) {
            g->particles[i].active = 1;
            g->particles[i].x = x;
            g->particles[i].y = y;
            g->particles[i].vx = vx;
            g->particles[i].vy = vy;
            g->particles[i].life = life;
            g->particles[i].max_life = life;
            g->particles[i].size = size;
            g->particles[i].col = col;
            g->particles[i].kind = 0;
            return;
        }
    }
}

static void spawn_smoke(Game *g, float x, float y, float size) {
    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (!g->particles[i].active) {
            g->particles[i].active = 1;
            g->particles[i].x = x;
            g->particles[i].y = y;
            g->particles[i].vx = 20.0f + randf() * 14.0f;
            g->particles[i].vy = -22.0f - randf() * 12.0f;
            g->particles[i].life = 2.4f + randf() * 1.1f;
            g->particles[i].max_life = g->particles[i].life;
            g->particles[i].size = size;
            g->particles[i].col = COL_BLACK;
            g->particles[i].kind = 1;
            return;
        }
    }
}

static void spawn_explosion(Game *g, float x, float y, int count, float power) {
    for (int i = 0; i < count; i++) {
        float ang = randf() * 2.0f * (float)M_PI;
        float spd = (0.2f + randf() * 0.8f) * power;
        uint8_t col = (i % 3 == 0) ? COL_YELLOW : (i % 3 == 1 ? COL_ORANGE : COL_RED);
        spawn_particle(g, x, y, cosf(ang) * spd, sinf(ang) * spd,
                       0.35f + randf() * 0.45f, 2.0f + randf() * 4.0f, col);
    }
    audio_play(SFX_EXPLOSION);
}

static void spawn_bullet(Game *g, int from_player, float x, float y,
                         float vx, float vy, int damage, uint8_t col, float life) {
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (!g->bullets[i].active) {
            g->bullets[i].active = 1;
            g->bullets[i].from_player = from_player;
            g->bullets[i].x = x;
            g->bullets[i].y = y;
            g->bullets[i].vx = vx;
            g->bullets[i].vy = vy;
            g->bullets[i].damage = damage;
            g->bullets[i].col = col;
            g->bullets[i].life = life;
            return;
        }
    }
}

static int count_enemy_aircraft(const Game *g) {
    int count = 0;
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (g->enemies[i].active &&
            (g->enemies[i].type == ENEMY_FIGHTER || g->enemies[i].type == ENEMY_BOMBER)) {
            count++;
        }
    }
    return count;
}

static void spawn_enemy(Game *g, EnemyType type, float x, float y) {
    if (type == ENEMY_FIGHTER || type == ENEMY_BOMBER) {
        int max_aircraft = (g->difficulty == DIFF_EASY) ? 3 :
                           (g->difficulty == DIFF_NORMAL ? 6 : 9);
        if (count_enemy_aircraft(g) >= max_aircraft) {
            return;
        }
    }
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (!g->enemies[i].active) {
            EnemyUnit *e = &g->enemies[i];
            memset(e, 0, sizeof(*e));
            e->active = 1;
            e->type = type;
            e->x = x;
            e->y = y;
            e->angle = randf() * 2.0f * (float)M_PI;
            switch (type) {
                case ENEMY_FIGHTER: e->max_hp = 24;  break;
                case ENEMY_BOMBER:  e->max_hp = 45;  break;
                case ENEMY_BOAT:    e->max_hp = 35;  break;
                case ENEMY_CARRIER: e->max_hp = 320; g->enemy_carrier_alive = 1; break;
            }
            e->hp = e->max_hp;
            e->fire_timer = 1.0f + randf() * 1.5f;
            return;
        }
    }
}

static int find_land_spot(const Game *g, int min_dist_from_existing, float *out_x, float *out_y) {
    for (int tries = 0; tries < 400; tries++) {
        float wx = 260.0f + randf() * (float)(WORLD_W - 520);
        float wy = 350.0f + randf() * (float)(WORLD_H - 700);
        TileType t = world_get_tile(&g->world, wx, wy);
        if (t < TILE_SAND || t == TILE_ROCK) continue;

        int ok = 1;
        for (int i = 0; i < MAX_STRUCTURES; i++) {
            if (!g->structures[i].active) continue;
            float dx = g->structures[i].x - wx;
            float dy = g->structures[i].y - wy;
            if (dx * dx + dy * dy < (float)(min_dist_from_existing * min_dist_from_existing)) {
                ok = 0;
                break;
            }
        }
        if (ok) {
            *out_x = wx;
            *out_y = wy;
            return 1;
        }
    }
    return 0;
}

static int find_coast_spot(const Game *g, float *out_x, float *out_y) {
    for (int tries = 0; tries < 600; tries++) {
        int tx = 16 + (int)(randf() * (float)(WORLD_TILES_W - 32));
        int ty = 20 + (int)(randf() * (float)(WORLD_TILES_H - 40));
        if (!world_is_coast(&g->world, tx, ty)) continue;
        float wx = (float)(tx * TILE_SIZE + TILE_SIZE / 2);
        float wy = (float)(ty * TILE_SIZE + TILE_SIZE / 2);

        int ok = 1;
        for (int i = 0; i < MAX_STRUCTURES; i++) {
            if (!g->structures[i].active) continue;
            float dx = g->structures[i].x - wx;
            float dy = g->structures[i].y - wy;
            if (dx * dx + dy * dy < 160.0f * 160.0f) {
                ok = 0;
                break;
            }
        }
        if (ok) {
            *out_x = wx;
            *out_y = wy;
            return 1;
        }
    }
    return 0;
}

static int find_beach_spot(const Game *g, int min_dist, float *out_x, float *out_y) {
    for (int tries = 0; tries < 600; tries++) {
        int tx = 16 + (int)(randf() * (float)(WORLD_TILES_W - 32));
        int ty = 20 + (int)(randf() * (float)(WORLD_TILES_H - 40));
        TileType t = (TileType)g->world.tiles[ty][tx];
        if (t != TILE_SAND && !(t == TILE_GRASS && world_is_coast(&g->world, tx, ty))) {
            continue;
        }
        float wx = (float)(tx * TILE_SIZE + TILE_SIZE / 2);
        float wy = (float)(ty * TILE_SIZE + TILE_SIZE / 2);

        int ok = 1;
        for (int i = 0; i < MAX_STRUCTURES; i++) {
            if (!g->structures[i].active) continue;
            float dx = g->structures[i].x - wx;
            float dy = g->structures[i].y - wy;
            if (dx * dx + dy * dy < (float)(min_dist * min_dist)) {
                ok = 0;
                break;
            }
        }
        if (ok) {
            *out_x = wx;
            *out_y = wy;
            return 1;
        }
    }
    return 0;
}

static void add_structure(Game *g, StructureType type, float x, float y, int building_carrier) {
    for (int i = 0; i < MAX_STRUCTURES; i++) {
        if (!g->structures[i].active) {
            Structure *s = &g->structures[i];
            memset(s, 0, sizeof(*s));
            s->active = 1;
            s->type = type;
            s->x = x;
            s->y = y;
            s->level = 1;
            s->building_carrier = building_carrier;
            s->fire_timer = 1.0f + randf() * 2.0f;
            s->spawn_timer = 6.0f + randf() * 8.0f;
            switch (type) {
                case STRUCT_FACTORY: {
                    float r = randf();
                    if (g->difficulty == DIFF_EASY) {
                        s->size = (r < 0.45f) ? 0 : (r < 0.85f ? 1 : 2);
                    } else if (g->difficulty == DIFF_NORMAL) {
                        s->size = (r < 0.30f) ? 0 : (r < 0.70f ? 1 : 2);
                    } else {
                        s->size = (r < 0.20f) ? 0 : (r < 0.55f ? 1 : 2);
                    }
                    /* Bombs deal 55 dmg: small = 1 bomb, medium = 2 bombs, large = 3 bombs */
                    s->max_hp = (s->size == 0) ? 50 : (s->size == 1 ? 100 : 160);
                    g->factories_alive++;
                    break;
                }
                case STRUCT_SHIPYARD: s->max_hp = 110; break;
                case STRUCT_AA:       s->max_hp = 45;  break;
                case STRUCT_BARRACKS: s->max_hp = 60;  break;
                case STRUCT_RADAR:    s->max_hp = 40;  break;
                case STRUCT_OIL_FIELD: {
                    float r = randf();
                    s->size = (r < 0.35f) ? 0 : (r < 0.75f ? 1 : 2);
                    s->max_hp = (s->size == 0) ? 20 : (s->size == 1 ? 35 : 55);
                    break;
                }
                case STRUCT_BUNGALOW: {
                    float r = randf();
                    s->size = (r < 0.35f) ? 0 : (r < 0.70f ? 1 : 2); /* 0=hamlet, 1=village, 2=large village, 3=resort town */
                    s->max_hp = (s->size < 2) ? 35 : 55;
                    break;
                }
                case STRUCT_TOWER: {
                    float r = randf();
                    s->size = (r < 0.25f) ? 0 : (r < 0.50f ? 1 : (r < 0.75f ? 2 : 3));
                    s->max_hp = (s->size >= 2) ? 85 : 70;
                    break;
                }
                case STRUCT_NUCLEAR:
                    s->max_hp = 200;
                    break;
            }
            s->hp = s->max_hp;
            g->num_structures++;
            return;
        }
    }
}

void game_init(Game *g) {
    memset(g, 0, sizeof(*g));
    world_init(&g->world);
    g->state = STATE_TITLE;
    g->difficulty = DIFF_NORMAL;
    world_generate(&g->world, (uint32_t)time(NULL));
}

void game_free(Game *g) {
    world_free(&g->world);
}

static void place_nuclear_reactor(Game *g) {
    static uint16_t island_id[WORLD_TILES_H][WORLD_TILES_W];
    static int qx[WORLD_TILES_W * WORLD_TILES_H];
    static int qy[WORLD_TILES_W * WORLD_TILES_H];
    memset(island_id, 0, sizeof(island_id));

    uint16_t cur_id = 0;
    int best_id = 0;
    int best_area = 0;

    for (int ty = 0; ty < WORLD_TILES_H; ty++) {
        for (int tx = 0; tx < WORLD_TILES_W; tx++) {
            if (island_id[ty][tx] != 0 || g->world.tiles[ty][tx] < TILE_SAND) continue;
            cur_id++;
            int head = 0, tail = 0;
            island_id[ty][tx] = cur_id;
            qx[tail] = tx;
            qy[tail] = ty;
            tail++;

            while (head < tail) {
                int cx = qx[head];
                int cy = qy[head];
                head++;
                static const int dx[4] = {-1, 1, 0, 0};
                static const int dy[4] = {0, 0, -1, 1};
                for (int d = 0; d < 4; d++) {
                    int nx = cx + dx[d];
                    int ny = cy + dy[d];
                    if ((unsigned)nx < WORLD_TILES_W && (unsigned)ny < WORLD_TILES_H &&
                        island_id[ny][nx] == 0 && g->world.tiles[ny][nx] >= TILE_SAND) {
                        island_id[ny][nx] = cur_id;
                        qx[tail] = nx;
                        qy[tail] = ny;
                        tail++;
                    }
                }
            }

            /* Upgrade beach villages on large islands (>= 220 tiles) to upscale resort villages */
            if (tail >= 220) {
                for (int s = 0; s < MAX_STRUCTURES; s++) {
                    if (!g->structures[s].active || g->structures[s].type != STRUCT_BUNGALOW) continue;
                    int bx = (int)(g->structures[s].x / TILE_SIZE);
                    int by = (int)(g->structures[s].y / TILE_SIZE);
                    if ((unsigned)bx < WORLD_TILES_W && (unsigned)by < WORLD_TILES_H &&
                        island_id[by][bx] == cur_id) {
                        g->structures[s].size = 3;
                        g->structures[s].max_hp = 65;
                        g->structures[s].hp = 65;
                    }
                }
            }

            /* Island must be big enough (>= 220 land tiles) and have >= 3 factories */
            if (tail >= 220) {
                int fcount = 0;
                for (int s = 0; s < MAX_STRUCTURES; s++) {
                    if (!g->structures[s].active || g->structures[s].type != STRUCT_FACTORY) continue;
                    int fx = (int)(g->structures[s].x / TILE_SIZE);
                    int fy = (int)(g->structures[s].y / TILE_SIZE);
                    if ((unsigned)fx < WORLD_TILES_W && (unsigned)fy < WORLD_TILES_H &&
                        island_id[fy][fx] == cur_id) {
                        fcount++;
                    }
                }
                if (fcount >= 3 && tail > best_area) {
                    best_area = tail;
                    best_id = cur_id;
                }
            }
        }
    }

    if (best_id == 0) return;

    for (int tries = 0; tries < 500; tries++) {
        int tx = 16 + (int)(randf() * (float)(WORLD_TILES_W - 32));
        int ty = 20 + (int)(randf() * (float)(WORLD_TILES_H - 40));
        if (island_id[ty][tx] != best_id) continue;
        TileType t = (TileType)g->world.tiles[ty][tx];
        if (t < TILE_SAND || t == TILE_ROCK) continue;

        float wx = (float)(tx * TILE_SIZE + TILE_SIZE / 2);
        float wy = (float)(ty * TILE_SIZE + TILE_SIZE / 2);
        int ok = 1;
        for (int i = 0; i < MAX_STRUCTURES; i++) {
            if (!g->structures[i].active) continue;
            float dx = g->structures[i].x - wx;
            float dy = g->structures[i].y - wy;
            if (dx * dx + dy * dy < 120.0f * 120.0f) {
                ok = 0;
                break;
            }
        }
        if (ok) {
            add_structure(g, STRUCT_NUCLEAR, wx, wy, 0);
            return;
        }
    }
}

void game_start_new(Game *g, Difficulty diff, uint32_t seed) {
    fprintf(stderr, "[game] Starting new game (diff=%d, seed=%u)\n", (int)diff, seed);
    fflush(stderr);
    g->difficulty = diff;
    g_rng = seed ? seed : 1u;
    world_generate(&g->world, g_rng);

    memset(g->structures, 0, sizeof(g->structures));
    memset(g->enemies, 0, sizeof(g->enemies));
    memset(g->bullets, 0, sizeof(g->bullets));
    memset(g->bombs, 0, sizeof(g->bombs));
    memset(g->particles, 0, sizeof(g->particles));

    g->num_structures = 0;
    g->score = 0;
    g->factories_alive = 0;
    g->factories_destroyed = 0;
    g->enemy_carrier_alive = 0;
    g->factory_spawn_timer = 45.0f;
    g->gameover_reason[0] = '\0';

    /* Friendly carrier starts in the southern sea lane and steams north */
    g->carrier.x = (float)CARRIER_LANE_X;
    g->carrier.y = (float)(WORLD_H - 500);
    g->carrier.vy = -12.0f; /* South to North */
    g->carrier.max_hp = (diff == DIFF_EASY) ? 500 : (diff == DIFF_NORMAL ? 380 : 280);
    g->carrier.hp = g->carrier.max_hp;
    g->carrier.aa_timer = 0.0f;
    g->carrier.warn_timer = 0.0f;

    /* Player starts on the carrier */
    memset(&g->player, 0, sizeof(g->player));
    g->player.x = g->carrier.x;
    g->player.y = g->carrier.y + 30.0f;
    g->player.angle = -(float)M_PI * 0.5f; /* Facing north */
    g->player.max_hp = 100;
    g->player.hp = 100;
    g->player.bombs = MAX_BOMBS_CARRIED;
    g->player.lives = (diff == DIFF_EASY) ? 5 : (diff == DIFF_NORMAL ? 3 : 2);

    g->cam_x = g->player.x - SCREEN_W * 0.5f;
    g->cam_y = g->player.y - SCREEN_H * 0.5f;

    int num_factories = (diff == DIFF_EASY) ? 7 : (diff == DIFF_NORMAL ? 10 : 14);
    int num_shipyards = (diff == DIFF_EASY) ? 2 : (diff == DIFF_NORMAL ? 3 : 4);
    int num_aa        = (diff == DIFF_EASY) ? 10 : (diff == DIFF_NORMAL ? 16 : 24);
    int num_barracks  = (diff == DIFF_EASY) ? 5 : (diff == DIFF_NORMAL ? 8 : 12);
    int num_radar     = (diff == DIFF_EASY) ? 3 : (diff == DIFF_NORMAL ? 5 : 8);
    int num_oil       = (diff == DIFF_EASY) ? 6 : (diff == DIFF_NORMAL ? 9 : 12);

    /* Place at least one shipyard building the enemy carrier */
    for (int i = 0; i < num_shipyards; i++) {
        float sx, sy;
        if (find_coast_spot(g, &sx, &sy)) {
            add_structure(g, STRUCT_SHIPYARD, sx, sy, i == 0 ? 1 : 0);
        }
    }
    for (int i = 0; i < num_factories; i++) {
        float fx, fy;
        if (find_land_spot(g, 240, &fx, &fy)) {
            add_structure(g, STRUCT_FACTORY, fx, fy, 0);
        }
    }
    g->factories_initial = g->factories_alive;

    for (int i = 0; i < num_aa; i++) {
        float ax, ay;
        if (find_land_spot(g, 130, &ax, &ay)) {
            add_structure(g, STRUCT_AA, ax, ay, 0);
        }
    }
    for (int i = 0; i < num_barracks; i++) {
        float bx, by;
        if (find_land_spot(g, 150, &bx, &by)) {
            add_structure(g, STRUCT_BARRACKS, bx, by, 0);
        }
    }
    for (int i = 0; i < num_radar; i++) {
        float rx, ry;
        if (find_land_spot(g, 160, &rx, &ry)) {
            add_structure(g, STRUCT_RADAR, rx, ry, 0);
        }
    }
    for (int i = 0; i < num_oil; i++) {
        float ox, oy;
        if (find_land_spot(g, 130, &ox, &oy)) {
            add_structure(g, STRUCT_OIL_FIELD, ox, oy, 0);
        }
    }

    /* Civilian beach villages (upgraded to upscale villages with glass towers on big islands) */
    for (int i = 0; i < 14; i++) {
        float bx, by;
        if (find_beach_spot(g, 140, &bx, &by)) {
            add_structure(g, STRUCT_BUNGALOW, bx, by, 0);
        }
    }
    for (int i = 0; i < 8; i++) {
        float tx, ty;
        if (find_beach_spot(g, 140, &tx, &ty)) {
            add_structure(g, STRUCT_TOWER, tx, ty, 0);
        }
    }
    place_nuclear_reactor(g);

    /* Connect nearby non-AA structures on islands with sparse orthogonal roads */
    for (int i = 0; i < MAX_STRUCTURES; i++) {
        if (!g->structures[i].active || g->structures[i].type == STRUCT_AA) continue;
        int best_j = -1;
        float best_d2 = 480.0f * 480.0f;
        for (int j = i + 1; j < MAX_STRUCTURES; j++) {
            if (!g->structures[j].active || g->structures[j].type == STRUCT_AA) continue;
            float dx = g->structures[j].x - g->structures[i].x;
            float dy = g->structures[j].y - g->structures[i].y;
            float d2 = dx * dx + dy * dy;
            if (d2 < best_d2) {
                best_d2 = d2;
                best_j = j;
            }
        }
        if (best_j >= 0) {
            world_draw_road(&g->world,
                            (int)g->structures[i].x, (int)g->structures[i].y,
                            (int)g->structures[best_j].x, (int)g->structures[best_j].y);
        }
    }

    /* Initial air patrol */
    int init_planes = (diff == DIFF_EASY) ? 3 : (diff == DIFF_NORMAL ? 5 : 8);
    for (int i = 0; i < init_planes; i++) {
        spawn_enemy(g, ENEMY_FIGHTER,
                    260.0f + randf() * (float)(WORLD_W - 520),
                    600.0f + randf() * (float)(WORLD_H - 1200));
    }

    /* Initial sea patrol (gunboats in open water) */
    int init_boats = (diff == DIFF_EASY) ? 4 : (diff == DIFF_NORMAL ? 7 : 10);
    for (int i = 0; i < init_boats; i++) {
        for (int tries = 0; tries < 60; tries++) {
            float bx = 260.0f + randf() * (float)(WORLD_W - 520);
            float by = 520.0f + randf() * (float)(WORLD_H - 1350);
            if (!world_is_land(&g->world, (int)bx, (int)by)) {
                spawn_enemy(g, ENEMY_BOAT, bx, by);
                break;
            }
        }
    }

    g->state = STATE_PLAYING;
    fprintf(stderr, "[game] World ready: %d structures, %d factories\n",
            g->num_structures, g->factories_alive);
    fflush(stderr);
    set_status(g, "DESTROY ALL %d FACTORIES!", g->factories_alive);
}

static void damage_player(Game *g, int dmg) {
    if (g->player.invuln_timer > 0.0f) return;
    g->player.hp -= dmg;
    audio_play(SFX_HIT);
    if (g->player.hp <= 0) {
        spawn_explosion(g, g->player.x, g->player.y, 35, 160.0f);
        g->player.lives--;
        if (g->player.lives <= 0) {
            g->player.hp = 0;
            snprintf(g->gameover_reason, sizeof(g->gameover_reason), "ALL HELICOPTERS LOST!");
            g->end_screen_timer = 1.6f;
            g->state = STATE_GAMEOVER;
            audio_set_rotor(0, 0.0f);
            audio_set_gun(0);
            audio_play(SFX_GAMEOVER);
            return;
        }
        /* Respawn on friendly carrier */
        g->player.x = g->carrier.x;
        g->player.y = g->carrier.y + 30.0f;
        g->player.vx = 0.0f;
        g->player.vy = 0.0f;
        g->player.hp = g->player.max_hp;
        g->player.bombs = MAX_BOMBS_CARRIED;
        g->player.invuln_timer = 2.5f;
        set_status(g, "HELICOPTER LOST! LAUNCHING SPARE (%d LEFT)", g->player.lives);
    }
}

static void damage_structure(Game *g, Structure *s, int dmg) {
    if (!s->active) return;
    if (s->type == STRUCT_BUNGALOW || s->type == STRUCT_TOWER) {
        g->score -= 50;
        if (g->score < 0) g->score = 0;
        set_alert_status(g, "AVOID CIVILIAN CASUALTIES! (-50)");
        audio_play(SFX_PENALTY);
    }
    s->hp -= dmg;
    if (s->hp <= 0) {
        s->active = 0;
        int exp_cnt = (s->type == STRUCT_FACTORY) ? (24 + s->size * 14) : 30;
        int crater_r = (s->type == STRUCT_FACTORY) ? (16 + s->size * 7) : 20;
        spawn_explosion(g, s->x, s->y, exp_cnt, 140.0f + (float)(s->size * 20));
        world_stamp_crater(&g->world, (int)s->x, (int)s->y, crater_r);
        if (s->type == STRUCT_FACTORY) {
            g->factories_alive--;
            g->factories_destroyed++;
            g->score += 400 * (s->size + 1) * s->level;
            set_status(g, "%s FACTORY DESTROYED! %d REMAINING",
                       s->size == 2 ? "LARGE" : (s->size == 1 ? "MEDIUM" : "SMALL"),
                       g->factories_alive);
        } else if (s->type == STRUCT_SHIPYARD) {
            g->score += s->building_carrier ? 800 : 400;
            if (s->building_carrier && !s->carrier_launched) {
                set_status(g, "ENEMY CARRIER SHIPYARD DESTROYED!");
            }
        } else if (s->type == STRUCT_OIL_FIELD) {
            g->score += 200 * (s->size + 1);
            float blast_r = 75.0f + (float)(s->size * 25);
            for (int k = 0; k < MAX_STRUCTURES; k++) {
                Structure *other = &g->structures[k];
                if (!other->active) continue;
                float dx = other->x - s->x, dy = other->y - s->y;
                if (dx * dx + dy * dy <= blast_r * blast_r) {
                    damage_structure(g, other, 45 + s->size * 15);
                }
            }
        } else if (s->type == STRUCT_BUNGALOW || s->type == STRUCT_TOWER) {
            g->score -= 150;
            if (g->score < 0) g->score = 0;
            set_alert_status(g, "AVOID CIVILIAN CASUALTIES! (-200)");
            audio_play(SFX_PENALTY);
        } else {
            g->score += 150;
        }
    }
}

static void damage_enemy(Game *g, EnemyUnit *e, int dmg) {
    if (!e->active) return;
    e->hp -= dmg;
    if (e->hp <= 0) {
        e->active = 0;
        spawn_explosion(g, e->x, e->y, e->type == ENEMY_CARRIER ? 55 : 18, 130.0f);
        if (e->type == ENEMY_CARRIER) {
            g->enemy_carrier_alive = 0;
            g->score += 2000;
            set_status(g, "ENEMY CARRIER SUNK!");
        } else if (e->type == ENEMY_BOMBER) {
            g->score += 200;
        } else {
            g->score += 100;
        }
    }
}

static void update_player(Game *g, const InputState *in, float dt) {
    PlayerHeli *p = &g->player;
    if (p->invuln_timer > 0.0f) p->invuln_timer -= dt;
    if (p->gun_cooldown > 0.0f) p->gun_cooldown -= dt;
    if (p->bomb_cooldown > 0.0f) p->bomb_cooldown -= dt;

    float accel = 680.0f;
    float max_spd = 290.0f;
    float drag = 2.6f;

    float mx = in->move_x;
    float my = in->move_y;
    float mag = sqrtf(mx * mx + my * my);
    if (mag > 1.0f) { mx /= mag; my /= mag; }

    p->vx += (mx * accel - p->vx * drag) * dt;
    p->vy += (my * accel - p->vy * drag) * dt;

    float spd = sqrtf(p->vx * p->vx + p->vy * p->vy);
    if (spd > max_spd) {
        p->vx = (p->vx / spd) * max_spd;
        p->vy = (p->vy / spd) * max_spd;
        spd = max_spd;
    }

    p->x += p->vx * dt;
    p->y += p->vy * dt;

    if (p->x < 32.0f) { p->x = 32.0f; p->vx = 0; }
    if (p->x > WORLD_W - 32.0f) { p->x = WORLD_W - 32.0f; p->vx = 0; }
    if (p->y < 32.0f) { p->y = 32.0f; p->vy = 0; }
    if (p->y > WORLD_H - 32.0f) { p->y = WORLD_H - 32.0f; p->vy = 0; }

    if (mag > 0.15f) {
        float target_ang = atan2f(my, mx);
        float diff = target_ang - p->angle;
        while (diff > (float)M_PI) diff -= 2.0f * (float)M_PI;
        while (diff < -(float)M_PI) diff += 2.0f * (float)M_PI;
        p->angle += diff * (dt * 12.0f > 1.0f ? 1.0f : dt * 12.0f);
    }

    p->rotor_phase += (18.0f + spd * 0.04f) * dt;
    audio_set_rotor(1, spd / max_spd);

    /* Machine gun fire: single tap plays crisp shot; holding spins up rotary BRRRRRRT */
    if (!in->fire_gun) {
        p->gun_burst = 0;
        audio_set_gun(0);
    } else if (p->gun_cooldown <= 0.0f) {
        float ca = cosf(p->angle), sa = sinf(p->angle);
        spawn_bullet(g, 1, p->x + ca * 22.0f, p->y + sa * 22.0f,
                     p->vx * 0.3f + ca * 640.0f, p->vy * 0.3f + sa * 640.0f,
                     10, COL_YELLOW, 0.75f);
        if (p->gun_burst == 0) {
            audio_play(SFX_GUN);
        } else {
            audio_set_gun(1);
        }
        p->gun_burst++;
        if (p->gun_burst >= 8) {
            p->gun_burst = 0;
            audio_set_gun(0);
            p->gun_cooldown = (g->difficulty == DIFF_EASY) ? 0.18f :
                              (g->difficulty == DIFF_NORMAL ? 0.38f : 0.62f);
        } else {
            p->gun_cooldown = 0.045f;
        }
    } else if (p->gun_burst == 0) {
        audio_set_gun(0);
    }

    /* Bomb drop */
    if (in->drop_bomb_pressed && p->bomb_cooldown <= 0.0f) {
        if (p->bombs > 0) {
            for (int i = 0; i < MAX_BOMBS; i++) {
                if (!g->bombs[i].active) {
                    p->bombs--;
                    p->bomb_cooldown = 0.22f;
                    g->bombs[i].active = 1;
                    g->bombs[i].x = p->x;
                    g->bombs[i].y = p->y;
                    g->bombs[i].vx = p->vx * 0.65f;
                    g->bombs[i].vy = p->vy * 0.65f;
                    g->bombs[i].z = 1.0f;
                    g->bombs[i].vz = 1.55f;
                    audio_play(SFX_BOMB_DROP);
                    break;
                }
            }
        } else {
            set_status(g, "OUT OF BOMBS! RETURN TO CARRIER!");
        }
    }

    /* Resupply & repair when hovering over friendly carrier deck */
    float dx = fabsf(p->x - g->carrier.x);
    float dy = fabsf(p->y - g->carrier.y);
    if (dx < 40.0f && dy < 88.0f && spd < 140.0f) {
        p->resupply_timer += dt;
        if (p->resupply_timer >= 0.14f) {
            p->resupply_timer = 0.0f;
            int did = 0;
            if (p->bombs < MAX_BOMBS_CARRIED) { p->bombs++; did = 1; }
            if (p->hp < p->max_hp) { p->hp += 5; if (p->hp > p->max_hp) p->hp = p->max_hp; did = 1; }
            if (did) audio_play(SFX_RESUPPLY);
        }
    }

    /* Camera: helicopter stays centered, but leads forward during acceleration before camera catches up */
    float lead_x = (p->vx / max_spd) * 85.0f;
    float lead_y = (p->vy / max_spd) * 75.0f;
    float target_cam_x = p->x + lead_x - SCREEN_W * 0.5f;
    float target_cam_y = p->y + lead_y - SCREEN_H * 0.5f;

    float cam_lerp = dt * 4.5f;
    if (cam_lerp > 1.0f) cam_lerp = 1.0f;
    g->cam_x += (target_cam_x - g->cam_x) * cam_lerp;
    g->cam_y += (target_cam_y - g->cam_y) * cam_lerp;

    if (g->cam_x < 0.0f) g->cam_x = 0.0f;
    if (g->cam_y < 0.0f) g->cam_y = 0.0f;
    if (g->cam_x > WORLD_W - SCREEN_W) g->cam_x = (float)(WORLD_W - SCREEN_W);
    if (g->cam_y > WORLD_H - SCREEN_H) g->cam_y = (float)(WORLD_H - SCREEN_H);
}

static void update_carrier(Game *g, float dt) {
    FriendlyCarrier *c = &g->carrier;
    c->y += c->vy * dt;
    if (c->y < 260.0f) {
        c->y = 260.0f; /* Hold station at northern end of bay */
    }
    if (c->warn_timer > 0.0f) c->warn_timer -= dt;

    /* Carrier AA defense against nearby enemy planes */
    c->aa_timer -= dt;
    if (c->aa_timer <= 0.0f) {
        for (int i = 0; i < MAX_ENEMIES; i++) {
            EnemyUnit *e = &g->enemies[i];
            if (!e->active) continue;
            float dx = e->x - c->x;
            float dy = e->y - c->y;
            float d2 = dx * dx + dy * dy;
            if (d2 < 340.0f * 340.0f) {
                float d = sqrtf(d2);
                spawn_bullet(g, 1, c->x, c->y, (dx / d) * 440.0f, (dy / d) * 440.0f,
                             8, COL_CYAN, 0.8f);
                c->aa_timer = 0.45f;
                break;
            }
        }
    }
}

static void update_structures(Game *g, float dt) {
    float diff_mult = (g->difficulty == DIFF_EASY) ? 0.65f : (g->difficulty == DIFF_NORMAL ? 1.0f : 1.45f);

    for (int i = 0; i < MAX_STRUCTURES; i++) {
        Structure *s = &g->structures[i];
        if (!s->active) continue;

        /* Damaged factories & structures emit wind-drifted dark smoke */
        if (s->hp < s->max_hp && (s->type == STRUCT_FACTORY || s->type == STRUCT_SHIPYARD ||
                                  s->type == STRUCT_TOWER || s->type == STRUCT_BUNGALOW)) {
            s->smoke_timer -= dt;
            if (s->smoke_timer <= 0.0f) {
                int heavy = (s->hp * 2 <= s->max_hp);
                s->smoke_timer = heavy ? 0.14f : 0.24f;
                if (s->type == STRUCT_FACTORY && s->size == 2) {
                    spawn_smoke(g, s->x - 28.0f + (randf() - 0.5f) * 10.0f, s->y - 18.0f, 4.0f);
                    spawn_smoke(g, s->x + 22.0f + (randf() - 0.5f) * 10.0f, s->y + 10.0f, 3.5f);
                } else {
                    spawn_smoke(g, s->x + (randf() - 0.5f) * 10.0f, s->y - 4.0f, 3.5f);
                }
            }
        }

        if (s->type == STRUCT_FACTORY) {
            if (s->level < 3) {
                s->build_progress += dt * 0.025f * diff_mult;
                if (s->build_progress >= 1.0f) {
                    s->build_progress = 0.0f;
                    s->level++;
                    s->max_hp += 30;
                    s->hp += 30;
                }
            }
            s->spawn_timer -= dt * diff_mult * (0.7f + 0.35f * (float)s->level);
            if (s->spawn_timer <= 0.0f) {
                s->spawn_timer = 22.0f + randf() * 14.0f;
                spawn_enemy(g, (s->level >= 2 && randf() < 0.35f) ? ENEMY_BOMBER : ENEMY_FIGHTER,
                            s->x, s->y);
            }
        } else if (s->type == STRUCT_SHIPYARD) {
            if (s->building_carrier && !s->carrier_launched && !g->enemy_carrier_alive) {
                s->carrier_progress += dt * 0.011f * diff_mult;
                if (s->carrier_progress >= 1.0f) {
                    s->carrier_launched = 1;
                    /* Launch enemy carrier in nearby sea */
                    spawn_enemy(g, ENEMY_CARRIER, s->x - 70.0f, s->y);
                    set_status(g, "WARNING: ENEMY CARRIER LAUNCHED!");
                    audio_play(SFX_WARNING);
                }
            }
            s->spawn_timer -= dt * diff_mult;
            if (s->spawn_timer <= 0.0f) {
                s->spawn_timer = 15.0f + randf() * 9.0f;
                spawn_enemy(g, ENEMY_BOAT, s->x - 36.0f, s->y);
            }
        } else if (s->type == STRUCT_AA || s->type == STRUCT_RADAR) {
            s->fire_timer -= dt * diff_mult;
            float range = (s->type == STRUCT_RADAR) ? 480.0f : 380.0f;
            float dx = g->player.x - s->x;
            float dy = g->player.y - s->y;
            float d2 = dx * dx + dy * dy;
            if (s->fire_timer <= 0.0f && d2 < range * range) {
                float d = sqrtf(d2);
                float spd = (s->type == STRUCT_RADAR) ? 330.0f : 260.0f;
                spawn_bullet(g, 0, s->x, s->y, (dx / d) * spd, (dy / d) * spd,
                             10, COL_ORANGE, 1.5f);
                s->fire_timer = (s->type == STRUCT_RADAR) ? 1.7f : 1.3f;
            }
        } else if (s->type == STRUCT_BARRACKS) {
            s->spawn_timer -= dt * diff_mult;
            if (s->spawn_timer <= 0.0f) {
                s->spawn_timer = 30.0f + randf() * 15.0f;
                spawn_enemy(g, ENEMY_FIGHTER, s->x, s->y);
            }
        }
    }

    /* High-level factories can occasionally establish a new factory on Hard/Normal */
    if (g->difficulty >= DIFF_NORMAL && g->factories_alive > 0 && g->factories_alive < 16) {
        g->factory_spawn_timer -= dt * diff_mult;
        if (g->factory_spawn_timer <= 0.0f) {
            g->factory_spawn_timer = 65.0f;
            float fx, fy;
            if (find_land_spot(g, 240, &fx, &fy)) {
                add_structure(g, STRUCT_FACTORY, fx, fy, 0);
                set_status(g, "INTEL: NEW ENEMY FACTORY BUILT!");
            }
        }
    }
}

static void update_enemies(Game *g, float dt) {
    for (int i = 0; i < MAX_ENEMIES; i++) {
        EnemyUnit *e = &g->enemies[i];
        if (!e->active) continue;

        float tx = g->player.x;
        float ty = g->player.y;
        if (e->type == ENEMY_BOMBER || (e->type == ENEMY_CARRIER)) {
            tx = g->carrier.x;
            ty = g->carrier.y;
        } else if (e->type == ENEMY_BOAT) {
            /* Boats engage player if close, else patrol toward carrier */
            float dp2 = (g->player.x - e->x) * (g->player.x - e->x) +
                        (g->player.y - e->y) * (g->player.y - e->y);
            if (dp2 > 550.0f * 550.0f) {
                tx = g->carrier.x;
                ty = g->carrier.y;
            }
        }

        float dx = tx - e->x;
        float dy = ty - e->y;
        float dist = sqrtf(dx * dx + dy * dy);
        float desired = atan2f(dy, dx);

        /* Dogfighting: fighters try to approach from behind, then fly straight past on overshoot before looping back */
        if (e->type == ENEMY_FIGHTER) {
            if (e->ai_timer > 0.0f) {
                e->ai_timer -= dt;
                desired = e->angle; /* Keep flying past without turning around */
            } else if (dist < 115.0f) {
                e->ai_timer = 1.4f; /* Fly past the helicopter for 1.4s before banking back */
                desired = e->angle;
            } else if (dist > 180.0f) {
                /* Aim behind the helicopter's 6 o'clock */
                float bx = g->player.x - cosf(g->player.angle) * 130.0f;
                float by = g->player.y - sinf(g->player.angle) * 130.0f;
                desired = atan2f(by - e->y, bx - e->x);
            }
        }

        float turn_rate = (e->type == ENEMY_FIGHTER) ? 2.6f : (e->type == ENEMY_CARRIER ? 0.5f : 1.5f);
        float diff = desired - e->angle;
        while (diff > (float)M_PI) diff -= 2.0f * (float)M_PI;
        while (diff < -(float)M_PI) diff += 2.0f * (float)M_PI;
        if (diff > turn_rate * dt) diff = turn_rate * dt;
        if (diff < -turn_rate * dt) diff = -turn_rate * dt;
        e->angle += diff;

        float spd = (e->type == ENEMY_FIGHTER) ? 165.0f :
                    (e->type == ENEMY_BOMBER) ? 115.0f :
                    (e->type == ENEMY_BOAT) ? 65.0f : 28.0f;

        float nx = e->x + cosf(e->angle) * spd * dt;
        float ny = e->y + sinf(e->angle) * spd * dt;

        /* Sea units avoid driving onto land */
        if (e->type == ENEMY_BOAT || e->type == ENEMY_CARRIER) {
            if (world_is_land(&g->world, nx, ny)) {
                e->angle += 1.4f * dt;
            } else {
                e->x = nx;
                e->y = ny;
            }
        } else {
            e->x = nx;
            e->y = ny;
        }

        if (e->x < 32.0f) e->x = 32.0f;
        if (e->x > WORLD_W - 32.0f) e->x = WORLD_W - 32.0f;
        if (e->y < 32.0f) e->y = 32.0f;
        if (e->y > WORLD_H - 32.0f) e->y = WORLD_H - 32.0f;

        /* Firing (fighters only fire forward during attack pass, not while flying past) */
        e->fire_timer -= dt;
        if (e->fire_timer <= 0.0f && dist < 440.0f &&
            (e->type != ENEMY_FIGHTER || (e->ai_timer <= 0.0f && fabsf(diff) < 0.75f))) {
            float bspd = 310.0f;
            spawn_bullet(g, 0, e->x, e->y, (dx / (dist + 1.0f)) * bspd,
                         (dy / (dist + 1.0f)) * bspd,
                         e->type == ENEMY_BOMBER ? 14 : 8, COL_RED, 1.4f);
            e->fire_timer = (e->type == ENEMY_CARRIER) ? 0.9f : 1.6f;
        }

        /* Enemy carrier periodically launches fighters */
        if (e->type == ENEMY_CARRIER) {
            e->ai_timer += dt;
            if (e->ai_timer >= 14.0f) {
                e->ai_timer = 0.0f;
                spawn_enemy(g, ENEMY_FIGHTER, e->x, e->y);
            }
        }
    }
}

static void update_projectiles(Game *g, float dt) {
    /* Bullets */
    for (int i = 0; i < MAX_BULLETS; i++) {
        Bullet *b = &g->bullets[i];
        if (!b->active) continue;
        b->x += b->vx * dt;
        b->y += b->vy * dt;
        b->life -= dt;
        if (b->life <= 0.0f || b->x < 0 || b->x >= WORLD_W || b->y < 0 || b->y >= WORLD_H) {
            b->active = 0;
            continue;
        }

        if (b->from_player) {
            for (int e = 0; e < MAX_ENEMIES; e++) {
                EnemyUnit *eu = &g->enemies[e];
                if (!eu->active) continue;
                float r = (eu->type == ENEMY_CARRIER) ? 56.0f : 24.0f;
                float dx = b->x - eu->x, dy = b->y - eu->y;
                if (dx * dx + dy * dy < r * r) {
                    b->active = 0;
                    damage_enemy(g, eu, b->damage);
                    spawn_particle(g, b->x, b->y, 0, 0, 0.2f, 2.0f, COL_YELLOW);
                    break;
                }
            }
            if (!b->active) continue;
            /* Machine gun can also lightly chip AA/Radar/Barracks */
            for (int s = 0; s < MAX_STRUCTURES; s++) {
                Structure *st = &g->structures[s];
                if (!st->active) continue;
                float dx = b->x - st->x, dy = b->y - st->y;
                if (dx * dx + dy * dy < 20.0f * 20.0f) {
                    b->active = 0;
                    if (st->type == STRUCT_NUCLEAR) {
                        set_alert_status(g, "WARNING: NUCLEAR REACTOR! DO NOT BOMB!");
                        spawn_particle(g, b->x, b->y, 0, 0, 0.2f, 2.0f, COL_CYAN);
                        break;
                    }
                    int dmg = (g->difficulty == DIFF_EASY && st->type == STRUCT_AA)
                              ? st->max_hp
                              : (st->type == STRUCT_OIL_FIELD ? b->damage : (b->damage / 2));
                    damage_structure(g, st, dmg);
                    spawn_particle(g, b->x, b->y, 0, 0, 0.2f, 2.0f, COL_ORANGE);
                    break;
                }
            }
        } else {
            /* Enemy bullet hitting player */
            float dx = b->x - g->player.x, dy = b->y - g->player.y;
            if (dx * dx + dy * dy < 18.0f * 18.0f) {
                b->active = 0;
                damage_player(g, b->damage);
                continue;
            }
            /* Enemy bullet hitting friendly carrier */
            float cx = fabsf(b->x - g->carrier.x), cy = fabsf(b->y - g->carrier.y);
            if (cx < 38.0f && cy < 88.0f) {
                b->active = 0;
                g->carrier.hp -= b->damage;
                spawn_particle(g, b->x, b->y, 0, 0, 0.25f, 3.0f, COL_ORANGE);
                if (g->carrier.warn_timer <= 0.0f) {
                    g->carrier.warn_timer = 5.0f;
                    audio_play(SFX_WARNING);
                    set_status(g, "ALERT: FRIENDLY CARRIER UNDER ATTACK!");
                }
                if (g->carrier.hp <= 0) {
                    g->carrier.hp = 0;
                    spawn_explosion(g, g->carrier.x, g->carrier.y, 60, 180.0f);
                    snprintf(g->gameover_reason, sizeof(g->gameover_reason), "FRIENDLY CARRIER SUNK!");
                    g->end_screen_timer = 1.6f;
                    g->state = STATE_GAMEOVER;
                    audio_set_rotor(0, 0.0f);
                    audio_set_gun(0);
                    audio_play(SFX_GAMEOVER);
                }
            }
        }
    }

    /* Bombs */
    for (int i = 0; i < MAX_BOMBS; i++) {
        Bomb *bm = &g->bombs[i];
        if (!bm->active) continue;
        bm->x += bm->vx * dt;
        bm->y += bm->vy * dt;
        bm->z -= bm->vz * dt;
        if (bm->z <= 0.0f) {
            bm->active = 0;
            spawn_explosion(g, bm->x, bm->y, 26, 130.0f);
            world_stamp_crater(&g->world, (int)bm->x, (int)bm->y, 14);

            /* Splash damage to structures & enemies */
            const float blast_r = 58.0f;
            for (int s = 0; s < MAX_STRUCTURES; s++) {
                Structure *st = &g->structures[s];
                if (!st->active) continue;
                float r = blast_r + (st->type == STRUCT_FACTORY ? (float)(8 + st->size * 20) : 0.0f);
                float dx = st->x - bm->x, dy = st->y - bm->y;
                if (dx * dx + dy * dy <= r * r) {
                    if (st->type == STRUCT_NUCLEAR) {
                        st->active = 0;
                        spawn_explosion(g, st->x, st->y, 90, 220.0f);
                        world_stamp_crater(&g->world, (int)st->x, (int)st->y, 44);
                        snprintf(g->gameover_reason, sizeof(g->gameover_reason),
                                 "NUCLEAR REACTOR BOMBED - MELTDOWN!");
                        g->end_screen_timer = 1.6f;
                        g->state = STATE_GAMEOVER;
                        audio_set_rotor(0, 0.0f);
                        audio_set_gun(0);
                        audio_play(SFX_GAMEOVER);
                        return;
                    }
                    damage_structure(g, st, 55);
                }
            }
            for (int e = 0; e < MAX_ENEMIES; e++) {
                EnemyUnit *eu = &g->enemies[e];
                if (!eu->active) continue;
                if (eu->type == ENEMY_FIGHTER || eu->type == ENEMY_BOMBER) continue;
                float dx = eu->x - bm->x, dy = eu->y - bm->y;
                float r = (eu->type == ENEMY_CARRIER) ? 105.0f : blast_r;
                if (dx * dx + dy * dy <= r * r) {
                    damage_enemy(g, eu, 75);
                }
            }
        }
    }

    /* Particles */
    for (int i = 0; i < MAX_PARTICLES; i++) {
        Particle *pt = &g->particles[i];
        if (!pt->active) continue;
        pt->x += pt->vx * dt;
        pt->y += pt->vy * dt;
        if (pt->kind == 1) {
            /* Drifting dark smoke expands and gradually lightens as it floats away */
            pt->size += dt * 2.2f;
            float age = 1.0f - (pt->life / pt->max_life);
            pt->col = (age < 0.40f) ? COL_BLACK : (age < 0.78f ? COL_GRAY_DARK : COL_GRAY_MID);
        } else {
            pt->vx *= (1.0f - 2.0f * dt);
            pt->vy *= (1.0f - 2.0f * dt);
        }
        pt->life -= dt;
        if (pt->life <= 0.0f) pt->active = 0;
    }
}

void game_update(Game *g, const InputState *in, float dt) {
    if (in->mute_pressed) {
        audio_toggle_mute();
    }

    if (g->quit_prompt) {
        audio_set_rotor(0, 0.0f);
        audio_set_gun(0);
        if (in->yes_pressed || in->confirm_pressed) {
            g->should_quit = 1;
        } else if (in->no_pressed || in->pause_pressed || in->quit_pressed) {
            g->quit_prompt = 0;
        }
        return;
    }

    if (in->quit_pressed) {
        g->quit_prompt = 1;
        audio_set_rotor(0, 0.0f);
        return;
    }

    g->time_sec += dt;

    if (g->state == STATE_GALLERY) {
        audio_set_rotor(0, 0.0f);
        audio_set_gun(0);
        if (in->gallery_pressed || in->pause_pressed || in->confirm_pressed) {
            g->state = g->prev_state;
        }
        return;
    }

    if (in->gallery_pressed) {
        g->prev_state = g->state;
        g->state = STATE_GALLERY;
        audio_set_rotor(0, 0.0f);
        audio_set_gun(0);
        return;
    }

    if (g->state == STATE_TITLE) {
        audio_set_rotor(0, 0.0f);
        audio_set_music(1);
        if (in->diff_delta != 0) {
            int d = ((int)g->difficulty + in->diff_delta + 3) % 3;
            g->difficulty = (Difficulty)d;
        }
        if (in->confirm_pressed) {
            audio_set_music(0);
            game_start_new(g, g->difficulty, (uint32_t)time(NULL) ^ (uint32_t)(g->time_sec * 1000.0f));
        }
        return;
    }
    audio_set_music(0);

    if (g->state == STATE_PAUSED) {
        audio_set_rotor(0, 0.0f);
        audio_set_gun(0);
        if (in->pause_pressed) g->state = STATE_PLAYING;
        return;
    }

    if (g->state == STATE_VICTORY || g->state == STATE_GAMEOVER) {
        audio_set_rotor(0, 0.0f);
        audio_set_gun(0);
        if (g->end_screen_timer > 0.0f) {
            g->end_screen_timer -= dt;
        } else if (in->confirm_pressed) {
            g->state = STATE_TITLE;
        }
        return;
    }

    if (in->pause_pressed) {
        g->state = STATE_PAUSED;
        return;
    }

    if (g->status_timer > 0.0f) g->status_timer -= dt;

    update_player(g, in, dt);
    update_carrier(g, dt);
    update_structures(g, dt);
    update_enemies(g, dt);
    update_projectiles(g, dt);

    if (g->factories_alive <= 0 && g->state == STATE_PLAYING) {
        g->end_screen_timer = 1.6f;
        g->state = STATE_VICTORY;
        audio_set_rotor(0, 0.0f);
        audio_set_gun(0);
        audio_play(SFX_VICTORY);
    }
}

void game_render(const Game *g, Framebuffer *f) {
    render_game(g, f);
}
