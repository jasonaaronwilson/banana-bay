#include "audio.h"
#include "game.h"
#include "palette.h"
#include "version.h"
#include <SDL2/SDL.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>

EM_JS(void, web_quit_redirect, (void), {
    window.location.href = 'about:blank';
});

EM_JS(void, web_poll_gamepad, (float *out_mx, float *out_my, int *out_gun, int *out_bomb, int *out_start, int *out_a, int *out_b), {
    var mx = 0.0, my = 0.0, gun = 0, bomb = 0, start = 0, ba = 0, bb = 0;
    var pads = navigator.getGamepads ? navigator.getGamepads() : [];
    for (var i = 0; i < pads.length; i++) {
        var gp = pads[i];
        if (!gp || !gp.connected) continue;
        var ax0 = gp.axes.length > 0 ? gp.axes[0] : 0;
        var ax1 = gp.axes.length > 1 ? gp.axes[1] : 0;
        if (Math.abs(ax0) > 0.22) mx += ax0;
        if (Math.abs(ax1) > 0.22) my += ax1;

        var b = gp.buttons;
        if (gp.mapping === 'standard') {
            if (b.length > 12 && b[12].pressed) my -= 1.0;
            if (b.length > 13 && b[13].pressed) my += 1.0;
            if (b.length > 14 && b[14].pressed) mx -= 1.0;
            if (b.length > 15 && b[15].pressed) mx += 1.0;
            if (b.length > 0 && b[0].pressed) { gun = 1; ba = 1; }
            if ((b.length > 1 && b[1].pressed) || (b.length > 2 && b[2].pressed)) { bomb = 1; }
            if (b.length > 1 && b[1].pressed) bb = 1;
            if (b.length > 7 && b[7].pressed) gun = 1;
            if (b.length > 6 && b[6].pressed) bomb = 1;
            if (b.length > 9 && b[9].pressed) start = 1;
        } else {
            /* Non-standard Linux/evdev gamepad in browser (e.g. GameSir-Tegenaria Lite) */
            if (gp.axes.length > 7) {
                if (gp.axes[6] < -0.5) mx -= 1.0;
                if (gp.axes[6] >  0.5) mx += 1.0;
                if (gp.axes[7] < -0.5) my -= 1.0;
                if (gp.axes[7] >  0.5) my += 1.0;
            }
            if (gp.axes.length > 9 && gp.axes[9] >= -1.0 && gp.axes[9] <= 1.0) {
                var h = gp.axes[9];
                if (h < -0.9) { my -= 1.0; }
                else if (h < -0.6) { my -= 1.0; mx += 1.0; }
                else if (h < -0.3) { mx += 1.0; }
                else if (h < 0.0)  { my += 1.0; mx += 1.0; }
                else if (h < 0.3)  { my += 1.0; }
                else if (h < 0.6)  { my += 1.0; mx -= 1.0; }
                else if (h < 0.9)  { mx -= 1.0; }
                else { my -= 1.0; mx -= 1.0; }
            }
            if (b.length > 0 && b[0].pressed) { gun = 1; ba = 1; }
            if ((b.length > 1 && b[1].pressed) || (b.length > 3 && b[3].pressed)) { bomb = 1; }
            if (b.length > 1 && b[1].pressed) bb = 1;
            if (gp.axes.length > 4 && gp.axes[4] > 0.25) gun = 1;
            if (gp.axes.length > 5 && gp.axes[5] > 0.25) bomb = 1;
            if (b.length > 11 && b[11].pressed) start = 1;
        }
        break;
    }
    HEAPF32[out_mx >> 2] = mx;
    HEAPF32[out_my >> 2] = my;
    HEAP32[out_gun >> 2] = gun;
    HEAP32[out_bomb >> 2] = bomb;
    HEAP32[out_start >> 2] = start;
    HEAP32[out_a >> 2] = ba;
    HEAP32[out_b >> 2] = bb;
});
#endif
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_debug_input = 0;

static SDL_GameController *open_first_controller(SDL_Joystick **out_joy) {
    for (int i = 0; i < SDL_NumJoysticks(); i++) {
        if (SDL_IsGameController(i)) {
            SDL_GameController *gc = SDL_GameControllerOpen(i);
            if (gc) {
                fprintf(stderr, "[input] GameController: %s\n",
                        SDL_GameControllerName(gc) ? SDL_GameControllerName(gc) : "unknown");
                if (g_debug_input) {
                    char *m = SDL_GameControllerMapping(gc);
                    fprintf(stderr, "[input] Mapping: %s\n", m ? m : "(null)");
                    SDL_free(m);
                }
                return gc;
            }
        }
    }
    if (out_joy && SDL_NumJoysticks() > 0) {
        SDL_Joystick *j = SDL_JoystickOpen(0);
        if (j) {
            fprintf(stderr, "[input] Joystick fallback: %s\n",
                    SDL_JoystickName(j) ? SDL_JoystickName(j) : "unknown");
            *out_joy = j;
        }
    }
    return NULL;
}

