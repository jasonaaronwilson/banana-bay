#ifndef AUDIO_H
#define AUDIO_H

#include <stdint.h>

typedef enum {
    SFX_GUN = 0,
    SFX_BOMB_DROP,
    SFX_EXPLOSION,
    SFX_HIT,
    SFX_RESUPPLY,
    SFX_WARNING,
    SFX_VICTORY,
    SFX_GAMEOVER,
    SFX_PENALTY
} SfxType;

void audio_init(void);
void audio_shutdown(void);
void audio_play(SfxType sfx);
void audio_set_rotor(int active, float speed_ratio);
void audio_set_gun(int active);
void audio_set_music(int active);
void audio_toggle_mute(void);
int audio_is_muted(void);

#endif /* AUDIO_H */
