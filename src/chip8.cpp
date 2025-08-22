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

    // Precompute common values
    uint8_t x = (opcode & 0x0F00) >> 8;
    uint8_t y = (opcode & 0x00F0) >> 4;
    uint8_t kk = opcode & 0x00FF;
    uint16_t nnn = opcode & 0x0FFF;

    // Decode Opcode
    switch (opcode & 0xF000)
    {
    case 0x0000:
        switch (opcode & 0x000F)
        {
        case 0x0000: // 00E0 (CLS): Clears the screen
            memset(gfx, 0, sizeof(gfx));
            pc += 2;
            break;

        case 0x000E: // 00EE (RET): Returns from subroutine
            pc = stack[--sp];
            break;

        default:
            printf("Unknown 0x00XX opcode: 0x%X\n", opcode);
        }
        break;

    case 0x1000: // 1NNN (JP addr): Jump to location NNN
        pc = opcode & 0x0FFF;
        break;

    case 0x2000: // 2NNN (CALL addr): Calls subroutine at NNN
        stack[sp] = pc;
        ++sp;
        pc = opcode & 0x0FFF;
        break;

    case 0x3000: // 3XKK (SE Vx, byte): Skip next instruction if Vx = kk
        if (V[x] == kk)
        {
            pc += 4;
        }
        else
        {
            pc += 2;
        }
        break;

    case 0x4000: // 4XKK (SNE Vx, byte): Skip next instruction if Vx != kk
        if (V[x] != kk)
        {
            pc += 4;
        }
        else
        {
            pc += 2;
        }
        break;

    case 0x5000: // 5XY0 (SE Vx, Vy): Skip next instruction if Vx = Vy
        if (V[x] == V[y])
        {
            pc += 4;
        }
        else
        {
            pc += 2;
        }
        break;

    case 0x6000: // 6XKK (LD V, byte): Put the value kk into register Vx
        V[x] = kk;
        pc += 2;
        break;

    case 0x7000: // 7XKK (ADD Vx, byte): Add kk to Vx, wrap around if overflows
        V[x] += kk;
        pc += 2;
        break;

    case 0x8000:
        switch (opcode & 0x000F)
        {
        case 0x0000: // 8XY0 (LD Vx, Vy): Stores Vy's value in Vx
            V[x] = V[y];
            pc += 2;
            break;

        case 0x0001: // 8XY1 (OR Vx, Vy): ORs values of Vx and Vy, stores result in Vx
            V[x] = V[x] | V[y];
            pc += 2;
            break;

        case 0x0002: // 8XY2 (AND Vx, Vy): ANDs values of Vx and Vy, stores result in Vx
            V[x] = V[x] & V[y];
            pc += 2;
            break;

        case 0x0003: // 8XY3 (XOR Vx, Vy): XORs values of Vx and Vy, stores result in Vx
            V[x] = V[x] ^ V[y];
            pc += 2;
            break;

        case 0x0004:                                 // 8XY4 (ADD Vx, Vy): ADD Vy to Vx, set VF if carry
            V[0xF] = (V[y] > (0xFF - V[x])) ? 1 : 0; // Set or clear carry flag
            V[x] += V[y];
            pc += 2;
            break;

        case 0x0005:                         // 8XY5 (SUB Vx, Vy): Set Vx = Vx - Vy, set VF = NOT borrow
            V[0xF] = (V[x] >= V[y]) ? 1 : 0; // No borrow if Vx >= Vy
            V[x] -= V[y];
            pc += 2;
            break;

        case 0x0006: // 8XY6 (SHR Vx {, Vy})
        {
            /*
            If the least-significant bit of Vx is 1, then VF is set to 1, otherwise 0.
            Then Vx is divided by 2.
            */
            uint8_t lsb = (V[x] & 0x1);
            V[0xF] = lsb ? 1 : 0; // No borrow or borrow
            V[x] >>= 1;
            pc += 2;
            break;
        }

        case 0x0007: // 8XY7 (SUBN Vx, Vy): SUB Vx from Vy, stores the result in Vx, set VF if borrow
        {
            V[0xF] = (V[y] >= V[x]) ? 1 : 0; // No borrow or borrow
            V[x] = V[y] - V[x];
            pc += 2;
            break;
        }

        case 0x000E: // 8XYE (SHL Vx {, Vy})
        {
            /*
            If the most-significant bit of Vx is 1, then VF is set to 1, otherwise 0.
            Then Vx is multiplied by 2.
            */
            uint8_t msb = ((V[x] & 0x80) >> 7);
            V[0xF] = msb ? 1 : 0; // No borrow or borrow
            V[x] <<= 1;
            pc += 2;
            break;
        }

        default:
            printf("Unknown 0x8XY_ opcode: 0x%X\n", opcode);
            break;
        }
        break;

    case 0x9000: // 9000 (SNE Vx, Vy): Skip next instruction if Vx != Vy
        if (V[x] != V[y])
        {
            pc += 4;
        }
        else
        {
            pc += 2;
        }
        break;

    case 0xA000: // ANNN (LD I, addr): Sets I to the address NNN
        I = opcode & 0x0FFF;
        pc += 2;
        break;

        /*
        TODO:

        Bnnn - JP V0, addr
        Jump to location nnn + V0. The program counter is set to nnn plus the value of V0.

        Cxkk - RND Vx, byte
        Set Vx = random byte AND kk. The interpreter generates a random number from 0 to 255, which is then
        ANDed with the value kk. The results are stored in Vx. See instruction 8xy2 for more information on AND.
        */

    case 0xD000: // DXYN (DRW Vx, Vy, nibble): Draws a sprite at (Vx, Vy) with N bytes of data starting at memory location I
    {
        unsigned short xpos = V[x];
        unsigned short ypos = V[y];
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
                    if (gfx[(xpos + xline + ((ypos + yline) * 64))] == 1)
                        V[0xF] = 1;                                 // Set collision flag
                    gfx[xpos + xline + ((ypos + yline) * 64)] ^= 1; // Toggle pixel
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
        case 0x009E: // EX9E (SKP Vx): Skips the next instruction if the key stored in VX is pressed
            if (key[V[(opcode & 0x0F00) >> 8]] != 0)
                pc += 4; // Skip next instruction
            else
                pc += 2; // Go to next instruction
            break;
        /*
        TODO: ExA1 - SKNP Vx
        Skip next instruction if key with the value of Vx is not pressed. Checks the keyboard, and if the key
        corresponding to the value of Vx is currently in the up position, PC is increased by 2.
        */
        default:
            printf("Unknown 0xEX__ opcode: 0x%X\n", opcode);
            break;
        }
        break;

    case 0xF000:
        /*
        TODO:

        Fx07 - LD Vx, DT
        Set Vx = delay timer value. The value of DT is placed into Vx.

        Fx0A - LD Vx, K
        Wait for a key press, store the value of the key in Vx. All execution stops until a key is pressed, then the
        value of that key is stored in Vx.
        */
        switch (opcode & 0x00FF)
        {
            /*
            TODO:

            Fx15 - LD DT, Vx
            Set delay timer = Vx. Delay Timer is set equal to the value of Vx.

            Fx18 - LD ST, Vx
            Set sound timer = Vx. Sound Timer is set equal to the value of Vx.

            Fx1E - ADD I, Vx
            Set I = I + Vx. The values of I and Vx are added, and the results are stored in I.

            Fx29 - LD F, Vx
            Set I = location of sprite for digit Vx. The value of I is set to the location for the hexadecimal sprite
            corresponding to the value of Vx. See section 2.4, Display, for more information on the Chip-8 hexadecimal
            font. To obtain this value, multiply VX by 5 (all font data stored in first 80 bytes of memory).
            */
        case 0x0033: // FX33 (LD B, Vx): Stores BCD representation of VX in memory locations I, I+1, and I+2
        {
            memory[I] = V[x] / 100;
            memory[I + 1] = (V[x] / 10) % 10;
            memory[I + 2] = V[x] % 10;
            pc += 2;
            break;
        }
        /*
        TODO:

        Fx55 - LD [I], Vx
        Stores V0 to VX in memory starting at address I. I is then set to I + x + 1.

        Fx65 - LD Vx, [I]
        Fills V0 to VX with values from memory starting at address I. I is then set to I + x + 1.
        */
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