#ifndef __EMSCRIPTEN__
static float apply_deadzone(Sint16 raw, float deadzone) {
    float v = (float)raw / 32767.0f;
    if (fabsf(v) < deadzone) return 0.0f;
    return v;
}
#endif

typedef struct {
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *texture;
    Framebuffer *fb;
    Game *game;
    SDL_GameController *controller;
    SDL_Joystick *joystick;
    int running;
    int prev_bomb;
    Uint64 prev_ticks;
    double freq;
} AppState;

static void main_loop_step(void *arg) {
    AppState *app = (AppState *)arg;
    Game *game = app->game;
    Framebuffer *fb = app->fb;

    InputState in;
    SDL_zero(in);

    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        if (ev.type == SDL_QUIT) {
            app->running = 0;
        } else if (ev.type == SDL_CONTROLLERDEVICEADDED) {
            if (!app->controller) app->controller = open_first_controller(&app->joystick);
        } else if (ev.type == SDL_CONTROLLERDEVICEREMOVED) {
            if (app->controller && ev.cdevice.which == SDL_JoystickInstanceID(
                    SDL_GameControllerGetJoystick(app->controller))) {
                SDL_GameControllerClose(app->controller);
                app->controller = open_first_controller(&app->joystick);
            }
        } else if (ev.type == SDL_KEYDOWN && !ev.key.repeat) {
            switch (ev.key.keysym.sym) {
                case SDLK_RETURN:
                case SDLK_KP_ENTER:
                    in.confirm_pressed = 1;
                    break;
                case SDLK_p:
                case SDLK_ESCAPE:
                    in.pause_pressed = 1;
                    break;
                case SDLK_m:
                    in.mute_pressed = 1;
                    break;
                case SDLK_g:
                case SDLK_F1:
                    in.gallery_pressed = 1;
                    break;
                case SDLK_q:
                    in.quit_pressed = 1;
                    break;
                case SDLK_y:
                    in.yes_pressed = 1;
                    break;
                case SDLK_n:
                    in.no_pressed = 1;
                    break;
                case SDLK_UP:
                case SDLK_w:
                    if (game->state == STATE_TITLE) in.diff_delta = -1;
                    break;
                case SDLK_DOWN:
                case SDLK_s:
                    if (game->state == STATE_TITLE) in.diff_delta = 1;
                    break;
                case SDLK_1:
                    if (game->state == STATE_TITLE) game->difficulty = DIFF_EASY;
                    break;
                case SDLK_2:
                    if (game->state == STATE_TITLE) game->difficulty = DIFF_NORMAL;
                    break;
                case SDLK_3:
                    if (game->state == STATE_TITLE) game->difficulty = DIFF_HARD;
                    break;
                default:
                    break;
            }
        } else if (ev.type == SDL_CONTROLLERBUTTONDOWN) {
#ifndef __EMSCRIPTEN__
            if (g_debug_input) {
                fprintf(stderr, "[input] GC button down: %d (%s)\n",
                        ev.cbutton.button,
                        SDL_GameControllerGetStringForButton((SDL_GameControllerButton)ev.cbutton.button));
            }
            if (ev.cbutton.button == SDL_CONTROLLER_BUTTON_START) {
                if (game->state == STATE_PLAYING || game->state == STATE_PAUSED) {
                    in.pause_pressed = 1;
                } else {
                    in.confirm_pressed = 1;
                }
            }
            if ((game->state != STATE_PLAYING || game->quit_prompt) &&
                ev.cbutton.button == SDL_CONTROLLER_BUTTON_A) {
                in.confirm_pressed = 1;
            }
            if (game->quit_prompt && ev.cbutton.button == SDL_CONTROLLER_BUTTON_B) {
                in.no_pressed = 1;
            }
            if (game->state == STATE_TITLE) {
                if (ev.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_UP) in.diff_delta = -1;
                if (ev.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_DOWN) in.diff_delta = 1;
            }
#endif
        } else if (ev.type == SDL_JOYBUTTONDOWN && g_debug_input) {
            fprintf(stderr, "[input] Joy button down: %d\n", ev.jbutton.button);
        } else if (ev.type == SDL_CONTROLLERAXISMOTION && g_debug_input) {
            if (abs(ev.caxis.value) > 8000) {
                fprintf(stderr, "[input] GC axis %d (%s) = %d\n",
                        ev.caxis.axis,
                        SDL_GameControllerGetStringForAxis((SDL_GameControllerAxis)ev.caxis.axis),
                        ev.caxis.value);
            }
        }
    }

    const Uint8 *keys = SDL_GetKeyboardState(NULL);
    if (keys[SDL_SCANCODE_LEFT]  || keys[SDL_SCANCODE_A]) in.move_x -= 1.0f;
    if (keys[SDL_SCANCODE_RIGHT] || keys[SDL_SCANCODE_D]) in.move_x += 1.0f;
    if (keys[SDL_SCANCODE_UP]    || keys[SDL_SCANCODE_W]) in.move_y -= 1.0f;
    if (keys[SDL_SCANCODE_DOWN]  || keys[SDL_SCANCODE_S]) in.move_y += 1.0f;

    if (keys[SDL_SCANCODE_SPACE] || keys[SDL_SCANCODE_Z] || keys[SDL_SCANCODE_J]) {
        in.fire_gun = 1;
    }
    if (keys[SDL_SCANCODE_X] || keys[SDL_SCANCODE_K] || keys[SDL_SCANCODE_LSHIFT]) {
        in.drop_bomb = 1;
    }

