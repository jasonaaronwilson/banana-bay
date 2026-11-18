#include "audio.h"
#include "game.h"
#include "palette.h"
#include <SDL2/SDL.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static SDL_GameController *open_first_controller(void) {
    for (int i = 0; i < SDL_NumJoysticks(); i++) {
        if (SDL_IsGameController(i)) {
            SDL_GameController *gc = SDL_GameControllerOpen(i);
            if (gc) return gc;
        }
    }
    return NULL;
}

static float apply_deadzone(Sint16 raw, float deadzone) {
    float v = (float)raw / 32767.0f;
    if (fabsf(v) < deadzone) return 0.0f;
    return v;
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window *window = SDL_CreateWindow(
        "Banana Bay",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        SCREEN_W, SCREEN_H,
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN
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
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (!renderer) {
        fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
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

    SDL_GameController *controller = open_first_controller();

    int running = 1;
    int prev_bomb = 0;
    Uint64 prev_ticks = SDL_GetPerformanceCounter();
    const double freq = (double)SDL_GetPerformanceFrequency();

    while (running) {
        InputState in;
        SDL_zero(in);

        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) {
                running = 0;
            } else if (ev.type == SDL_CONTROLLERDEVICEADDED) {
                if (!controller) controller = open_first_controller();
            } else if (ev.type == SDL_CONTROLLERDEVICEREMOVED) {
                if (controller && ev.cdevice.which == SDL_JoystickInstanceID(
                        SDL_GameControllerGetJoystick(controller))) {
                    SDL_GameControllerClose(controller);
                    controller = open_first_controller();
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
                if (ev.cbutton.button == SDL_CONTROLLER_BUTTON_START ||
                    (game->state != STATE_PLAYING && ev.cbutton.button == SDL_CONTROLLER_BUTTON_A)) {
                    in.confirm_pressed = 1;
                }
                if (game->state == STATE_TITLE) {
                    if (ev.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_UP) in.diff_delta = -1;
                    if (ev.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_DOWN) in.diff_delta = 1;
                }
                if (ev.cbutton.button == SDL_CONTROLLER_BUTTON_BACK) {
                    in.pause_pressed = 1;
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

        /* Gamepad: left analog stick OR d-pad + separate gun/bomb buttons */
        if (controller) {
            float ax = apply_deadzone(SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTX), 0.18f);
            float ay = apply_deadzone(SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTY), 0.18f);
            if (ax != 0.0f || ay != 0.0f) {
                in.move_x += ax;
                in.move_y += ay;
            }
            if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_LEFT))  in.move_x -= 1.0f;
            if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_RIGHT)) in.move_x += 1.0f;
            if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_UP))    in.move_y -= 1.0f;
            if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_DOWN))  in.move_y += 1.0f;

            if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_A) ||
                SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) > 8000) {
                in.fire_gun = 1;
            }
            if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_B) ||
                SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_X) ||
                SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_TRIGGERLEFT) > 8000) {
                in.drop_bomb = 1;
            }
        }

        in.drop_bomb_pressed = (in.drop_bomb && !prev_bomb);
        prev_bomb = in.drop_bomb;

        Uint64 now = SDL_GetPerformanceCounter();
        float dt = (float)((double)(now - prev_ticks) / freq);
        prev_ticks = now;
        if (dt > 0.05f) dt = 0.05f;

        game_update(game, &in, dt);
        game_render(game, fb);

        void *pixels = NULL;
        int pitch = 0;
        if (SDL_LockTexture(texture, NULL, &pixels, &pitch) == 0) {
            gfx_present_argb8888(fb, (uint32_t *)pixels, pitch);
            SDL_UnlockTexture(texture);
        }

        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, NULL, NULL);
        SDL_RenderPresent(renderer);
    }

    if (controller) SDL_GameControllerClose(controller);
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
