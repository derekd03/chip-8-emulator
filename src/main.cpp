#include <SDL2/SDL.h>
#include <SDL2/SDL_audio.h>
#include <iostream>
#include <stdlib.h>
#include <stdexcept>
#include "chip8.h"

// Keypad       Keyboard
// +-+-+-+-+    +-+-+-+-+
// |1|2|3|C|    |1|2|3|4|
// +-+-+-+-+    +-+-+-+-+
// |4|5|6|D|    |Q|W|E|R|
// +-+-+-+-+ => +-+-+-+-+
// |7|8|9|E|    |A|S|D|F|
// +-+-+-+-+    +-+-+-+-+
// |A|0|B|F|    |Z|X|C|V|
// +-+-+-+-+    +-+-+-+-+
const uint8_t KEY_MAP[16] = {
    SDLK_x,
    SDLK_1,
    SDLK_2,
    SDLK_3,
    SDLK_q,
    SDLK_w,
    SDLK_e,
    SDLK_a,
    SDLK_s,
    SDLK_d,
    SDLK_z,
    SDLK_c,
    SDLK_4,
    SDLK_r,
    SDLK_f,
    SDLK_v,
};

SDL_AudioDeviceID audioDevice = 0;
bool audioPlaying = false;

// Square wave callback
void audioCallback(void *userdata, Uint8 *stream, int len)
{
    static int phase = 0;
    int freq = 440;         // 440 Hz beep
    int sampleRate = 44100; // 44.1 kHz
    int samplesPerCycle = sampleRate / freq;

    for (int i = 0; i < len; i++)
    {
        // Generate square wave
        stream[i] = (phase < samplesPerCycle / 2) ? 128 : 0;
        phase = (phase + 1) % samplesPerCycle;
    }
}

