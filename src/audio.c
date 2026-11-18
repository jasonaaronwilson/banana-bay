#include "audio.h"
#include <SDL2/SDL.h>
#include <math.h>
#include <string.h>

#define SAMPLE_RATE 44100
#define MAX_VOICES  16

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

typedef struct {
    int active;
    SfxType type;
    float time;
    float duration;
    float phase;
} Voice;

static SDL_AudioDeviceID g_dev = 0;
static Voice g_voices[MAX_VOICES];
static int g_muted = 0;
static int g_rotor_active = 0;
static float g_rotor_speed = 0.0f;
static float g_rotor_phase = 0.0f;
static uint32_t g_noise_state = 0x12345678u;

static float next_noise(void) {
    g_noise_state = g_noise_state * 1664525u + 1013904223u;
    return ((float)((g_noise_state >> 16) & 0xFFFFu) / 32768.0f) - 1.0f;
}

static void audio_callback(void *userdata, Uint8 *stream, int len) {
    (void)userdata;
    int16_t *out = (int16_t *)stream;
    int samples = len / (int)sizeof(int16_t);

    if (g_muted) {
        memset(out, 0, (size_t)len);
        return;
    }

    const float dt = 1.0f / (float)SAMPLE_RATE;

    for (int i = 0; i < samples; i++) {
        float mix = 0.0f;

        /* Subtle helicopter rotor thump */
        if (g_rotor_active) {
            float blade_hz = 13.0f + g_rotor_speed * 5.0f;
            g_rotor_phase += blade_hz * dt;
            if (g_rotor_phase >= 1.0f) g_rotor_phase -= 1.0f;
            float env = g_rotor_phase < 0.25f ? (1.0f - g_rotor_phase * 4.0f) : 0.0f;
            float thump = sinf(g_rotor_phase * 2.0f * (float)M_PI * 2.0f) * env * 0.055f;
            float air = next_noise() * env * 0.025f;
            mix += thump + air;
        }

        /* Active SFX voices */
        for (int v = 0; v < MAX_VOICES; v++) {
            Voice *vc = &g_voices[v];
            if (!vc->active) continue;

            float t = vc->time;
            float u = t / vc->duration;
            float env = 1.0f - u;
            if (env < 0.0f) env = 0.0f;
            float s = 0.0f;

            switch (vc->type) {
                case SFX_GUN: {
                    float freq = 260.0f - 180.0f * u;
                    vc->phase += freq * dt;
                    s = (sinf(vc->phase * 2.0f * (float)M_PI) * 0.4f + next_noise() * 0.6f) * env * env * 0.24f;
                    break;
                }
                case SFX_BOMB_DROP: {
                    float freq = 750.0f - 520.0f * u;
                    vc->phase += freq * dt;
                    s = sinf(vc->phase * 2.0f * (float)M_PI) * env * 0.16f;
                    break;
                }
                case SFX_EXPLOSION: {
                    float freq = 95.0f - 65.0f * u;
                    vc->phase += freq * dt;
                    s = (sinf(vc->phase * 2.0f * (float)M_PI) * 0.45f + next_noise() * 0.75f) * env * 0.42f;
                    break;
                }
                case SFX_HIT: {
                    float freq = 520.0f - 220.0f * u;
                    vc->phase += freq * dt;
                    s = (vc->phase - floorf(vc->phase) < 0.5f ? 1.0f : -1.0f) * env * 0.15f;
                    break;
                }
                case SFX_RESUPPLY: {
                    int step = (int)(u * 4.0f);
                    static const float notes[4] = {440.0f, 554.37f, 659.25f, 880.0f};
                    float freq = notes[step < 4 ? step : 3];
                    vc->phase += freq * dt;
                    s = (sinf(vc->phase * 2.0f * (float)M_PI) > 0.0f ? 1.0f : -1.0f) * env * 0.14f;
                    break;
                }
                case SFX_WARNING: {
                    float freq = (fmodf(t, 0.20f) < 0.10f) ? 620.0f : 440.0f;
                    vc->phase += freq * dt;
                    s = sinf(vc->phase * 2.0f * (float)M_PI) * env * 0.18f;
                    break;
                }
                case SFX_VICTORY: {
                    int step = (int)(u * 5.0f);
                    static const float notes[5] = {523.25f, 659.25f, 783.99f, 1046.5f, 1318.5f};
                    float freq = notes[step < 5 ? step : 4];
                    vc->phase += freq * dt;
                    s = sinf(vc->phase * 2.0f * (float)M_PI) * env * 0.20f;
                    break;
                }
                case SFX_GAMEOVER: {
                    int step = (int)(u * 4.0f);
                    static const float notes[4] = {392.0f, 349.23f, 311.13f, 261.63f};
                    float freq = notes[step < 4 ? step : 3];
                    vc->phase += freq * dt;
                    s = (vc->phase - floorf(vc->phase) < 0.5f ? 1.0f : -1.0f) * env * 0.18f;
                    break;
                }
            }

            mix += s;
            vc->time += dt;
            if (vc->time >= vc->duration) {
                vc->active = 0;
            }
        }

        if (mix > 0.95f) mix = 0.95f;
        if (mix < -0.95f) mix = -0.95f;
        out[i] = (int16_t)(mix * 32000.0f);
    }
}

void audio_init(void) {
    SDL_AudioSpec want, have;
    SDL_zero(want);
    want.freq = SAMPLE_RATE;
    want.format = AUDIO_S16SYS;
    want.channels = 1;
    want.samples = 1024;
    want.callback = audio_callback;

    g_dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (g_dev != 0) {
        SDL_PauseAudioDevice(g_dev, 0);
    }
}

void audio_shutdown(void) {
    if (g_dev != 0) {
        SDL_CloseAudioDevice(g_dev);
        g_dev = 0;
    }
}

void audio_play(SfxType sfx) {
    if (g_dev == 0 || g_muted) return;
    SDL_LockAudioDevice(g_dev);
    for (int i = 0; i < MAX_VOICES; i++) {
        if (!g_voices[i].active) {
            g_voices[i].active = 1;
            g_voices[i].type = sfx;
            g_voices[i].time = 0.0f;
            g_voices[i].phase = 0.0f;
            switch (sfx) {
                case SFX_GUN:       g_voices[i].duration = 0.08f; break;
                case SFX_BOMB_DROP: g_voices[i].duration = 0.45f; break;
                case SFX_EXPLOSION: g_voices[i].duration = 0.55f; break;
                case SFX_HIT:       g_voices[i].duration = 0.07f; break;
                case SFX_RESUPPLY:  g_voices[i].duration = 0.40f; break;
                case SFX_WARNING:   g_voices[i].duration = 0.42f; break;
                case SFX_VICTORY:   g_voices[i].duration = 0.95f; break;
                case SFX_GAMEOVER:  g_voices[i].duration = 0.90f; break;
            }
            break;
        }
    }
    SDL_UnlockAudioDevice(g_dev);
}

void audio_set_rotor(int active, float speed_ratio) {
    if (g_dev == 0) return;
    g_rotor_active = active;
    g_rotor_speed = speed_ratio;
}

void audio_toggle_mute(void) {
    g_muted = !g_muted;
}

int audio_is_muted(void) {
    return g_muted;
}
