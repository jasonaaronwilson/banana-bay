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
static int g_music_loops = 0;
static int g_music_songs_played = 0;
static float g_music_downtime = 0.0f;
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

        /* Soft navy/military title themes (rotates after 2 loops with 4.5s downtime) */
        if (g_music_active) {
            if (g_music_downtime > 0.0f) {
                g_music_downtime -= dt;
                if (g_music_downtime <= 0.0f) {
                    g_music_downtime = 0.0f;
                    g_music_track = (g_music_track + 1) % 8;
                    g_music_loops = 0;
                    g_music_songs_played++;
                    g_music_time = 0.0f;
                    g_music_mel_phase = 0.0f;
                    g_music_bass_phase = 0.0f;
                    g_music_harm_phase = 0.0f;
                    g_music_timp_phase = 0.0f;
                }
            } else {
            static const float step_durs[8] = {0.30f, 0.24f, 0.27f, 0.16f, 0.17f, 0.22f, 0.28f, 0.25f};
            const float step_dur = step_durs[g_music_track % 8];
            const int num_steps = 96;
            float loop_dur = step_dur * (float)num_steps;
            g_music_time += dt;
            if (g_music_time >= loop_dur) {
                g_music_time -= loop_dur;
                g_music_loops++;
                if (g_music_loops >= 2) {
                    g_music_downtime = 4.5f;
                    g_music_time = 0.0f;
                }
            }
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

            /* Track 2: "Midnight Fleet" — D-minor / F-major sweeping naval ballad-march */
            static const float mel2_hz[96] = {
                /* Section A: Stately D-minor theme */
                220.00f, 293.66f, -1.0f,   349.23f, 329.63f, 293.66f, -1.0f,   0.0f,
                261.63f, 329.63f, -1.0f,   392.00f, 349.23f, 329.63f, -1.0f,   0.0f,
                349.23f, 392.00f, 440.00f, -1.0f,   587.33f, -1.0f,   523.25f, 466.16f,
                440.00f, -1.0f,   392.00f, 349.23f, 329.63f, -1.0f,   0.0f,    0.0f,
                /* Section B: Uplifting F-major / Bb-major horizon chorus */
                349.23f, 440.00f, -1.0f,   523.25f, 466.16f, 440.00f, -1.0f,   0.0f,
                392.00f, 466.16f, -1.0f,   587.33f, 523.25f, 466.16f, -1.0f,   0.0f,
                440.00f, 523.25f, 698.46f, -1.0f,   659.25f, 587.33f, 523.25f, 466.16f,
                440.00f, -1.0f,   392.00f, 349.23f, 392.00f, -1.0f,   0.0f,    0.0f,
                /* Section C: Gm -> A7 -> Dm climax & resolution */
                392.00f, 466.16f, 587.33f, -1.0f,   523.25f, 466.16f, 440.00f, -1.0f,
                349.23f, 392.00f, 440.00f, -1.0f,   554.37f, -1.0f,   440.00f, 0.0f,
                587.33f, 523.25f, 466.16f, 440.00f, 392.00f, 349.23f, 329.63f, 277.18f,
                293.66f, 349.23f, 440.00f, 587.33f, 293.66f, -1.0f,   -1.0f,   0.0f
            };
            static const float bass2_hz[96] = {
                /* Section A */
                73.42f,  110.00f, 146.83f, 110.00f, 73.42f,  110.00f, 146.83f, 110.00f,
                130.81f, 98.00f,  130.81f, 98.00f,  116.54f, 87.31f,  110.00f, 0.0f,
                87.31f,  130.81f, 110.00f, 146.83f, 116.54f, 146.83f, 130.81f, 116.54f,
                110.00f, 164.81f, 110.00f, 146.83f, 110.00f, 164.81f, 110.00f, 0.0f,
                /* Section B */
                87.31f,  130.81f, 174.61f, 130.81f, 87.31f,  130.81f, 174.61f, 130.81f,
                98.00f,  146.83f, 116.54f, 146.83f, 98.00f,  146.83f, 116.54f, 0.0f,
                87.31f,  130.81f, 174.61f, 130.81f, 130.81f, 116.54f, 110.00f, 98.00f,
                110.00f, 146.83f, 130.81f, 116.54f, 110.00f, 164.81f, 110.00f, 0.0f,
                /* Section C */
                98.00f,  146.83f, 98.00f,  146.83f, 130.81f, 116.54f, 110.00f, 164.81f,
                87.31f,  130.81f, 110.00f, 146.83f, 110.00f, 164.81f, 110.00f, 0.0f,
                116.54f, 130.81f, 146.83f, 110.00f, 98.00f,  87.31f,  110.00f, 164.81f,
                73.42f,  110.00f, 146.83f, 110.00f, 73.42f,  110.00f, 73.42f,  0.0f
            };
            static const float harm2_hz[96] = {
                /* Enters at Section B: warm third/sixth harmony */
                0.0f,    0.0f,    0.0f,    0.0f,    0.0f,    0.0f,    0.0f,    0.0f,
                0.0f,    0.0f,    0.0f,    0.0f,    0.0f,    0.0f,    0.0f,    0.0f,
                0.0f,    0.0f,    0.0f,    0.0f,    0.0f,    0.0f,    0.0f,    0.0f,
                0.0f,    0.0f,    0.0f,    0.0f,    0.0f,    0.0f,    0.0f,    0.0f,
                261.63f, 349.23f, -1.0f,   440.00f, 392.00f, 349.23f, -1.0f,   0.0f,
                293.66f, 392.00f, -1.0f,   466.16f, 440.00f, 392.00f, -1.0f,   0.0f,
                349.23f, 440.00f, 523.25f, -1.0f,   523.25f, 466.16f, 440.00f, 392.00f,
                349.23f, -1.0f,   329.63f, 293.66f, 329.63f, -1.0f,   0.0f,    0.0f,
                293.66f, 392.00f, 466.16f, -1.0f,   440.00f, 392.00f, 349.23f, -1.0f,
                261.63f, 329.63f, 349.23f, -1.0f,   440.00f, -1.0f,   329.63f, 0.0f,
                466.16f, 440.00f, 392.00f, 349.23f, 293.66f, 261.63f, 220.00f, 220.00f,
                220.00f, 293.66f, 349.23f, 440.00f, 220.00f, -1.0f,   -1.0f,   0.0f
            };

            /* Track 3: "Pacific Hornpipe" — A-minor / C-major brisk maritime march */
            static const float mel3_hz[96] = {
                /* Section A */
                329.63f, 440.00f, 523.25f, 493.88f, 440.00f, -1.0f,   329.63f, 0.0f,
                392.00f, 493.88f, 587.33f, 523.25f, 493.88f, -1.0f,   392.00f, 0.0f,
                523.25f, 587.33f, 659.25f, 587.33f, 523.25f, 493.88f, 440.00f, 392.00f,
                329.63f, 440.00f, 493.88f, 415.30f, 440.00f, -1.0f,   0.0f,    0.0f,
                /* Section B */
                523.25f, 659.25f, 783.99f, -1.0f,   659.25f, 587.33f, 523.25f, 0.0f,
                587.33f, 698.46f, 880.00f, -1.0f,   698.46f, 659.25f, 587.33f, 0.0f,
                659.25f, 783.99f, 523.25f, 587.33f, 659.25f, 523.25f, 493.88f, 392.00f,
                440.00f, 493.88f, 523.25f, 392.00f, 440.00f, -1.0f,   0.0f,    0.0f,
                /* Section C */
                587.33f, 698.46f, 659.25f, 587.33f, 523.25f, 493.88f, 440.00f, 0.0f,
                493.88f, 659.25f, 830.61f, 659.25f, 587.33f, 523.25f, 493.88f, 0.0f,
                440.00f, 523.25f, 659.25f, 783.99f, 659.25f, 587.33f, 523.25f, 493.88f,
                440.00f, 329.63f, 415.30f, 493.88f, 440.00f, -1.0f,   -1.0f,   0.0f
            };
            static const float bass3_hz[96] = {
                110.00f, 164.81f, 110.00f, 164.81f, 110.00f, 164.81f, 110.00f, 164.81f,
                98.00f,  146.83f, 98.00f,  146.83f, 98.00f,  146.83f, 98.00f,  146.83f,
                130.81f, 196.00f, 130.81f, 196.00f, 110.00f, 164.81f, 98.00f,  146.83f,
                110.00f, 146.83f, 164.81f, 207.65f, 110.00f, 164.81f, 110.00f, 0.0f,
                130.81f, 196.00f, 130.81f, 196.00f, 130.81f, 196.00f, 130.81f, 196.00f,
                146.83f, 174.61f, 146.83f, 174.61f, 146.83f, 174.61f, 98.00f,  146.83f,
                130.81f, 196.00f, 130.81f, 196.00f, 130.81f, 164.81f, 98.00f,  146.83f,
                110.00f, 123.47f, 130.81f, 98.00f,  110.00f, 164.81f, 110.00f, 0.0f,
                146.83f, 174.61f, 146.83f, 174.61f, 130.81f, 123.47f, 110.00f, 164.81f,
                164.81f, 207.65f, 164.81f, 207.65f, 146.83f, 130.81f, 123.47f, 164.81f,
                110.00f, 130.81f, 164.81f, 196.00f, 164.81f, 146.83f, 130.81f, 123.47f,
                110.00f, 164.81f, 207.65f, 164.81f, 110.00f, 164.81f, 110.00f, 0.0f
            };
            static const float harm3_hz[96] = {
                0.0f,    0.0f,    0.0f,    0.0f,    0.0f,    0.0f,    0.0f,    0.0f,
                0.0f,    0.0f,    0.0f,    0.0f,    0.0f,    0.0f,    0.0f,    0.0f,
                0.0f,    0.0f,    0.0f,    0.0f,    0.0f,    0.0f,    0.0f,    0.0f,
                0.0f,    0.0f,    0.0f,    0.0f,    0.0f,    0.0f,    0.0f,    0.0f,
                392.00f, 523.25f, 659.25f, -1.0f,   523.25f, 493.88f, 392.00f, 0.0f,
                440.00f, 587.33f, 698.46f, -1.0f,   587.33f, 523.25f, 493.88f, 0.0f,
                523.25f, 659.25f, 392.00f, 440.00f, 523.25f, 392.00f, 392.00f, 293.66f,
                329.63f, 392.00f, 440.00f, 293.66f, 329.63f, -1.0f,   0.0f,    0.0f,
                440.00f, 587.33f, 523.25f, 440.00f, 392.00f, 349.23f, 329.63f, 0.0f,
                329.63f, 493.88f, 659.25f, 493.88f, 440.00f, 392.00f, 329.63f, 0.0f,
                329.63f, 440.00f, 523.25f, 659.25f, 523.25f, 440.00f, 392.00f, 329.63f,
                329.63f, 261.63f, 329.63f, 392.00f, 329.63f, -1.0f,   -1.0f,   0.0f
            };

            /* Track 4: "Ironclad Patrol" — E-minor / G-major heavy destroyer march */
            static const float mel4_hz[96] = {
                /* Section A */
                246.94f, 329.63f, -1.0f,   392.00f, 369.99f, 329.63f, -1.0f,   0.0f,
                293.66f, 369.99f, -1.0f,   440.00f, 392.00f, 369.99f, -1.0f,   0.0f,
                329.63f, 392.00f, 493.88f, -1.0f,   659.25f, 587.33f, 493.88f, 440.00f,
                392.00f, -1.0f,   369.99f, 329.63f, 293.66f, -1.0f,   0.0f,    0.0f,
                /* Section B */
                392.00f, 493.88f, -1.0f,   587.33f, 523.25f, 493.88f, -1.0f,   0.0f,
                523.25f, 659.25f, -1.0f,   783.99f, 739.99f, 659.25f, -1.0f,   0.0f,
                587.33f, 523.25f, 493.88f, 440.00f, 392.00f, 440.00f, 493.88f, 392.00f,
                440.00f, -1.0f,   369.99f, 293.66f, 392.00f, -1.0f,   0.0f,    0.0f,
                /* Section C */
                440.00f, 523.25f, 659.25f, -1.0f,   587.33f, 523.25f, 493.88f, -1.0f,
                493.88f, 622.25f, 739.99f, -1.0f,   659.25f, 587.33f, 493.88f, 0.0f,
                659.25f, 587.33f, 523.25f, 493.88f, 440.00f, 392.00f, 369.99f, 311.13f,
                329.63f, 392.00f, 493.88f, 659.25f, 329.63f, -1.0f,   -1.0f,   0.0f
            };
            static const float bass4_hz[96] = {
                82.41f,  123.47f, 164.81f, 123.47f, 82.41f,  123.47f, 164.81f, 123.47f,
                146.83f, 110.00f, 146.83f, 185.00f, 98.00f,  146.83f, 123.47f, 0.0f,
                82.41f,  123.47f, 164.81f, 196.00f, 130.81f, 146.83f, 123.47f, 110.00f,
                98.00f,  146.83f, 110.00f, 130.81f, 146.83f, 110.00f, 146.83f, 0.0f,
                98.00f,  146.83f, 196.00f, 146.83f, 98.00f,  146.83f, 196.00f, 146.83f,
                130.81f, 164.81f, 196.00f, 164.81f, 130.81f, 164.81f, 196.00f, 0.0f,
                146.83f, 130.81f, 123.47f, 110.00f, 98.00f,  110.00f, 123.47f, 98.00f,
                110.00f, 146.83f, 185.00f, 146.83f, 98.00f,  146.83f, 98.00f,  0.0f,
                110.00f, 164.81f, 130.81f, 164.81f, 146.83f, 130.81f, 123.47f, 164.81f,
                123.47f, 185.00f, 123.47f, 185.00f, 164.81f, 146.83f, 123.47f, 0.0f,
                130.81f, 146.83f, 164.81f, 123.47f, 110.00f, 98.00f,  185.00f, 123.47f,
                82.41f,  123.47f, 164.81f, 123.47f, 82.41f,  123.47f, 82.41f,  0.0f
            };
            static const float harm4_hz[96] = {
                0.0f,    0.0f,    0.0f,    0.0f,    0.0f,    0.0f,    0.0f,    0.0f,
                0.0f,    0.0f,    0.0f,    0.0f,    0.0f,    0.0f,    0.0f,    0.0f,
                0.0f,    0.0f,    0.0f,    0.0f,    0.0f,    0.0f,    0.0f,    0.0f,
                0.0f,    0.0f,    0.0f,    0.0f,    0.0f,    0.0f,    0.0f,    0.0f,
                293.66f, 392.00f, -1.0f,   493.88f, 440.00f, 392.00f, -1.0f,   0.0f,
                392.00f, 523.25f, -1.0f,   659.25f, 587.33f, 523.25f, -1.0f,   0.0f,
                493.88f, 440.00f, 392.00f, 329.63f, 293.66f, 329.63f, 392.00f, 293.66f,
                329.63f, -1.0f,   293.66f, 220.00f, 293.66f, -1.0f,   0.0f,    0.0f,
                329.63f, 440.00f, 523.25f, -1.0f,   493.88f, 440.00f, 392.00f, -1.0f,
                369.99f, 493.88f, 622.25f, -1.0f,   493.88f, 440.00f, 369.99f, 0.0f,
                523.25f, 493.88f, 440.00f, 392.00f, 329.63f, 293.66f, 246.94f, 246.94f,
                246.94f, 329.63f, 392.00f, 493.88f, 246.94f, -1.0f,   -1.0f,   0.0f
            };

            /* Track 5: "Coral Dawn" — Bb-major / G-minor tropical naval patrol */
            static const float mel5_hz[96] = {
                /* Section A */
                349.23f, 466.16f, -1.0f,   587.33f, 523.25f, 466.16f, -1.0f,   0.0f,
                392.00f, 466.16f, 523.25f, -1.0f,   622.25f, 587.33f, 523.25f, 0.0f,
                587.33f, 698.46f, 587.33f, 523.25f, 466.16f, 392.00f, 349.23f, 0.0f,
                392.00f, 440.00f, 466.16f, 349.23f, 466.16f, -1.0f,   0.0f,    0.0f,
                /* Section B */
                392.00f, 466.16f, 587.33f, -1.0f,   783.99f, -1.0f,   698.46f, 0.0f,
                622.25f, 587.33f, 523.25f, -1.0f,   466.16f, -1.0f,   392.00f, 0.0f,
                523.25f, 587.33f, 622.25f, 698.46f, 783.99f, 698.46f, 622.25f, 587.33f,
                523.25f, 466.16f, 440.00f, 349.23f, 466.16f, -1.0f,   0.0f,    0.0f,
                /* Section C */
                622.25f, 698.46f, 783.99f, -1.0f,   698.46f, 622.25f, 587.33f, 0.0f,
                523.25f, 587.33f, 622.25f, -1.0f,   587.33f, 523.25f, 466.16f, 0.0f,
                466.16f, 523.25f, 587.33f, 698.46f, 587.33f, 523.25f, 440.00f, 349.23f,
                466.16f, 587.33f, 698.46f, 587.33f, 466.16f, -1.0f,   -1.0f,   0.0f
            };
            static const float bass5_hz[96] = {
                116.54f, 174.61f, 116.54f, 174.61f, 116.54f, 174.61f, 116.54f, 174.61f,
                98.00f,  146.83f, 130.81f, 196.00f, 155.56f, 146.83f, 130.81f, 0.0f,
                116.54f, 174.61f, 146.83f, 130.81f, 116.54f, 98.00f,  87.31f,  0.0f,
                98.00f,  110.00f, 116.54f, 87.31f,  116.54f, 174.61f, 116.54f, 0.0f,
                98.00f,  146.83f, 98.00f,  146.83f, 98.00f,  146.83f, 98.00f,  0.0f,
                155.56f, 146.83f, 130.81f, 196.00f, 116.54f, 174.61f, 98.00f,  0.0f,
                130.81f, 146.83f, 155.56f, 174.61f, 196.00f, 174.61f, 155.56f, 146.83f,
                130.81f, 116.54f, 110.00f, 87.31f,  116.54f, 174.61f, 116.54f, 0.0f,
                155.56f, 174.61f, 196.00f, 155.56f, 174.61f, 155.56f, 146.83f, 0.0f,
                130.81f, 146.83f, 155.56f, 196.00f, 146.83f, 130.81f, 116.54f, 0.0f,
                116.54f, 130.81f, 146.83f, 174.61f, 146.83f, 130.81f, 110.00f, 87.31f,
                116.54f, 146.83f, 174.61f, 146.83f, 116.54f, 174.61f, 116.54f, 0.0f
            };
            static const float harm5_hz[96] = {
                0.0f,    0.0f,    0.0f,    0.0f,    0.0f,    0.0f,    0.0f,    0.0f,
                0.0f,    0.0f,    0.0f,    0.0f,    0.0f,    0.0f,    0.0f,    0.0f,
                0.0f,    0.0f,    0.0f,    0.0f,    0.0f,    0.0f,    0.0f,    0.0f,
                0.0f,    0.0f,    0.0f,    0.0f,    0.0f,    0.0f,    0.0f,    0.0f,
                293.66f, 392.00f, 466.16f, -1.0f,   587.33f, -1.0f,   523.25f, 0.0f,
                466.16f, 440.00f, 392.00f, -1.0f,   349.23f, -1.0f,   293.66f, 0.0f,
                392.00f, 440.00f, 466.16f, 523.25f, 587.33f, 523.25f, 466.16f, 440.00f,
                392.00f, 349.23f, 349.23f, 261.63f, 349.23f, -1.0f,   0.0f,    0.0f,
                466.16f, 523.25f, 587.33f, -1.0f,   523.25f, 466.16f, 440.00f, 0.0f,
                392.00f, 440.00f, 466.16f, -1.0f,   440.00f, 392.00f, 349.23f, 0.0f,
                349.23f, 392.00f, 466.16f, 523.25f, 466.16f, 392.00f, 349.23f, 261.63f,
                349.23f, 466.16f, 523.25f, 466.16f, 349.23f, -1.0f,   -1.0f,   0.0f
            };

            /* Track 3 (middle slot): "Top Gun Reef" — E-minor / G-major driving arcade rock */
            static const float mel_rock1_hz[96] = {
                /* Section A: Driving E-minor guitar riff */
                329.63f, 329.63f, 392.00f, 440.00f, 493.88f, -1.0f,   440.00f, 392.00f,
                329.63f, 293.66f, 329.63f, -1.0f,   329.63f, 0.0f,    392.00f, 440.00f,
                493.88f, 493.88f, 587.33f, 659.25f, 587.33f, -1.0f,   493.88f, 440.00f,
                392.00f, 440.00f, 369.99f, -1.0f,   329.63f, -1.0f,   0.0f,    0.0f,
                /* Section B: C -> D -> G soaring chorus */
                523.25f, 523.25f, 659.25f, 587.33f, 523.25f, 493.88f, 440.00f, 0.0f,
                587.33f, 587.33f, 739.99f, 659.25f, 587.33f, 523.25f, 493.88f, 0.0f,
                392.00f, 493.88f, 587.33f, 783.99f, 739.99f, 659.25f, 587.33f, 493.88f,
                440.00f, 493.88f, 523.25f, 587.33f, 493.88f, -1.0f,   0.0f,    0.0f,
                /* Section C: Am -> B7 -> Em solo & turnaround */
                440.00f, 523.25f, 659.25f, 523.25f, 440.00f, 523.25f, 659.25f, 783.99f,
                739.99f, 659.25f, 622.25f, 493.88f, 622.25f, 739.99f, 622.25f, 493.88f,
                659.25f, 587.33f, 523.25f, 493.88f, 440.00f, 392.00f, 369.99f, 311.13f,
                329.63f, 493.88f, 659.25f, -1.0f,   329.63f, -1.0f,   0.0f,    0.0f
            };
            static const float bass_rock1_hz[96] = {
                82.41f,  82.41f,  82.41f,  82.41f,  123.47f, 123.47f, 110.00f, 98.00f,
                82.41f,  82.41f,  73.42f,  82.41f,  82.41f,  82.41f,  98.00f,  110.00f,
                123.47f, 123.47f, 146.83f, 164.81f, 146.83f, 146.83f, 123.47f, 110.00f,
                98.00f,  110.00f, 92.50f,  123.47f, 82.41f,  82.41f,  82.41f,  0.0f,
                130.81f, 130.81f, 130.81f, 130.81f, 130.81f, 123.47f, 110.00f, 98.00f,
                146.83f, 146.83f, 146.83f, 146.83f, 146.83f, 130.81f, 123.47f, 110.00f,
                98.00f,  98.00f,  146.83f, 196.00f, 185.00f, 164.81f, 146.83f, 123.47f,
                110.00f, 123.47f, 130.81f, 146.83f, 123.47f, 123.47f, 123.47f, 0.0f,
                110.00f, 110.00f, 110.00f, 110.00f, 110.00f, 130.81f, 164.81f, 130.81f,
                123.47f, 123.47f, 123.47f, 123.47f, 155.56f, 185.00f, 155.56f, 123.47f,
                130.81f, 123.47f, 110.00f, 98.00f,  110.00f, 98.00f,  92.50f,  123.47f,
                82.41f,  123.47f, 164.81f, 123.47f, 82.41f,  82.41f,  82.41f,  0.0f
            };
            static const float harm_rock1_hz[96] = {
                0.0f,    0.0f,    0.0f,    0.0f,    0.0f,    0.0f,    0.0f,    0.0f,
                0.0f,    0.0f,    0.0f,    0.0f,    0.0f,    0.0f,    0.0f,    0.0f,
                0.0f,    0.0f,    0.0f,    0.0f,    0.0f,    0.0f,    0.0f,    0.0f,
                0.0f,    0.0f,    0.0f,    0.0f,    0.0f,    0.0f,    0.0f,    0.0f,
                329.63f, 329.63f, 392.00f, 392.00f, 329.63f, 329.63f, 293.66f, 0.0f,
                369.99f, 369.99f, 440.00f, 440.00f, 369.99f, 369.99f, 329.63f, 0.0f,
                293.66f, 392.00f, 493.88f, 587.33f, 523.25f, 493.88f, 392.00f, 329.63f,
                329.63f, 369.99f, 392.00f, 440.00f, 369.99f, -1.0f,   0.0f,    0.0f,
                329.63f, 329.63f, 440.00f, 440.00f, 329.63f, 440.00f, 523.25f, 523.25f,
                493.88f, 493.88f, 440.00f, 369.99f, 493.88f, 587.33f, 493.88f, 369.99f,
                440.00f, 392.00f, 369.99f, 329.63f, 293.66f, 246.94f, 246.94f, 246.94f,
                246.94f, 329.63f, 493.88f, -1.0f,   246.94f, -1.0f,   0.0f,    0.0f
            };

            /* Track 4 (middle slot): "Danger Zone Bay" — A-minor / C-major upbeat surf-rock */
            static const float mel_rock2_hz[96] = {
                /* Section A */
                440.00f, 523.25f, 659.25f, 587.33f, 523.25f, 440.00f, 392.00f, 0.0f,
                392.00f, 493.88f, 587.33f, 523.25f, 493.88f, 392.00f, 329.63f, 0.0f,
                349.23f, 440.00f, 523.25f, 659.25f, 587.33f, 523.25f, 493.88f, 440.00f,
                493.88f, 415.30f, 329.63f, 493.88f, 440.00f, -1.0f,   0.0f,    0.0f,
                /* Section B */
                698.46f, 698.46f, 659.25f, 587.33f, 523.25f, 440.00f, 349.23f, 0.0f,
                587.33f, 587.33f, 523.25f, 493.88f, 392.00f, 493.88f, 587.33f, 0.0f,
                523.25f, 659.25f, 783.99f, 659.25f, 523.25f, 587.33f, 659.25f, 523.25f,
                440.00f, 523.25f, 659.25f, 587.33f, 523.25f, -1.0f,   0.0f,    0.0f,
                /* Section C */
                587.33f, 659.25f, 698.46f, 587.33f, 783.99f, 698.46f, 659.25f, 587.33f,
                523.25f, 587.33f, 659.25f, 783.99f, 659.25f, 523.25f, 440.00f, 0.0f,
                493.88f, 659.25f, 830.61f, 659.25f, 587.33f, 523.25f, 493.88f, 415.30f,
                440.00f, 523.25f, 659.25f, 880.00f, 440.00f, -1.0f,   0.0f,    0.0f
            };
            static const float bass_rock2_hz[96] = {
                110.00f, 110.00f, 164.81f, 110.00f, 110.00f, 164.81f, 110.00f, 110.00f,
                98.00f,  98.00f,  146.83f, 98.00f,  98.00f,  146.83f, 98.00f,  98.00f,
                87.31f,  87.31f,  130.81f, 87.31f,  130.81f, 110.00f, 98.00f,  87.31f,
                123.47f, 103.83f, 82.41f,  123.47f, 110.00f, 110.00f, 110.00f, 0.0f,
                87.31f,  87.31f,  174.61f, 87.31f,  130.81f, 110.00f, 87.31f,  87.31f,
                98.00f,  98.00f,  196.00f, 98.00f,  146.83f, 123.47f, 98.00f,  98.00f,
                130.81f, 130.81f, 196.00f, 130.81f, 130.81f, 146.83f, 164.81f, 130.81f,
                110.00f, 110.00f, 164.81f, 146.83f, 130.81f, 110.00f, 110.00f, 0.0f,
                146.83f, 146.83f, 174.61f, 146.83f, 98.00f,  98.00f,  146.83f, 98.00f,
                130.81f, 130.81f, 196.00f, 130.81f, 164.81f, 130.81f, 110.00f, 110.00f,
                164.81f, 164.81f, 207.65f, 164.81f, 146.83f, 130.81f, 123.47f, 103.83f,
                110.00f, 130.81f, 164.81f, 110.00f, 110.00f, 110.00f, 110.00f, 0.0f
            };
            static const float harm_rock2_hz[96] = {
                0.0f,    0.0f,    0.0f,    0.0f,    0.0f,    0.0f,    0.0f,    0.0f,
                0.0f,    0.0f,    0.0f,    0.0f,    0.0f,    0.0f,    0.0f,    0.0f,
                0.0f,    0.0f,    0.0f,    0.0f,    0.0f,    0.0f,    0.0f,    0.0f,
                0.0f,    0.0f,    0.0f,    0.0f,    0.0f,    0.0f,    0.0f,    0.0f,
                440.00f, 440.00f, 523.25f, 440.00f, 349.23f, 349.23f, 261.63f, 0.0f,
                392.00f, 392.00f, 493.88f, 392.00f, 293.66f, 392.00f, 493.88f, 0.0f,
                392.00f, 523.25f, 659.25f, 523.25f, 392.00f, 440.00f, 523.25f, 392.00f,
                329.63f, 440.00f, 523.25f, 493.88f, 440.00f, -1.0f,   0.0f,    0.0f,
                440.00f, 523.25f, 587.33f, 440.00f, 587.33f, 493.88f, 440.00f, 392.00f,
                392.00f, 440.00f, 523.25f, 659.25f, 523.25f, 392.00f, 329.63f, 0.0f,
                329.63f, 493.88f, 659.25f, 493.88f, 440.00f, 392.00f, 329.63f, 246.94f,
                329.63f, 440.00f, 523.25f, 659.25f, 329.63f, -1.0f,   0.0f,    0.0f
            };

            const float *mel_tbls[8]  = {mel0_hz,  mel1_hz,  mel2_hz,  mel_rock1_hz,  mel_rock2_hz,  mel3_hz,  mel4_hz,  mel5_hz};
            const float *bass_tbls[8] = {bass0_hz, bass1_hz, bass2_hz, bass_rock1_hz, bass_rock2_hz, bass3_hz, bass4_hz, bass5_hz};
            const float *harm_tbls[8] = {fife0_hz, harm1_hz, harm2_hz, harm_rock1_hz, harm_rock2_hz, harm3_hz, harm4_hz, harm5_hz};
            int trk = g_music_track % 8;
            const float *mel_tbl  = mel_tbls[trk];
            const float *bass_tbl = bass_tbls[trk];
            const float *harm_tbl = harm_tbls[trk];

            /* Per-track instrument palette (rock tracks 3 & 4 sit right in the middle):
               0: Horn + Fife + Sine Tuba + Snare
               1: Bright Trumpet + Brass Harmony + Plucked Bass + Snare
               2: Sea Accordion (vibrato reed) + String Pad + Deep Bass + Shaker
               3: Overdriven Rock Guitar + Power-Chord Organ + Driving Rock Bass + Rock Kit
               4: Surf/Arcade Electric Guitar + Harmony Guitar + Driving Bass + Rock Kit
               5: Pennywhistle / Flute + Plucked Harp + Octave Bass + Shaker
               6: Heavy Low Brass + French Horn + Electric Pulse Bass + Snare
               7: Steel Drum / Marimba + Bell Chimes + Warm Bass + Shaker */
            int ms = step;
            while (ms > 0 && mel_tbl[ms] < 0.0f) ms--;
            float mf = mel_tbl[ms];
            if (mf > 0.0f) {
                int me = step;
                while (me + 1 < num_steps && mel_tbl[me + 1] < 0.0f) me++;
                int total_steps = me - ms + 1;
                float held_u = ((float)(step - ms) + u) / (float)total_steps;
                float vib = (trk == 2 || trk == 5) ? (1.0f + 0.005f * sinf(g_music_time * 28.0f)) : 1.0f;
                g_music_mel_phase += mf * vib * dt;
                if (g_music_mel_phase >= 1.0f) g_music_mel_phase -= 1.0f;
                float atk = (step == ms && u < 0.06f) ? (u / 0.06f) : 1.0f;
                float rel = 1.0f - held_u * 0.50f;
                float ph = g_music_mel_phase;
                float tri = 1.0f - 4.0f * fabsf(ph - 0.5f);
                float sin_t = sinf(ph * 2.0f * (float)M_PI);
                float saw = 1.0f - 2.0f * ph;
                float sq = (ph < 0.5f) ? 1.0f : -1.0f;
                float tone = 0.0f;
                if (trk == 0) {
                    tone = tri * 0.45f + sin_t * 0.55f; /* Warm naval horn */
                } else if (trk == 1) {
                    tone = saw * 0.35f + sin_t * 0.45f + sinf(ph * 4.0f * (float)M_PI) * 0.20f; /* Bright trumpet */
                } else if (trk == 2) {
                    tone = sq * 0.25f + tri * 0.55f + sin_t * 0.20f; /* Sea accordion / reed */
                } else if (trk == 3 || trk == 4) {
                    /* Overdriven electric rock guitar (soft-clipped saw+pulse) */
                    float raw = saw * 0.55f + sq * 0.35f + sinf(ph * 4.0f * (float)M_PI) * 0.25f;
                    tone = tanhf(raw * 1.6f);
                } else if (trk == 5) {
                    tone = sin_t * 0.82f + sinf(ph * 4.0f * (float)M_PI) * 0.18f; /* Pennywhistle / flute */
                } else if (trk == 6) {
                    tone = saw * 0.45f + tri * 0.40f + sinf(ph * (float)M_PI) * 0.15f; /* Heavy destroyer brass */
                } else {
                    float perc = expf(-held_u * 3.8f);
                    tone = (sin_t * 0.72f + sinf(ph * 6.0f * (float)M_PI) * 0.28f * (1.0f - held_u)) * perc; /* Steel drum / marimba */
                    rel = 1.0f;
                }
                mix += tone * atk * rel * ((trk == 6) ? 0.056f : (trk == 3 || trk == 4 ? 0.054f : 0.062f));
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
                float atk = (step == hs && u < 0.10f) ? (u / 0.10f) : 1.0f;
                float rel = (trk == 5 || trk == 7) ? expf(-held_u * 3.2f) : (1.0f - held_u * 0.65f);
                float hph = g_music_harm_phase;
                float f1 = sinf(hph * 2.0f * (float)M_PI);
                float f2 = sinf(hph * 4.0f * (float)M_PI);
                float htone = (trk == 2) ? (1.0f - 4.0f * fabsf(hph - 0.5f))
                            : ((trk == 3 || trk == 4) ? ((hph < 0.35f) ? 0.7f : -0.7f) : (f1 * 0.80f + f2 * 0.20f));
                mix += htone * atk * rel * ((trk == 3 || trk == 4) ? 0.026f : 0.032f);
            }

            float bf = bass_tbl[step];
            if (bf > 0.0f) {
                g_music_bass_phase += bf * dt;
                if (g_music_bass_phase >= 1.0f) g_music_bass_phase -= 1.0f;
                float benv = (trk == 1 || trk == 3 || trk == 4 || trk == 5) ? expf(-u * 2.6f) : (1.0f - u * 0.55f);
                float bsin = sinf(g_music_bass_phase * 2.0f * (float)M_PI);
                float btri = 1.0f - 4.0f * fabsf(g_music_bass_phase - 0.5f);
                float btone = (trk == 6 || trk == 3 || trk == 4)
                    ? (bsin * 0.65f + btri * 0.35f)
                    : ((trk == 1) ? (bsin * 0.80f + sinf(g_music_bass_phase * 4.0f * (float)M_PI) * 0.20f) : bsin);
                mix += btone * benv * ((trk == 3 || trk == 4) ? 0.058f : 0.052f);
            }

            /* Naval timpani / rock kick drum */
            if (trk == 3 || trk == 4) {
                /* Driving rock kick on beats 1 & 3 (step % 4 == 0) */
                if ((step & 3) == 0) {
                    float kick_hz = 130.0f * expf(-u * 6.0f) + 45.0f;
                    g_music_timp_phase += kick_hz * dt;
                    if (g_music_timp_phase >= 1.0f) g_music_timp_phase -= 1.0f;
                    float kenv = expf(-u * 5.0f);
                    mix += sinf(g_music_timp_phase * 2.0f * (float)M_PI) * kenv * 0.075f;
                }
            } else if (step >= 32 && ((step & 7) == 0 || step == 94)) {
                float timp_hz = 105.0f - 42.0f * u;
                g_music_timp_phase += timp_hz * dt;
                if (g_music_timp_phase >= 1.0f) g_music_timp_phase -= 1.0f;
                float tenv = (u < 0.05f) ? (u / 0.05f) : (1.0f - u);
                mix += sinf(g_music_timp_phase * 2.0f * (float)M_PI) * tenv * tenv * 0.065f;
            }

            /* Varied percussion: rock backbeat (3,4), brushed snare (0,1,6), shaker (2,5,7) */
            if (trk == 3 || trk == 4) {
                if ((step & 3) == 2) {
                    /* Crisp rock snare backbeat on 2 & 4 */
                    float senv = (u < 0.45f) ? (1.0f - u * 2.2f) : 0.0f;
                    mix += next_noise() * senv * senv * 0.036f;
                } else {
                    /* Tight 8th-note hi-hat */
                    float henv = (u < 0.12f) ? (1.0f - u * 8.0f) : 0.0f;
                    mix += next_noise() * henv * henv * 0.012f;
                }
            } else if (trk == 0 || trk == 1 || trk == 6) {
                if ((step & 1) == 0 || (step & 7) == 7) {
                    float senv = (u < 0.25f) ? (1.0f - u * 4.0f) : 0.0f;
                    mix += next_noise() * senv * senv * 0.018f;
                }
            } else {
                if ((step & 1) == 1) {
                    float senv = (u < 0.14f) ? (1.0f - u * 7.0f) : 0.0f;
                    mix += next_noise() * senv * senv * 0.014f;
                }
            }

            /* First song starts at 1.0f and ducks to 0.38f; subsequent songs slowly swell to 0.52f and settle to 0.38f */
            float music_vol = 0.38f;
            if (g_music_loops == 0) {
                float peak_vol   = (g_music_songs_played == 0) ? 1.0f : 0.52f;
                float swell_end  = 16.0f * step_dur;
                float fade_start = 48.0f * step_dur;
                float fade_end   = 80.0f * step_dur;
                if (g_music_songs_played > 0 && g_music_time < swell_end) {
                    music_vol = peak_vol * (g_music_time / swell_end);
                } else if (g_music_time <= fade_start) {
                    music_vol = peak_vol;
                } else if (g_music_time < fade_end) {
                    float f = (g_music_time - fade_start) / (fade_end - fade_start);
                    music_vol = peak_vol - f * (peak_vol - 0.38f);
                }
            }
            mix *= music_vol;
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
        g_music_loops = 0;
        g_music_songs_played = 0;
        g_music_downtime = 0.0f;
        g_music_mel_phase = 0.0f;
        g_music_bass_phase = 0.0f;
        g_music_harm_phase = 0.0f;
        g_music_timp_phase = 0.0f;
        g_music_track = (int)(((uint32_t)time(NULL) ^ (uint32_t)SDL_GetTicks() ^ g_noise_state) % 8u);
    }
    g_music_active = active;
}

void audio_toggle_mute(void) {
    g_muted = !g_muted;
}

int audio_is_muted(void) {
    return g_muted;
}
