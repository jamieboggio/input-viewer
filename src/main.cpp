#include <fcntl.h>
#include <unistd.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <libevdev-1.0/libevdev/libevdev.h>

#include <array>
#include <algorithm>
#include <iostream>
#include <optional>
#include <chrono>

// TODO: using namespace XXX is bad please don't
using namespace std::chrono_literals;

enum class InputAction {
    Green,Red,Yellow,Blue,Orange,StrumUp,StrumDown,SP,Whammy
};

struct InputState {
    std::array<bool, 5> frets{};

    std::array<bool, 2> strum{};
    bool starPower = false;

    float whammy = 0.0f;
};


int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    int fd = open("/dev/input/event4", O_RDONLY | O_NONBLOCK);

    if (fd < 0) {
        std::cerr << "failed to open input device\n";
        return 1;
    }

    libevdev* device = nullptr;
    int result = libevdev_new_from_fd(fd, &device);
    if (result < 0) {
        std::cerr << "failed to initialize libevdev\n";
        close(fd);
        return 1;
    }

    std::cout << "Device: " << libevdev_get_name(device) << '\n';


    if (!SDL_Init(SDL_INIT_VIDEO))
        return 1;

    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;

    constexpr int SCREEN_WIDTH = 512;
    constexpr int SCREEN_HEIGHT = 256;

    if (!SDL_CreateWindowAndRenderer(
            "Input Viewer", SCREEN_WIDTH, SCREEN_HEIGHT, 0,
            &window, &renderer)) {
        SDL_Quit();
        return 1;
            }

    constexpr std::array strumStates{
        1, -1
    };

    constexpr std::array fretButtons{
        BTN_SOUTH,
        BTN_EAST,
        BTN_WEST,
        BTN_NORTH,
        BTN_TL
    };
    constexpr std::array<SDL_Color, 5> colors{{
        {0, 200, 0, 255},
        {220, 0, 0, 255},
        {240, 200, 0, 255},
        {0, 100, 240, 255},
        {240, 120, 0, 255}
    }};

    bool running = true;

    InputState state{};
    std::optional<std::chrono::microseconds> spPressedAt;
    std::optional<std::chrono::microseconds> nextButtonPressed;
    bool squeezeCheck = false;
    bool drawSqueeze = false;
    float whammyFull = 255.0f;
    float whammyRest = 0.0f;
    float whammyRaw = 0.0f;

    // use double to avoid type narrowing :)
    double squeezeTimeMS = 0.0;

    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
                goto cleanup;
            }
            if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_R) {
                whammyRest = whammyRaw;
#ifdef PRINT
                std::cout << "whammy rest: " << whammyRest << '\n';
#endif
            }
            if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_F) {
                whammyFull = whammyRaw;
#ifdef PRINT
                std::cout << "whammy full: " << whammyFull << '\n';
#endif
            }

        }

        input_event input{};
        while (libevdev_next_event(
        device,
        LIBEVDEV_READ_FLAG_NORMAL,
        &input
        ) == LIBEVDEV_READ_STATUS_SUCCESS) {
            // HANDLE FRET PRESS
            for (size_t i = 0; i < fretButtons.size(); i++) {
                if (input.type == EV_KEY && input.code == fretButtons[i]) {
                    state.frets[i] = (input.value == 1);
                    if (squeezeCheck && state.frets[i]) {
                        const auto timestamp = std::chrono::seconds{input.time.tv_sec}
                        + std::chrono::microseconds{input.time.tv_usec};
                        nextButtonPressed = timestamp;
                        squeezeTimeMS = std::chrono::duration<double, std::milli>{
                            *nextButtonPressed - *spPressedAt
                        }.count();
                        squeezeCheck = false;
                        drawSqueeze = true;
                    }
                }
            }

            // HANDLE STRUM
            if (input.type == EV_ABS && input.code == ABS_HAT0Y) {
                for (size_t i = 0; i < strumStates.size(); i++) {
                    state.strum[i] = (input.value == strumStates[i]);
                    if (squeezeCheck && state.strum[i]) {
                        const auto timestamp = std::chrono::seconds{input.time.tv_sec}
                        + std::chrono::microseconds{input.time.tv_usec};
                        nextButtonPressed = timestamp;
                        squeezeTimeMS = std::chrono::duration<double, std::milli>{
                            *nextButtonPressed - *spPressedAt
                        }.count();
                        squeezeCheck = false;
                        drawSqueeze = true;
                    }
                }

#ifdef PRINT
                std::cout << "Up: " << state.strum[0]
                << " Down: " << state.strum[1] << '\n';
#endif
            }

            // HANDLE SP
            if (input.type == EV_KEY && input.code == BTN_SELECT) {
                state.starPower = (input.value == 1);
                if (input.value == 1) {
                    const auto timestamp = std::chrono::seconds{input.time.tv_sec}
                    + std::chrono::microseconds{input.time.tv_usec};
                    spPressedAt = timestamp;
                    squeezeCheck = true;
                    nextButtonPressed.reset();
                }
#ifdef PRINT
                std::cout << (state.starPower ? "Select pressed\n" : "");
#endif

            }

            // HANDLE WHAMMY
            if (input.type == EV_ABS && input.code == ABS_X) {
                    whammyRaw = static_cast<float>(input.value);
#ifdef PRINT_WHAMMY_RAW
                    std::cout << "whammy % " << state.whammy * 100.0f << "%\n";
#endif
            }
        }
        if (whammyFull == whammyRest) [[unlikely]] {
            state.whammy = 0.0f;
        }else {
            state.whammy = std::clamp((whammyRaw - whammyRest)/(whammyFull - whammyRest), 0.0f, 1.0f);
            // fixes -0.0 error! not actually redundant!
            if (state.whammy == 0.0f) {
                state.whammy = 0.0f;
            }
        }
        SDL_SetRenderDrawColor(renderer, 25, 25, 25, 0);
        SDL_RenderClear(renderer);

        for (size_t i = 0; i < colors.size(); ++i) {
            SDL_Color color = colors[i];

            if (!state.frets[i]) {
                color.r /= 2;
                color.g /= 2;
                color.b /= 2;
            }

            SDL_SetRenderDrawColor(
                renderer, color.r, color.g, color.b, 255
            );

            SDL_FRect rect{
                40.0f + static_cast<float>(i) * 70.0f,
                50.0f, 50.0f, 80.0f
            };

            SDL_RenderFillRect(renderer, &rect);
        }

        // DRAW WHAMMY
        {
            constexpr float outlineWidth = 0.75f * static_cast<float>(SCREEN_WIDTH);
            constexpr float height = 0.10f * static_cast<float>(SCREEN_HEIGHT);
            constexpr float whamX = 10.0f;
            constexpr float whamY = 10.0f;

            SDL_SetRenderDrawColor(
                renderer, 0, 128, 128, 255
                );

            SDL_FRect rect{
                .x = whamX, .y = whamY, .w = outlineWidth, .h = height
            };

            SDL_RenderFillRect(renderer, &rect);

            float width = outlineWidth * state.whammy;
            rect.w = width;
            SDL_SetRenderDrawColor(renderer, 0, 255, 255, 255);
            SDL_RenderFillRect(renderer, &rect);

        }

        // DRAW STRUM
        {
            SDL_SetRenderDrawColor(
             renderer, 128, 0, 128, 255
            );

            float upX = 155, upY = 140;
            float downX = 215, downY = 140;
            float width = 40.f, height = 25.f;

            SDL_FRect upRect{
                .x = upX, .y=upY, .w = width, .h = height
            };

            if (state.strum[0]) {
                SDL_SetRenderDrawColor(
                 renderer, 255, 0, 255, 255
                );
            }
            SDL_RenderFillRect(renderer, &upRect);
            SDL_SetRenderDrawColor(renderer, 128, 0, 128, 255);

            SDL_FRect downRect{
                .x = downX, .y=downY, .w = width, .h = height
            };

            if (state.strum[1]) {
                SDL_SetRenderDrawColor(
                 renderer, 255, 0, 255, 255
                );
            }
            SDL_RenderFillRect(renderer, &downRect);

        }

        // DRAW SP
        {
            SDL_SetRenderDrawColor(renderer, 128, 128, 128, 255);
            float spX = static_cast<float>(SCREEN_WIDTH) - 128.0f, spY = 50.f;
            float width = 108.f, height = 80.f;
            if (state.starPower) {
                SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            }
            SDL_FRect spRect{
                .x = spX, .y = spY, .w = width, .h = height
            };

            SDL_RenderFillRect(renderer, &spRect);
        }

        // draw squeeze timer text
        if (nextButtonPressed.has_value()) {
            constexpr float fontSize = 32.0f;
            constexpr float scale = fontSize / static_cast<float>(SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE);

            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);

            SDL_SetRenderScale(renderer, scale, scale);
            SDL_RenderDebugTextFormat(
                renderer, 40.0f/scale, 195.0f/scale,
                "%.f ms", squeezeTimeMS
            );
            SDL_SetRenderScale(renderer, 1.0f, 1.0f);
        }

        SDL_RenderPresent(renderer);
        SDL_Delay(1);
    }
cleanup:
    libevdev_free(device);
    close(fd);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
}
