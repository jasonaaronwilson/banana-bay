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

static void draw_structure(const Structure *s, Framebuffer *f, int cam_x, int cam_y, float time_sec) {
    int sx = (int)(s->x - cam_x);
    int sy = (int)(s->y - cam_y);
    if (sx < -64 || sx > SCREEN_W + 64 || sy < -64 || sy > SCREEN_H + 64) return;

    switch (s->type) {
        case STRUCT_FACTORY: {
            int hw = (s->size == 0) ? 15 : (s->size == 1 ? 22 : 32);
            int hh = (s->size == 0) ? 12 : (s->size == 1 ? 17 : 24);
            /* Industrial compound */
            gfx_rect(f, sx - hw, sy - hh, hw * 2, hh * 2, COL_BROWN_DARK);
            gfx_rect(f, sx - hw + 2, sy - hh + 2, hw * 2 - 4, hh * 2 - 4, COL_GRAY_MID);
            /* Sawtooth roof bays */
            int bays = s->size + 2;
            int bay_w = (hw * 2 - 8) / bays;
            for (int b = 0; b < bays; b++) {
                int rx = -hw + 4 + b * bay_w;
                gfx_rect(f, sx + rx, sy - hh + 4, bay_w - 2, hh * 2 - 10, COL_GRAY_DARK);
                gfx_line(f, sx + rx, sy - hh + 4, sx + rx + bay_w - 2, sy - hh + 4, COL_GRAY_LIGHT);
            }
            /* Smokestacks (1 for small, 2 for medium, 3 for large) */
            if (s->size == 0) {
                gfx_rect(f, sx - 3, sy - hh - 6, 6, 9, COL_RED_DARK);
            } else if (s->size == 1) {
                gfx_rect(f, sx - hw + 4, sy - hh - 6, 6, 10, COL_RED_DARK);
                gfx_rect(f, sx + hw - 10, sy - hh - 6, 6, 10, COL_RED_DARK);
            } else {
                gfx_rect(f, sx - hw + 5, sy - hh - 8, 7, 12, COL_RED_DARK);
                gfx_rect(f, sx - 3, sy - hh - 8, 7, 12, COL_RED_DARK);
                gfx_rect(f, sx + hw - 12, sy - hh - 8, 7, 12, COL_RED_DARK);
                /* Extra storage silos on large factory */
                gfx_circle(f, sx - hw + 8, sy + hh - 8, 5, COL_GRAY_LIGHT);
                gfx_circle(f, sx + hw - 8, sy + hh - 8, 5, COL_GRAY_LIGHT);
            }
            /* Level stars / status */
            for (int l = 0; l < s->level; l++) {
                gfx_rect(f, sx - (s->level * 9) / 2 + l * 9, sy + hh - 6, 6, 4, COL_ORANGE);
            }
            /* Build progress bar */
            if (s->level < 3) {
                int bar_w = hw * 2 - 6;
                int pw = (int)(s->build_progress * (float)bar_w);
                gfx_rect(f, sx - bar_w / 2, sy + hh + 2, bar_w, 3, COL_BLACK);
                gfx_rect(f, sx - bar_w / 2, sy + hh + 2, pw, 3, COL_YELLOW);
            }
            break;
        }
        case STRUCT_SHIPYARD: {
            /* Drydock / slipway */
            gfx_rect(f, sx - 24, sy - 22, 48, 44, COL_GRAY_DARK);
            gfx_rect(f, sx - 20, sy - 18, 40, 36, COL_SHALLOW_SEA);
            gfx_rect(f, sx - 24, sy - 22, 10, 44, COL_GRAY_MID);
            gfx_rect(f, sx + 14, sy - 22, 10, 44, COL_GRAY_MID);
            /* Cranes */
            gfx_rect(f, sx - 22, sy - 8, 12, 4, COL_YELLOW);
            gfx_rect(f, sx + 10, sy + 4, 12, 4, COL_YELLOW);
            if (s->building_carrier && !s->carrier_launched) {
                /* Hull under construction */
                int hull_h = (int)(6.0f + s->carrier_progress * 28.0f);
                gfx_rect(f, sx - 8, sy - hull_h / 2, 16, hull_h, COL_RED_DARK);
                gfx_rect_outline(f, sx - 8, sy - hull_h / 2, 16, hull_h, COL_ORANGE);
                int pw = (int)(s->carrier_progress * 40.0f);
                gfx_rect(f, sx - 20, sy + 24, 40, 4, COL_BLACK);
                gfx_rect(f, sx - 20, sy + 24, pw, 4, COL_RED);
            }
            break;
        }
        case STRUCT_AA: {
            /* Concrete bunker + rotating dual barrel */
            gfx_circle(f, sx, sy, 12, COL_GRAY_DARK);
            gfx_circle_outline(f, sx, sy, 12, COL_GRAY_LIGHT);
            gfx_circle(f, sx, sy, 6, COL_ROCK_DARK);
            float ang = time_sec * 1.4f + s->x * 0.01f;
            int bx = (int)(cosf(ang) * 13.0f);
            int by = (int)(sinf(ang) * 13.0f);
            gfx_line(f, sx, sy, sx + bx, sy + by, COL_YELLOW);
            break;
        }
        case STRUCT_BARRACKS: {
            /* Military tents / huts */
            gfx_rect(f, sx - 16, sy - 12, 14, 10, COL_GRASS_DARK);
            gfx_rect(f, sx + 2,  sy - 12, 14, 10, COL_GRASS_DARK);
            gfx_rect(f, sx - 16, sy + 2,  14, 10, COL_BROWN);
            gfx_rect(f, sx + 2,  sy + 2,  14, 10, COL_BROWN);
            gfx_rect_outline(f, sx - 18, sy - 14, 36, 28, COL_ROCK_DARK);
            break;
        }
        case STRUCT_RADAR: {
            gfx_rect(f, sx - 10, sy - 10, 20, 20, COL_GRAY_MID);
            gfx_circle_outline(f, sx, sy, 9, COL_CYAN);
            float ang = time_sec * 3.2f;
            gfx_line(f, sx, sy, sx + (int)(cosf(ang) * 9.0f), sy + (int)(sinf(ang) * 9.0f), COL_GREEN_BRIGHT);
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
                gfx_circle(f, tx[t] + 3, ty[t] + 3, 11, COL_BLACK);
                gfx_circle(f, tx[t], ty[t], 11, COL_GRAY_LIGHT);
                gfx_circle_outline(f, tx[t], ty[t], 11, COL_GRAY_DARK);
                gfx_circle_outline(f, tx[t], ty[t], 8, COL_GRAY_MID);
                gfx_circle(f, tx[t], ty[t], 3, COL_RED_DARK);
                gfx_pixel(f, tx[t], ty[t], COL_ORANGE);
            }
            break;
        }
    }

    /* Health bar if damaged */
    if (s->hp < s->max_hp) {
        int bw = (s->type == STRUCT_FACTORY) ? (28 + s->size * 14) : 32;
        int by = (s->type == STRUCT_FACTORY) ? (sy - (18 + s->size * 6)) : (sy - 28);
        int fill = (s->hp * bw) / s->max_hp;
        gfx_rect(f, sx - bw / 2, by, bw, 4, COL_BLACK);
        gfx_rect(f, sx - bw / 2, by, fill, 4, COL_RED);
    }
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
            gfx_circle(f, sx + 12, sy + 16, 8, COL_BLACK);
            /* Delta wing fighter (scaled up) */
            int nx = sx + (int)(c * 22.0f);
            int ny = sy + (int)(s * 22.0f);
            int lx = sx + (int)(-c * 14.0f - s * 17.0f);
            int ly = sy + (int)(-s * 14.0f + c * 17.0f);
            int rx = sx + (int)(-c * 14.0f + s * 17.0f);
            int ry = sy + (int)(-s * 14.0f - c * 17.0f);
            gfx_triangle(f, nx, ny, lx, ly, rx, ry, COL_RED);
            gfx_line(f, nx, ny, sx - (int)(c * 14.0f), sy - (int)(s * 14.0f), COL_YELLOW);
            gfx_circle(f, sx + (int)(c * 6.0f), sy + (int)(s * 6.0f), 3, COL_CYAN);
            break;
        }
        case ENEMY_BOMBER: {
            gfx_circle(f, sx + 14, sy + 18, 11, COL_BLACK);
            int nx = sx + (int)(c * 24.0f);
            int ny = sy + (int)(s * 24.0f);
            int lx = sx + (int)(-c * 12.0f - s * 26.0f);
            int ly = sy + (int)(-s * 12.0f + c * 26.0f);
            int rx = sx + (int)(-c * 12.0f + s * 26.0f);
            int ry = sy + (int)(-s * 12.0f - c * 26.0f);
            gfx_triangle(f, nx, ny, lx, ly, rx, ry, COL_ORANGE);
            gfx_circle(f, sx, sy, 6, COL_RED_DARK);
            gfx_circle(f, sx + (int)(c * 8.0f), sy + (int)(s * 8.0f), 4, COL_CYAN);
            /* Engine nacelles */
            gfx_circle(f, sx + (int)(-c * 4.0f - s * 11.0f), sy + (int)(-s * 4.0f + c * 11.0f), 3, COL_GRAY_DARK);
            gfx_circle(f, sx + (int)(-c * 4.0f + s * 11.0f), sy + (int)(-s * 4.0f - c * 11.0f), 3, COL_GRAY_DARK);
            break;
        }
        case ENEMY_BOAT: {
            /* Patrol gunboat */
            int fx = sx + (int)(c * 14.0f);
            int fy = sy + (int)(s * 14.0f);
            int bx = sx - (int)(c * 14.0f);
            int by = sy - (int)(s * 14.0f);
            gfx_pixel(f, bx - (int)(s * 5), by + (int)(c * 5), COL_FOAM);
            gfx_pixel(f, bx + (int)(s * 5), by - (int)(c * 5), COL_FOAM);
            gfx_circle(f, sx, sy, 8, COL_GRAY_DARK);
            gfx_line(f, bx, by, fx, fy, COL_RED);
            gfx_circle(f, sx, sy, 3, COL_YELLOW);
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

static void draw_player_heli(const Game *g, Framebuffer *f, int cam_x, int cam_y) {
    const PlayerHeli *p = &g->player;
    if (p->invuln_timer > 0.0f && ((int)(p->invuln_timer * 16.0f) & 1)) {
        return; /* Blink when invulnerable */
    }

    int sx = (int)(p->x - cam_x);
    int sy = (int)(p->y - cam_y);

    float c = cosf(p->angle);
    float s = sinf(p->angle);

    /* Elongated ground shadow offset */
    gfx_circle(f, sx + 12, sy + 16, 9, COL_BLACK);
    gfx_circle(f, sx + 12 + (int)(c * 9.0f), sy + 16 + (int)(s * 9.0f), 6, COL_BLACK);
    gfx_line(f, sx + 12, sy + 16,
                sx + 12 - (int)(c * 28.0f), sy + 16 - (int)(s * 28.0f), COL_BLACK);

    /* Long tail boom */
    int tx = sx - (int)(c * 32.0f);
    int ty = sy - (int)(s * 32.0f);
    gfx_line(f, sx - (int)(c * 6.0f), sy - (int)(s * 6.0f), tx, ty, COL_GRAY_LIGHT);
    gfx_line(f, sx - (int)(c * 6.0f) - (int)s, sy - (int)(s * 6.0f) + (int)c,
                tx - (int)s, ty + (int)c, COL_GRAY_MID);
    /* Tail horizontal stabilizer & tail rotor */
    int hbx = sx - (int)(c * 26.0f);
    int hby = sy - (int)(s * 26.0f);
    gfx_line(f, hbx - (int)(s * 7.0f), hby + (int)(c * 7.0f),
                hbx + (int)(s * 7.0f), hby - (int)(c * 7.0f), COL_BLUE);
    gfx_line(f, tx - (int)(s * 6.0f), ty + (int)(c * 6.0f),
                tx + (int)(s * 6.0f), ty - (int)(c * 6.0f), COL_RED);

    /* Stub wings / weapon pylons */
    gfx_line(f, sx - (int)(s * 12.0f), sy + (int)(c * 12.0f),
                sx + (int)(s * 12.0f), sy - (int)(c * 12.0f), COL_GRAY_MID);

    /* Elongated fuselage (capsule from rear engine to forward nose) */
    gfx_circle(f, sx - (int)(c * 6.0f), sy - (int)(s * 6.0f), 8, COL_BLUE);
    gfx_circle(f, sx + (int)(c * 4.0f), sy + (int)(s * 4.0f), 8, COL_BLUE);
    gfx_circle(f, sx + (int)(c * 13.0f), sy + (int)(s * 13.0f), 5, COL_BLUE);
    gfx_circle_outline(f, sx + (int)(c * 4.0f), sy + (int)(s * 4.0f), 8, COL_WHITE);
    /* Cockpit canopy */
    gfx_circle(f, sx + (int)(c * 10.0f), sy + (int)(s * 10.0f), 5, COL_CYAN);
    gfx_circle(f, sx + (int)(c * 13.0f), sy + (int)(s * 13.0f), 3, COL_WHITE);

    /* Spinning main rotor (2 perpendicular blades, larger diameter) */
    for (int b = 0; b < 2; b++) {
        float ra = p->rotor_phase + (float)b * ((float)M_PI * 0.5f);
        int rx = (int)(cosf(ra) * 28.0f);
        int ry = (int)(sinf(ra) * 28.0f);
        gfx_line(f, sx - rx, sy - ry, sx + rx, sy + ry, COL_WHITE);
    }
    gfx_circle(f, sx, sy, 3, COL_YELLOW);
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
        uint8_t col = COL_ORANGE;
        if (s->type == STRUCT_FACTORY) {
            col = COL_RED;
            if (s->size == 2) {
                gfx_rect(f, sx - 2, sy - 2, 4, 4, col);
                continue;
            }
        } else if (s->type == STRUCT_SHIPYARD) col = COL_YELLOW;
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
        int tw = gfx_text_width(2, g->status_msg);
        int bx = (SCREEN_W - tw) / 2;
        gfx_rect(f, bx - 10, 48, tw + 20, 24, COL_BLACK);
        gfx_rect_outline(f, bx - 10, 48, tw + 20, 24, COL_YELLOW);
        gfx_text(f, bx, 53, 2, COL_YELLOW, "%s", g->status_msg);
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
    gfx_text(f, 210, 526, 2, COL_GRAY_LIGHT, "PAUSE:  P or ESC   MUTE: M   QUIT: Q");

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

void render_game(const Game *g, Framebuffer *f) {
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
            draw_structure(&g->structures[i], f, cam_x, cam_y, g->time_sec);
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

    /* Player helicopter */
    draw_player_heli(g, f, cam_x, cam_y);

    /* Particles */
    for (int i = 0; i < MAX_PARTICLES; i++) {
        const Particle *pt = &g->particles[i];
        if (!pt->active) continue;
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
        gfx_text(f, (SCREEN_W - gfx_text_width(3, hdr)) / 2, 295, 3,
                 g->state == STATE_VICTORY ? COL_GREEN_BRIGHT : COL_RED, "%s", hdr);

        gfx_text(f, 330, 355, 2, COL_WHITE, "FINAL SCORE: %d", g->score);
        gfx_text(f, 330, 385, 2, COL_YELLOW, "FACTORIES DESTROYED: %d", g->factories_destroyed);

        const char *again = "PRESS ENTER TO RETURN TO TITLE";
        gfx_text(f, (SCREEN_W - gfx_text_width(2, again)) / 2, 435, 2, COL_CYAN, "%s", again);
    }

    if (g->quit_prompt) {
        draw_quit_prompt(f);
    }
}