#ifdef __EMSCRIPTEN__
    {
        static int prev_wstart = 0, prev_wa = 0, prev_wb = 0, prev_wup = 0, prev_wdown = 0;
        float wmx = 0.0f, wmy = 0.0f;
        int wgun = 0, wbomb = 0, wstart = 0, wa = 0, wb = 0;
        web_poll_gamepad(&wmx, &wmy, &wgun, &wbomb, &wstart, &wa, &wb);
        in.move_x += wmx;
        in.move_y += wmy;
        if (wgun) in.fire_gun = 1;
        if (wbomb) in.drop_bomb = 1;

        if (wstart && !prev_wstart) {
            if (game->state == STATE_PLAYING || game->state == STATE_PAUSED) {
                in.pause_pressed = 1;
            } else {
                in.confirm_pressed = 1;
            }
        }
        if (wa && !prev_wa && (game->state != STATE_PLAYING || game->quit_prompt)) {
            in.confirm_pressed = 1;
        }
        if (wb && !prev_wb && game->quit_prompt) {
            in.no_pressed = 1;
        }
        int wup = (wmy < -0.5f), wdown = (wmy > 0.5f);
        if (game->state == STATE_TITLE) {
            if (wup && !prev_wup) in.diff_delta = -1;
            if (wdown && !prev_wdown) in.diff_delta = 1;
        }
        prev_wstart = wstart;
        prev_wa = wa;
        prev_wb = wb;
        prev_wup = wup;
        prev_wdown = wdown;
    }
#else
    if (app->controller) {
        float ax = apply_deadzone(SDL_GameControllerGetAxis(app->controller, SDL_CONTROLLER_AXIS_LEFTX), 0.22f);
        float ay = apply_deadzone(SDL_GameControllerGetAxis(app->controller, SDL_CONTROLLER_AXIS_LEFTY), 0.22f);
        if (ax != 0.0f || ay != 0.0f) {
            in.move_x += ax;
            in.move_y += ay;
        }
        if (SDL_GameControllerGetButton(app->controller, SDL_CONTROLLER_BUTTON_DPAD_LEFT))  in.move_x -= 1.0f;
        if (SDL_GameControllerGetButton(app->controller, SDL_CONTROLLER_BUTTON_DPAD_RIGHT)) in.move_x += 1.0f;
        if (SDL_GameControllerGetButton(app->controller, SDL_CONTROLLER_BUTTON_DPAD_UP))    in.move_y -= 1.0f;
        if (SDL_GameControllerGetButton(app->controller, SDL_CONTROLLER_BUTTON_DPAD_DOWN))  in.move_y += 1.0f;

        if (SDL_GameControllerGetButton(app->controller, SDL_CONTROLLER_BUTTON_A) ||
            SDL_GameControllerGetAxis(app->controller, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) > 8000) {
            in.fire_gun = 1;
        }
        if (SDL_GameControllerGetButton(app->controller, SDL_CONTROLLER_BUTTON_B) ||
            SDL_GameControllerGetButton(app->controller, SDL_CONTROLLER_BUTTON_X) ||
            SDL_GameControllerGetAxis(app->controller, SDL_CONTROLLER_AXIS_TRIGGERLEFT) > 8000) {
            in.drop_bomb = 1;
        }
    } else if (app->joystick) {
        float ax = apply_deadzone(SDL_JoystickGetAxis(app->joystick, 0), 0.22f);
        float ay = apply_deadzone(SDL_JoystickGetAxis(app->joystick, 1), 0.22f);
        if (ax != 0.0f || ay != 0.0f) {
            in.move_x += ax;
            in.move_y += ay;
        }
        Uint8 hat = SDL_JoystickGetHat(app->joystick, 0);
        if (hat & SDL_HAT_LEFT)  in.move_x -= 1.0f;
        if (hat & SDL_HAT_RIGHT) in.move_x += 1.0f;
        if (hat & SDL_HAT_UP)    in.move_y -= 1.0f;
        if (hat & SDL_HAT_DOWN)  in.move_y += 1.0f;
        if (SDL_JoystickGetButton(app->joystick, 0)) in.fire_gun = 1;
        if (SDL_JoystickGetButton(app->joystick, 1) || SDL_JoystickGetButton(app->joystick, 2)) {
            in.drop_bomb = 1;
        }
    }
