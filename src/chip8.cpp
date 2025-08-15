#include "chip8.h"
#include <cstring>
#include <cstdio>
#include <cstdint>

void Chip8::initialize()
{
    pc = 0x200; // Programs start at 0x200
    opcode = 0; // Reset current opcode
    I = 0;      // Reset index register
    sp = 0;     // Reset stack pointer

    memset(gfx, 0, sizeof(gfx));       // Clear display
    memset(stack, 0, sizeof(stack));   // Clear stack
    memset(V, 0, sizeof(V));           // Clear registers V0-VF
    memset(memory, 0, sizeof(memory)); // Clear memory
    memset(key, 0, sizeof(key));       // Clear key state

    // Load fontset
    for (int i = 0; i < 80; ++i)
        memory[i] = CHIP8_FONTSET[i];

    // Reset timers
    delay_timer = 0;
    sound_timer = 0;

    drawFlag = false;
}

void Chip8::loadGame(const char *filename)
{
    // Load game into memory
    FILE *file = fopen(filename, "rb");
    if (file)
    {
        fread(&memory[0x200], 1, 4096 - 0x200, file); // Fill the memory at 0x200 == 512
        fclose(file);
    }
}

void Chip8::emulateCycle()
{
    // Fetch Opcode:
    // merge both bytes with a bitwise OR operation
    // and store them in an unsigned short
    opcode = (memory[pc] << 8) | memory[pc + 1];

    // Decode Opcode
    switch (opcode & 0xF000)
    {
    case 0x0000:
        switch (opcode & 0x000F)
        {
        case 0x0000: // 00E0: Clears the screen
            memset(gfx, 0, sizeof(gfx));
            pc += 2;
            break;

        case 0x000E: // 00EE: Returns from subroutine
            pc = stack[--sp];
            break;

        default:
            printf("Unknown 0x00XX opcode: 0x%X\n", opcode);
        }
        break;

    case 0x2000: // 2NNN: Calls subroutine at NNN
        stack[sp] = pc;
        ++sp;
        pc = opcode & 0x0FFF;
        break;

    case 0x8000:
        switch (opcode & 0x000F)
        {
        case 0x0004: // 8XY4: Adds VY to VX, set VF if carry
        {
            uint8_t x = (opcode & 0x0F00) >> 8;
            uint8_t y = (opcode & 0x00F0) >> 4;
            if (V[y] > (0xFF - V[x]))
                V[0xF] = 1; // Set carry flag
            else
                V[0xF] = 0; // Clear carry flag
            V[x] += V[y];
            pc += 2;
            break;
        }
        default:
            printf("Unknown 0x8XY_ opcode: 0x%X\n", opcode);
            break;
        }
        break;

    case 0xA000: // ANNN: Sets I to the address NNN
        I = opcode & 0x0FFF;
        pc += 2;
        break;

    case 0xD000: // DXYN: Draws a sprite at (VX, VY) with N bytes of data starting at memory location I
    {
        unsigned short x = V[(opcode & 0x0F00) >> 8];
        unsigned short y = V[(opcode & 0x00F0) >> 4];
        unsigned short height = opcode & 0x000F;
        unsigned short pixel;

        V[0xF] = 0; // Reset collision flag

        for (int yline = 0; yline < height; yline++)
        {
            pixel = memory[I + yline];
            for (int xline = 0; xline < 8; xline++)
            {
                if ((pixel & (0x80 >> xline)) != 0)
                {
                    if (gfx[(x + xline + ((y + yline) * 64))] == 1)
                        V[0xF] = 1;                           // Set collision flag
                    gfx[x + xline + ((y + yline) * 64)] ^= 1; // Toggle pixel
                }
            }
        }
        drawFlag = true;
        pc += 2;
        break;
    }

    case 0xE000:
        switch (opcode & 0x00FF)
        {
        case 0x009E: // EX9E: Skips the next instruction if the key stored in VX is pressed
            if (key[V[(opcode & 0x0F00) >> 8]] != 0)
                pc += 4; // Skip next instruction
            else
                pc += 2; // Go to next instruction
            break;
        default:
            printf("Unknown 0xEX__ opcode: 0x%X\n", opcode);
            break;
        }
        break;

    case 0xF000:
        switch (opcode & 0x00FF)
        {
        case 0x0033: // FX33: Stores BCD representation of VX in memory locations I, I+1, and I+2
        {
            uint8_t x = (opcode & 0x0F00) >> 8;
            memory[I] = V[x] / 100;
            memory[I + 1] = (V[x] / 10) % 10;
            memory[I + 2] = V[x] % 10;
            pc += 2;
            break;
        }
        default:
            printf("Unknown 0xFX__ opcode: 0x%X\n", opcode);
            break;
        }
        break;

        /* TODO: More opcodes */

    default:
        printf("Unknown opcode: 0x%X\n", opcode);
    }

    // Update timers
    if (delay_timer > 0)
        --delay_timer;

    if (sound_timer > 0)
    {
        if (sound_timer == 1)
        {
            // TODO: Implement sound
        }
        --sound_timer;
    }
}
