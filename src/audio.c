#include "audio.h"
#include <SDL2/SDL.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

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
static int g_gun_active = 0;
static float g_gun_phase = 0.0f;
static float g_gun_sub_phase = 0.0f;
static float g_gun_ramp = 0.0f;
static float g_gun_tail = 0.0f;
static int g_music_active = 0;
static int g_music_track = 0;
static float g_music_time = 0.0f;
static float g_music_mel_phase = 0.0f;
static float g_music_bass_phase = 0.0f;
static float g_music_harm_phase = 0.0f;
static float g_music_timp_phase = 0.0f;
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

        /* Soft navy/military title themes (randomly chosen on start) */
        if (g_music_active) {
            const float step_dur = (g_music_track == 0) ? 0.30f : 0.24f;
            const int num_steps = 96;
            float loop_dur = step_dur * (float)num_steps;
            g_music_time = fmodf(g_music_time + dt, loop_dur);
            int step = (int)(g_music_time / step_dur);
            if (step >= num_steps) step = num_steps - 1;
            float u = (g_music_time - (float)step * step_dur) / step_dur;

            /* Track 0: C-minor naval march (0 = rest) */
            static const float mel0_hz[96] = {
                /* Section A */
                392.00f, 0.0f,    523.25f, 523.25f, 587.33f, 622.25f, 587.33f, 523.25f,
                392.00f, 0.0f,    466.16f, 523.25f, 466.16f, 392.00f, 349.23f, 0.0f,
                311.13f, 0.0f,    392.00f, 466.16f, 523.25f, 587.33f, 622.25f, 587.33f,
                523.25f, 466.16f, 392.00f, 349.23f, 392.00f, 0.0f,    261.63f, 0.0f,
                /* Section B */
                415.30f, 0.0f,    523.25f, 622.25f, 587.33f, 0.0f,    466.16f, 0.0f,
                622.25f, 587.33f, 523.25f, 466.16f, 523.25f, 0.0f,    392.00f, 0.0f,
                415.30f, 466.16f, 523.25f, 622.25f, 698.46f, 0.0f,    622.25f, 587.33f,
                622.25f, 0.0f,    587.33f, 523.25f, 466.16f, 415.30f, 392.00f, 0.0f,
                /* Section C */
                349.23f, 0.0f,    415.30f, 523.25f, 622.25f, 587.33f, 493.88f, 392.00f,
                523.25f, 0.0f,    622.25f, 698.46f, 783.99f, 698.46f, 622.25f, 587.33f,
                783.99f, 0.0f,    622.25f, 523.25f, 587.33f, 493.88f, 392.00f, 466.16f,
                523.25f, 587.33f, 622.25f, 587.33f, 523.25f, 0.0f,    261.63f, 0.0f
            };
            static const float bass0_hz[96] = {
                /* Section A */
                130.81f, 130.81f, 98.00f,  98.00f,  103.83f, 103.83f, 98.00f,  98.00f,
                130.81f, 130.81f, 116.54f, 116.54f, 103.83f, 103.83f, 98.00f,  98.00f,
                155.56f, 155.56f, 116.54f, 116.54f, 103.83f, 103.83f, 98.00f,  98.00f,
                130.81f, 116.54f, 103.83f, 98.00f,  130.81f, 98.00f,  130.81f, 0.0f,
                /* Section B */
                103.83f, 103.83f, 155.56f, 155.56f, 116.54f, 116.54f, 116.54f, 98.00f,
                155.56f, 155.56f, 130.81f, 116.54f, 103.83f, 103.83f, 98.00f,  98.00f,
                103.83f, 103.83f, 130.81f, 130.81f, 87.31f,  87.31f,  116.54f, 116.54f,
                155.56f, 155.56f, 116.54f, 116.54f, 103.83f, 103.83f, 98.00f,  0.0f,
                /* Section C */
                87.31f,  87.31f,  103.83f, 103.83f, 98.00f,  98.00f,  123.47f, 98.00f,
                130.81f, 130.81f, 155.56f, 155.56f, 196.00f, 174.61f, 155.56f, 146.83f,
                130.81f, 130.81f, 103.83f, 103.83f, 98.00f,  98.00f,  116.54f, 116.54f,
                130.81f, 146.83f, 155.56f, 98.00f,  130.81f, 98.00f,  130.81f, 0.0f
            };
            static const float fife0_hz[96] = {
                0.0f,    0.0f,    0.0f,    0.0f,    0.0f,    0.0f,    0.0f,    0.0f,
                0.0f,    0.0f,    0.0f,    0.0f,    0.0f,    0.0f,    0.0f,    0.0f,
                0.0f,    0.0f,    0.0f,    0.0f,    0.0f,    0.0f,    0.0f,    0.0f,
                0.0f,    0.0f,    0.0f,    0.0f,    0.0f,    0.0f,    0.0f,    0.0f,
                830.61f, 622.25f, 830.61f, 1046.50f,932.33f, 783.99f, 698.46f, 0.0f,
                932.33f, 830.61f, 783.99f, 698.46f, 830.61f, 622.25f, 587.33f, 0.0f,
                830.61f, 932.33f, 1046.50f,932.33f, 830.61f, 698.46f, 932.33f, 783.99f,
                932.33f, 783.99f, 698.46f, 622.25f, 587.33f, 622.25f, 587.33f, 0.0f,
                698.46f, 830.61f, 1046.50f,830.61f, 783.99f, 698.46f, 783.99f, 587.33f,
                783.99f, 932.33f, 1046.50f,0.0f,    1046.50f,932.33f, 830.61f, 783.99f,
                1046.50f,783.99f, 830.61f, 622.25f, 783.99f, 698.46f, 587.33f, 698.46f,
                783.99f, 932.33f, 1046.50f,932.33f, 783.99f, 0.0f,    523.25f, 0.0f
            };

            /* Track 1: "High Seas Horizon" — G-major/E-minor naval anthem (-1.0f = sustain previous note) */
            static const float mel1_hz[96] = {
                /* Section A: Heroic G-major brass fanfare with sustained phrasing */
                293.66f, 392.00f, -1.0f,   493.88f, 587.33f, -1.0f,   -1.0f,   0.0f,
                523.25f, 493.88f, 440.00f, -1.0f,   392.00f, -1.0f,   -1.0f,   0.0f,
                329.63f, 392.00f, 440.00f, -1.0f,   493.88f, -1.0f,   392.00f, 0.0f,
                440.00f, 392.00f, 369.99f, -1.0f,   293.66f, -1.0f,   -1.0f,   0.0f,
                /* Section B: E-minor / C-major open-sea swell */
                329.63f, 392.00f, 493.88f, -1.0f,   659.25f, -1.0f,   -1.0f,   0.0f,
                587.33f, 493.88f, 523.25f, -1.0f,   440.00f, -1.0f,   -1.0f,   0.0f,
                493.88f, 523.25f, 587.33f, -1.0f,   659.25f, -1.0f,   587.33f, 493.88f,
                440.00f, -1.0f,   392.00f, 440.00f, 440.00f, -1.0f,   0.0f,    0.0f,
                /* Section C: C -> D7 -> G triumphant return */
                523.25f, 587.33f, 659.25f, -1.0f,   783.99f, -1.0f,   -1.0f,   0.0f,
                739.99f, 659.25f, 587.33f, -1.0f,   493.88f, -1.0f,   -1.0f,   0.0f,
                523.25f, 493.88f, 440.00f, 392.00f, 440.00f, 369.99f, 293.66f, 392.00f,
                493.88f, 587.33f, 783.99f, -1.0f,   392.00f, -1.0f,   -1.0f,   0.0f
            };
            static const float bass1_hz[96] = {
                /* Section A */
                98.00f,  146.83f, 98.00f,  146.83f, 123.47f, 146.83f, 98.00f,  146.83f,
                130.81f, 164.81f, 146.83f, 185.00f, 98.00f,  146.83f, 98.00f,  0.0f,
                164.81f, 123.47f, 110.00f, 164.81f, 98.00f,  146.83f, 123.47f, 146.83f,
                110.00f, 146.83f, 146.83f, 110.00f, 146.83f, 110.00f, 98.00f,  0.0f,
                /* Section B */
                164.81f, 123.47f, 164.81f, 123.47f, 164.81f, 196.00f, 164.81f, 123.47f,
                123.47f, 146.83f, 130.81f, 164.81f, 110.00f, 164.81f, 146.83f, 0.0f,
                98.00f,  130.81f, 146.83f, 196.00f, 130.81f, 164.81f, 123.47f, 164.81f,
                110.00f, 146.83f, 98.00f,  110.00f, 146.83f, 110.00f, 146.83f, 0.0f,
                /* Section C */
                130.81f, 196.00f, 130.81f, 196.00f, 98.00f,  146.83f, 98.00f,  146.83f,
                146.83f, 185.00f, 146.83f, 185.00f, 98.00f,  146.83f, 123.47f, 146.83f,
                130.81f, 123.47f, 110.00f, 98.00f,  146.83f, 110.00f, 146.83f, 98.00f,
                123.47f, 146.83f, 196.00f, 146.83f, 98.00f,  146.83f, 98.00f,  0.0f
            };
            static const float harm1_hz[96] = {
                /* Enters at Section B: warm brass harmony + ship's bell accents */
                0.0f,    0.0f,    0.0f,    0.0f,    0.0f,    0.0f,    0.0f,    0.0f,
                0.0f,    0.0f,    0.0f,    0.0f,    0.0f,    0.0f,    0.0f,    0.0f,
                0.0f,    0.0f,    0.0f,    0.0f,    0.0f,    0.0f,    0.0f,    0.0f,
                0.0f,    0.0f,    0.0f,    0.0f,    0.0f,    0.0f,    0.0f,    0.0f,
                246.94f, 329.63f, 392.00f, -1.0f,   493.88f, -1.0f,   -1.0f,   0.0f,
                493.88f, 392.00f, 440.00f, -1.0f,   329.63f, -1.0f,   -1.0f,   0.0f,
                392.00f, 440.00f, 493.88f, -1.0f,   523.25f, -1.0f,   493.88f, 392.00f,
                329.63f, -1.0f,   293.66f, 329.63f, 369.99f, -1.0f,   0.0f,    0.0f,
                440.00f, 493.88f, 523.25f, -1.0f,   587.33f, -1.0f,   -1.0f,   0.0f,
                587.33f, 523.25f, 493.88f, -1.0f,   392.00f, -1.0f,   -1.0f,   0.0f,
                440.00f, 392.00f, 329.63f, 293.66f, 369.99f, 293.66f, 246.94f, 293.66f,
                392.00f, 493.88f, 587.33f, -1.0f,   293.66f, -1.0f,   -1.0f,   0.0f
            };

            const float *mel_tbl  = (g_music_track == 0) ? mel0_hz  : mel1_hz;
            const float *bass_tbl = (g_music_track == 0) ? bass0_hz : bass1_hz;
            const float *harm_tbl = (g_music_track == 0) ? fife0_hz : harm1_hz;

            /* Resolve sustained (-1.0f) melody notes cleanly */
            int ms = step;
            while (ms > 0 && mel_tbl[ms] < 0.0f) ms--;
            float mf = mel_tbl[ms];
            if (mf > 0.0f) {
                int me = step;
                while (me + 1 < num_steps && mel_tbl[me + 1] < 0.0f) me++;
                int total_steps = me - ms + 1;
                float held_u = ((float)(step - ms) + u) / (float)total_steps;
                g_music_mel_phase += mf * dt;
                if (g_music_mel_phase >= 1.0f) g_music_mel_phase -= 1.0f;
                float atk = (step == ms && u < 0.08f) ? (u / 0.08f) : 1.0f;
                float rel = 1.0f - held_u * 0.50f;
                float tri = 1.0f - 4.0f * fabsf(g_music_mel_phase - 0.5f);
                float sin_t = sinf(g_music_mel_phase * 2.0f * (float)M_PI);
                mix += (tri * 0.45f + sin_t * 0.55f) * atk * rel * 0.062f;
            }

            int hs = step;
            while (hs > 0 && harm_tbl[hs] < 0.0f) hs--;
            float hf = harm_tbl[hs];
            if (hf > 0.0f) {
                int he = step;
                while (he + 1 < num_steps && harm_tbl[he + 1] < 0.0f) he++;
                int total_steps = he - hs + 1;
                float held_u = ((float)(step - hs) + u) / (float)total_steps;
                g_music_harm_phase += hf * dt;
                if (g_music_harm_phase >= 1.0f) g_music_harm_phase -= 1.0f;
                float atk = (step == hs && u < 0.12f) ? (u / 0.12f) : 1.0f;
                float rel = 1.0f - held_u * 0.65f;
                float f1 = sinf(g_music_harm_phase * 2.0f * (float)M_PI);
                float f2 = sinf(g_music_harm_phase * 4.0f * (float)M_PI);
                mix += (f1 * 0.80f + f2 * 0.20f) * atk * rel * ((g_music_track == 0) ? 0.030f : 0.034f);
            }

            float bf = bass_tbl[step];
            if (bf > 0.0f) {
                g_music_bass_phase += bf * dt;
                if (g_music_bass_phase >= 1.0f) g_music_bass_phase -= 1.0f;
                float benv = 1.0f - u * 0.55f;
                mix += sinf(g_music_bass_phase * 2.0f * (float)M_PI) * benv * 0.052f;
            }

            /* Naval timpani hit on bar downbeats & cadence starting in Section B */
            if (step >= 32 && ((step & 7) == 0 || step == 94)) {
                float timp_hz = 105.0f - 42.0f * u;
                g_music_timp_phase += timp_hz * dt;
                if (g_music_timp_phase >= 1.0f) g_music_timp_phase -= 1.0f;
                float tenv = (u < 0.05f) ? (u / 0.05f) : (1.0f - u);
                mix += sinf(g_music_timp_phase * 2.0f * (float)M_PI) * tenv * tenv * 0.065f;
            }

            /* Soft brushed snare march pulse on beats */
            if ((step & 1) == 0 || (step & 7) == 7) {
                float senv = (u < 0.25f) ? (1.0f - u * 4.0f) : 0.0f;
                mix += next_noise() * senv * senv * 0.018f;
            }
        }

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

        /* A-10 / Phalanx CIWS rotary cannon ("BRRRRRRT") with spin-up ramp */
        if (g_gun_active || g_gun_tail > 0.0f) {
            if (g_gun_active) {
                g_gun_ramp += dt * 6.5f; /* ~0.15s spin-up ramp */
                if (g_gun_ramp > 1.0f) g_gun_ramp = 1.0f;
            } else {
                g_gun_tail -= dt;
                if (g_gun_tail < 0.0f) g_gun_tail = 0.0f;
            }
            float gain = g_gun_active ? g_gun_ramp : ((g_gun_tail / 0.11f) * g_gun_ramp);

            /* Spins up from 28 Hz to 72 Hz rotary firing cycle */
            float gun_hz = 28.0f + 44.0f * g_gun_ramp;
            g_gun_phase += gun_hz * dt;
            if (g_gun_phase >= 1.0f) g_gun_phase -= 1.0f;
            g_gun_sub_phase += (24.0f + 28.0f * g_gun_ramp) * dt;
            if (g_gun_sub_phase >= 1.0f) g_gun_sub_phase -= 1.0f;

            float pulse = 1.0f - g_gun_phase;
            float snap = pulse * pulse;
            float saw = (1.0f - 2.0f * g_gun_phase);
            float buzz = saw * 0.14f + sinf(g_gun_phase * 2.0f * (float)M_PI * 2.0f) * snap * 0.10f;
            float rip = next_noise() * (0.35f + 0.65f * snap) * 0.18f;
            float sub = sinf(g_gun_sub_phase * 2.0f * (float)M_PI) * 0.10f;
            mix += (buzz + rip + sub) * gain;
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
                case SFX_PENALTY: {
                    /* Two-tone descending negative buzzer */
                    float freq = (u < 0.45f) ? 220.0f : 146.83f;
                    vc->phase += freq * dt;
                    s = (vc->phase - floorf(vc->phase) < 0.5f ? 1.0f : -1.0f) * env * 0.20f;
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
    g_noise_state ^= (uint32_t)time(NULL) ^ (uint32_t)SDL_GetTicks();
    SDL_AudioSpec want, have;
    SDL_zero(want);
    want.freq = SAMPLE_RATE;
    want.format = AUDIO_S16SYS;
    want.channels = 1;
    want.samples = 1024;
    want.callback = audio_callback;

    g_dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (g_dev == 0) {
        fprintf(stderr, "[audio] SDL_OpenAudioDevice failed: %s (continuing muted)\n", SDL_GetError());
    } else {
        fprintf(stderr, "[audio] Initialized (%d Hz, %d samples)\n", have.freq, have.samples);
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
                case SFX_PENALTY:   g_voices[i].duration = 0.36f; break;
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

void audio_set_gun(int active) {
    if (g_dev == 0) return;
    if (active && !g_gun_active) {
        g_gun_ramp = 0.0f;
        g_gun_tail = 0.0f;
    } else if (g_gun_active && !active) {
        g_gun_tail = 0.11f; /* Short bassy muzzle echo tail when burst stops */
    }
    g_gun_active = active;
}

void audio_set_music(int active) {
    if (g_dev == 0) return;
    if (active && !g_music_active) {
        g_music_time = 0.0f;
        g_music_mel_phase = 0.0f;
        g_music_bass_phase = 0.0f;
        g_music_harm_phase = 0.0f;
        g_music_timp_phase = 0.0f;
        g_music_track = (int)(((uint32_t)time(NULL) ^ (uint32_t)SDL_GetTicks() ^ g_noise_state) & 1u);
    }
    g_music_active = active;
}

void audio_toggle_mute(void) {
    g_muted = !g_muted;
}

int audio_is_muted(void) {
    return g_muted;
}