int main(int argc, char **argv)
{
    try
    {
        Chip8 chip8;

        // --- Startup test: fill display with checkerboard pattern if no ROM is provided ---
        bool testMode = (argc != 2);

        if (testMode)
        {
            std::cout << "No ROM provided. Running in startup test mode (checkerboard pattern)..." << std::endl;
        }

        chip8 = Chip8(); // Initialize Chip8

        int w = 640; // Window width
        int h = 320; // Window height

        SDL_Window *window = NULL; // Window pointer

        // Initialize SDL
        if (SDL_Init(SDL_INIT_EVERYTHING) < 0)
        {
            std::cerr << "Failed to initialize SDL: " << SDL_GetError() << std::endl;
            return 1;
        }

        // Create the window
        window = SDL_CreateWindow("Chip8 Emulator", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, w, h, SDL_WINDOW_SHOWN);
        if (!window)
        {
            std::cerr << "Failed to create window: " << SDL_GetError() << std::endl;
            SDL_Quit();
            return 1;
        }

        // Create renderer
        SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, 0);
        if (!renderer)
        {
            std::cerr << "Failed to create renderer: " << SDL_GetError() << std::endl;
            SDL_DestroyWindow(window);
            SDL_Quit();
            return 1;
        }
        SDL_RenderSetLogicalSize(renderer, w, h);

        // Create texture that stores the frame buffer
        SDL_Texture *sdlTexture = SDL_CreateTexture(
            renderer,
            SDL_PIXELFORMAT_ARGB8888,
            SDL_TEXTUREACCESS_STREAMING,
            64, 32);

        if (!sdlTexture)
        {
            std::cerr << "Failed to create SDL texture: " << SDL_GetError() << std::endl;
            SDL_DestroyRenderer(renderer);
            SDL_DestroyWindow(window);
            SDL_Quit();
            return 1;
        }

        // Temporary pixel buffer
        uint32_t pixels[2048];

        if (!testMode)
        {
        load:
            try
            {
                chip8.loadGame(argv[1]);
            }
            catch (const std::exception &e)
            {
                std::cerr << "Error loading ROM: " << e.what() << std::endl;
                SDL_DestroyTexture(sdlTexture);
                SDL_DestroyRenderer(renderer);
                SDL_DestroyWindow(window);
                SDL_Quit();
                return 1;
            }
        }
        else
        {
            // Fill the display with a checkerboard pattern for testing
            for (int y = 0; y < 32; ++y)
            {
                for (int x = 0; x < 64; ++x)
                {
                    chip8.gfx[y * 64 + x] = ((x + y) % 2) ? 1 : 0; // 1 = white, 0 = black
                }
            }
            chip8.drawFlag = true;
        }

        // Initialize audio
        SDL_AudioSpec audioSpec = {};
        audioSpec.freq = 44100;
        audioSpec.format = AUDIO_U8;
        audioSpec.channels = 1;
        audioSpec.samples = 2048;
        audioSpec.callback = audioCallback;

        audioDevice = SDL_OpenAudioDevice(NULL, 0, &audioSpec, NULL, 0);
        if (!audioDevice)
        {
            std::cerr << "Failed to open audio device: " << SDL_GetError() << std::endl;
            SDL_DestroyTexture(sdlTexture);
            SDL_DestroyRenderer(renderer);
            SDL_DestroyWindow(window);
            SDL_Quit();
            return 1;
        }

        // Emulation loop
        for (;;)
        {
            try
            {
                // Emulate one cycle (skip in test mode)
                if (!testMode)
                    chip8.emulateCycle();
            }
            catch (const std::exception &e)
            {
                std::cerr << "Emulation error: " << e.what() << std::endl;
                break;
            }

            // Start beep if sound_timer > 0 and not already playing
            if (chip8.sound_timer > 0 && !audioPlaying && audioDevice)
            {
                SDL_PauseAudioDevice(audioDevice, 0); // Start audio
                audioPlaying = true;
            }

            // Stop beep if sound_timer == 0 and audio is playing
            if (chip8.sound_timer == 0 && audioPlaying && audioDevice)
            {
                SDL_PauseAudioDevice(audioDevice, 1); // Stop audio
                audioPlaying = false;
            }

            // Handle events
            SDL_Event e;
            while (SDL_PollEvent(&e))
            {
                if (e.type == SDL_QUIT)
                    goto cleanup;

                // Keydown events
                if (e.type == SDL_KEYDOWN)
                {
                    std::cout << "Key pressed: " << SDL_GetKeyName(e.key.keysym.sym) << std::endl;

                    if (e.key.keysym.sym == SDLK_ESCAPE)
                        goto cleanup;

                    if (!testMode && e.key.keysym.sym == SDLK_F1)
                        goto load;

                    for (int i = 0; i < 16; ++i)
                    {
                        if (e.key.keysym.sym == KEY_MAP[i])
                        {
                            chip8.key[i] = true;
                        }
                    }
                }

                // Keyup events
                if (e.type == SDL_KEYUP)
                {
                    std::cout << "Key released: " << SDL_GetKeyName(e.key.keysym.sym) << std::endl;

                    for (int i = 0; i < 16; ++i)
                    {
                        if (e.key.keysym.sym == KEY_MAP[i])
                        {
                            chip8.key[i] = false;
                        }
                    }
                }
            }

            if (testMode)
                chip8.drawFlag = true;

            if (chip8.drawFlag)
            {
                chip8.drawFlag = false;

                for (int i = 0; i < 64 * 32; ++i)
                {
                    pixels[i] = chip8.gfx[i] ? 0xFFFFFFFF : 0xFF000000;
                }

                SDL_UpdateTexture(sdlTexture, NULL, pixels, 64 * sizeof(uint32_t));
                SDL_RenderClear(renderer);
                SDL_RenderCopy(renderer, sdlTexture, NULL, NULL);
                SDL_RenderPresent(renderer);
            }

            SDL_Delay(16);
        }

    cleanup:
        if (audioDevice)
            SDL_CloseAudioDevice(audioDevice);
        if (sdlTexture)
            SDL_DestroyTexture(sdlTexture);
        if (renderer)
            SDL_DestroyRenderer(renderer);
        if (window)
            SDL_DestroyWindow(window);
        SDL_Quit();
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        SDL_Quit();
        return 1;
    }
}