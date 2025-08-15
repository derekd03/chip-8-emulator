#include "chip8.h"
#include <cstring>
#include <cstdio>

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
    /* Some opcodes */
    case 0x0000:
        switch (opcode & 0x000F)
        {
        case 0x0000: // 0x00E0: Clears the screen
            memset(gfx, 0, sizeof(gfx));
            pc += 2;
            break;

        case 0x000E: // 0x00EE: Returns from subroutine
            pc = stack[--sp];
            break;

        default:
            printf("Unknown 0x00XX opcode: 0x%X\n", opcode);
        }
        break;

    case 0x0004: // 0x8YX: Adds VY to VX, set VF if carry
        if (V[(opcode & 0x00F0) >> 4] > (0xFF - V[(opcode & 0x0F00) >> 8]))
        {
            V[0xF] = 1; // Set carry flag
        }
        else
        {
            V[0xF] = 0; // Clear carry flag
        }
        // Add VY to VX
        V[(opcode & 0x00F0) >> 4] += V[(opcode & 0x0F00) >> 8];
        pc += 2;
        break;

    case 0x0033: // 0xFX33: Stores BCD representation of VX in memory locations I, I+1, and I+2
        memory[I] = V[(opcode & 0x0F00) >> 8] / 100;
        memory[I + 1] = (V[(opcode & 0x0F00) >> 8] / 10) % 10;
        memory[I + 2] = V[(opcode & 0x0F00) >> 8] % 10;
        pc += 2;
        break;

    case 0x2000: // 2NNN: Calls subroutine at NNN
        stack[sp] = pc;
        ++sp;
        pc = opcode & 0x0FFF;
        break;

    case 0xA000: // ANNN: Sets I to the address NNN
        // Execute opcode
        I = opcode & 0x0FFF;
        pc += 2;
        break;

    case 0xD000:
    {
        // Fetch position and height of the sprite
        unsigned short x = V[(opcode & 0x0F00) >> 4];
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
                    // Check if the current evaluated pixel is set to 1
                    if (gfx[(x + xline + ((y + yline) * 64))] == 1)
                        V[0xF] = 1; // Set collision flag
                    // Set pixel value using XOR
                    gfx[x + xline + ((y + yline) * 64)] ^= 1; // Toggle pixel
                }
            }
        }
        // Update the screen
        drawFlag = true;
        pc += 2;
    }

    case 0xE000:
        switch (opcode & 0x00FF)
        {
        // EX9E: Skips the next instruction
        // if the key stored in VX is pressed
        case 0x009E:
            if (key[V[(opcode & 0x0F00) >> 8]] != 0)
                pc += 4; // Skip next instruction
            else
                pc += 2; // Go to next instruction
            break;
        }

    /* More opcodes */
    default:
        printf("Unknown opcode: 0x%X\n", opcode);
    }
    /*
    Check the opcode table to see what the opcode means
    0xAF0 // Assembly: mvi 2F0h

    If we take a look at the opcode table, it tells us the following:

    ANNN: Sets I to the address NNN

    We will need to set the index register I to the value of NNN (0x2F0)
    */

    // Execute Opcode
    /*
    1010001011110000 & // 0xA2F0 (opcode)
    0000111111111111 = // 0x0FFF
    ------------------
    0000001011110000   // 0x02F0 (0x2F0)

    Resulting code:

    I = opcode & 0x0FFF;
    pc += 2; // increment program counter by 2 after every executed opcode unless you call a subroutine
    */

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
