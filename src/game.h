#ifndef GAME_H
#define GAME_H

#include "palette.h"
#include "world.h"
#include <stdint.h>

#define MAX_STRUCTURES  192
#define MAX_ENEMIES     96
#define MAX_BULLETS     256
#define MAX_BOMBS       32
#define MAX_PARTICLES   512

#define MAX_BOMBS_CARRIED 9

typedef enum {
    DIFF_EASY = 0,
    DIFF_NORMAL = 1,
    DIFF_HARD = 2
} Difficulty;

typedef enum {
    STATE_TITLE = 0,
    STATE_PLAYING,
    STATE_PAUSED,
    STATE_VICTORY,
    STATE_GAMEOVER,
    STATE_GALLERY
} GameStateType;

typedef enum {
    STRUCT_FACTORY = 0,
    STRUCT_SHIPYARD,
    STRUCT_AA,
    STRUCT_BARRACKS,
    STRUCT_RADAR,
    STRUCT_OIL_FIELD,
    STRUCT_BUNGALOW,
    STRUCT_TOWER,
    STRUCT_NUCLEAR
} StructureType;

typedef struct {
    int active;
    StructureType type;
    float x, y;
    int hp, max_hp;
    int size;               /* 0 = small, 1 = medium, 2 = large (factories & oil fields) */
    int level;              /* 1..3 for factories */
    float build_progress;   /* 0..1 */
    float fire_timer;
    float spawn_timer;
    float smoke_timer;
    int building_carrier;   /* 1 if this shipyard is building the enemy carrier */
    float carrier_progress; /* 0..1 */
    int carrier_launched;
    float hit_x[6], hit_y[6]; /* World coords of recent hits for local damage */
    int num_hits;
} Structure;

typedef enum {
    ENEMY_FIGHTER = 0,
    ENEMY_BOMBER,
    ENEMY_BOAT,
    ENEMY_CARRIER
} EnemyType;

typedef struct {
    int active;
    EnemyType type;
    float x, y;
    float vx, vy;
    float angle;
    int hp, max_hp;
    float fire_timer;
    float ai_timer;
} EnemyUnit;

typedef struct {
    int active;
    int from_player;
    float x, y;
    float vx, vy;
    float life;
    int damage;
    uint8_t col;
} Bullet;

typedef struct {
    int active;
    float x, y;
    float vx, vy;
    float z;        /* altitude: starts at 1.0, hits ground at 0.0 */
    float vz;
} Bomb;

typedef struct {
    int active;
    float x, y;
    float vx, vy;
    float life, max_life;
    float size;
    uint8_t col;
    uint8_t kind;   /* 0 = spark/explosion, 1 = drifting smoke */
} Particle;

typedef struct {
    float x, y;
    float vx, vy;
    float angle;
    float rotor_phase;
    int hp, max_hp;
    int bombs;
    int lives;
    float gun_cooldown;
    int gun_burst;
    float bomb_cooldown;
    float invuln_timer;
    float resupply_timer;
} PlayerHeli;

typedef struct {
    float x, y;
    float vy;
    int hp, max_hp;
    float aa_timer;
    float warn_timer;
} FriendlyCarrier;

typedef struct {
    float move_x;   /* -1..1 */
    float move_y;   /* -1..1 */
    int fire_gun;
    int drop_bomb;
    int drop_bomb_pressed;
    int pause_pressed;
    int confirm_pressed;
    int diff_delta;
    int music_delta;
    int mute_pressed;
    int quit_pressed;
    int yes_pressed;
    int no_pressed;
    int gallery_pressed;
} InputState;

typedef struct {
    GameStateType state;
    GameStateType prev_state;
    Difficulty difficulty;
    World world;
    PlayerHeli player;
    FriendlyCarrier carrier;

    Structure structures[MAX_STRUCTURES];
    int num_structures;

    EnemyUnit enemies[MAX_ENEMIES];
    Bullet bullets[MAX_BULLETS];
    Bomb bombs[MAX_BOMBS];
    Particle particles[MAX_PARTICLES];

    float cam_x, cam_y;
    float time_sec;
    int score;
    int next_extra_life_score;
    int factories_initial;
    int factories_alive;
    int factories_destroyed;
    int enemy_carrier_alive;
    float factory_spawn_timer;
    char status_msg[96];
    float status_timer;
    int status_is_alert;
    char gameover_reason[96];
    float end_screen_timer;
    int quit_prompt;
    int should_quit;
} Game;

void game_init(Game *g);
void game_free(Game *g);
void game_start_new(Game *g, Difficulty diff, uint32_t seed);
void game_update(Game *g, const InputState *in, float dt);
void game_render(const Game *g, Framebuffer *f);

#endif /* GAME_H */
