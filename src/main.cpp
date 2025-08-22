#include <SDL2/SDL.h>
#include <SDL2/SDL_audio.h>
#include <iostream>
#include <stdlib.h>
#include "chip8.h"

using namespace std;

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
    Chip8 chip8;

    // --- Startup test: fill display with checkerboard pattern if no ROM is provided ---
    bool testMode = (argc != 2);

    if (testMode)
    {
        cout << "No ROM provided. Running in startup test mode (checkerboard pattern)..." << endl;
    }

    chip8 = Chip8(); // Initialize Chip8

    int w = 640; // Window width
    int h = 320; // Window height

    SDL_Window *window = NULL; // Window pointer

    // Initialize SDL
    if (SDL_Init(SDL_INIT_EVERYTHING) < 0)
    {
        cerr << "Failed to initialize SDL: " << SDL_GetError() << endl;
        return 1;
    }

    // Create the window
    window = SDL_CreateWindow("Chip8 Emulator", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, w, h, SDL_WINDOW_SHOWN);
    if (!window)
    {
        cerr << "Failed to create window: " << SDL_GetError() << endl;
        return 1;
    }

    // Create renderer
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, 0);
    SDL_RenderSetLogicalSize(renderer, w, h);

    // Create texture that stores the frame buffer
    SDL_Texture *sdlTexture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_ARGB8888, // <-- change this!
        SDL_TEXTUREACCESS_STREAMING,
        64, 32);

    if (!sdlTexture)
    {
        cerr << "Failed to create SDL texture: " << SDL_GetError() << endl;
        return 1;
    }

    // Temporary pixel buffer
    uint32_t pixels[2048];

    if (!testMode)
    {
    load:
        // Load a ROM
        chip8.loadGame(argv[1]);
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
        cerr << "Failed to open audio device: " << SDL_GetError() << endl;
        return 1;
    }

    // Emulation loop
    for (;;)
    {
        // Emulate one cycle (skip in test mode)
        if (!testMode)
            chip8.emulateCycle();

        // Start beep if sound_timer > 0 and not already playing
        if (chip8.sound_timer > 0 && !audioPlaying && audioDevice) {
            SDL_PauseAudioDevice(audioDevice, 0); // Start audio
            audioPlaying = true;
        }
        
        // Stop beep if sound_timer == 0 and audio is playing
        if (chip8.sound_timer == 0 && audioPlaying && audioDevice) {
            SDL_PauseAudioDevice(audioDevice, 1); // Stop audio
            audioPlaying = false;
        }

        // Handle events
        SDL_Event e;
        while (SDL_PollEvent(&e))
        {
            if (e.type == SDL_QUIT)
                return 0;

            // Keydown events
            if (e.type == SDL_KEYDOWN)
            {
                // Print key pressed
                std::cout << "Key pressed: " << SDL_GetKeyName(e.key.keysym.sym) << std::endl;

                // Exit the emulator
                if (e.key.keysym.sym == SDLK_ESCAPE)
                    return 0;

                // Reload the ROM
                if (!testMode && e.key.keysym.sym == SDLK_F1)
                    goto load;

                for (int i = 0; i < 16; ++i)
                {
                    if (e.key.keysym.sym == KEY_MAP[i])
                    {
                        chip8.key[i] = true; // Set key state to pressed
                    }
                }
            }

            // Keyup events
            if (e.type == SDL_KEYUP)
            {
                // Print key released
                std::cout << "Key released: " << SDL_GetKeyName(e.key.keysym.sym) << std::endl;

                for (int i = 0; i < 16; ++i)
                {
                    if (e.key.keysym.sym == KEY_MAP[i])
                    {
                        chip8.key[i] = false; // Set key state to released
                    }
                }
            }
        }

        // In test mode, always set drawFlag to true to keep showing the pattern
        if (testMode)
            chip8.drawFlag = true;

        // Update the screen if the draw flag is set
        if (chip8.drawFlag)
        {
            chip8.drawFlag = false;

            // Convert chip8.gfx[] (0 or 1) into real ARGB pixels
            for (int i = 0; i < 64 * 32; ++i)
            {
                pixels[i] = chip8.gfx[i] ? 0xFFFFFFFF : 0xFF000000; // white or black
            }

            // Update the SDL texture using this temporary pixel array
            // Convert chip8.gfx[] (0 or 1) into real ARGB pixels
            for (int i = 0; i < 64 * 32; ++i)
            {
                pixels[i] = chip8.gfx[i] ? 0xFFFFFFFF : 0xFF000000; // white or black
            }

            // Update the SDL texture using this temporary pixel array
            SDL_UpdateTexture(sdlTexture, NULL, pixels, 64 * sizeof(uint32_t));
            SDL_RenderClear(renderer);
            SDL_RenderCopy(renderer, sdlTexture, NULL, NULL);
            SDL_RenderPresent(renderer);
        }

        // Delay to control emulation speed
        SDL_Delay(16);
    }

    return 0;
}