#endif

    in.drop_bomb_pressed = (in.drop_bomb && !app->prev_bomb);
    app->prev_bomb = in.drop_bomb;

    Uint64 now = SDL_GetPerformanceCounter();
    float dt = (float)((double)(now - app->prev_ticks) / app->freq);
    app->prev_ticks = now;
    if (dt > 0.05f) dt = 0.05f;

    game_update(game, &in, dt);
    if (game->should_quit) {
        app->running = 0;
#ifdef __EMSCRIPTEN__
        audio_shutdown();
        emscripten_cancel_main_loop();
        web_quit_redirect();
#endif
        return;
    }
    game_render(game, fb);

    void *pixels = NULL;
    int pitch = 0;
    if (SDL_LockTexture(app->texture, NULL, &pixels, &pitch) == 0) {
        gfx_present_argb8888(fb, (uint32_t *)pixels, pitch);
        SDL_UnlockTexture(app->texture);
    } else {
        fprintf(stderr, "[main] SDL_LockTexture failed: %s\n", SDL_GetError());
    }

    if (SDL_RenderClear(app->renderer) != 0) {
        fprintf(stderr, "[main] SDL_RenderClear failed: %s\n", SDL_GetError());
    }
    if (SDL_RenderCopy(app->renderer, app->texture, NULL, NULL) != 0) {
        fprintf(stderr, "[main] SDL_RenderCopy failed: %s\n", SDL_GetError());
    }
    SDL_RenderPresent(app->renderer);
}

int main(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--version") == 0 || strcmp(argv[i], "-v") == 0) {
            printf("Banana Bay v%s\n", BANANA_BAY_VERSION);
            return 0;
        }
        if (strcmp(argv[i], "--debug-input") == 0) {
            g_debug_input = 1;
        }
    }

    fprintf(stderr, "[main] Banana Bay v%s\n", BANANA_BAY_VERSION);

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window *window = SDL_CreateWindow(
        "Banana Bay v" BANANA_BAY_VERSION,
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        SCREEN_W, SCREEN_H,
#ifdef __EMSCRIPTEN__
        SDL_WINDOW_SHOWN
#else
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN
#endif
    );
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(
        window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );
    if (!renderer) {
        fprintf(stderr, "[main] Accelerated renderer failed (%s), falling back to software\n", SDL_GetError());
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (!renderer) {
        fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_RendererInfo rinfo;
    if (SDL_GetRendererInfo(renderer, &rinfo) == 0) {
        fprintf(stderr, "[main] Video driver: %s, Renderer: %s\n",
                SDL_GetCurrentVideoDriver() ? SDL_GetCurrentVideoDriver() : "unknown",
                rinfo.name ? rinfo.name : "unknown");
    }

    SDL_RenderSetLogicalSize(renderer, SCREEN_W, SCREEN_H);

    SDL_Texture *texture = SDL_CreateTexture(
        renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
        SCREEN_W, SCREEN_H
    );
    if (!texture) {
        fprintf(stderr, "SDL_CreateTexture failed: %s\n", SDL_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    Framebuffer *fb = (Framebuffer *)malloc(sizeof(Framebuffer));
    Game *game = (Game *)malloc(sizeof(Game));
    if (!fb || !game) {
        fprintf(stderr, "Out of memory\n");
        free(fb);
        free(game);
        SDL_DestroyTexture(texture);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    palette_init(fb);
    audio_init();
    game_init(game);

    AppState app;
    app.window = window;
    app.renderer = renderer;
    app.texture = texture;
    app.fb = fb;
    app.game = game;
    app.joystick = NULL;
    app.controller = open_first_controller(&app.joystick);
    app.running = 1;
    app.prev_bomb = 0;
    app.prev_ticks = SDL_GetPerformanceCounter();
    app.freq = (double)SDL_GetPerformanceFrequency();

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop_arg(main_loop_step, &app, 0, 1);
#else
    while (app.running) {
        main_loop_step(&app);
    }
#endif

    if (app.controller) SDL_GameControllerClose(app.controller);
    if (app.joystick) SDL_JoystickClose(app.joystick);
    audio_shutdown();
    game_free(game);
    free(game);
    free(fb);
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
