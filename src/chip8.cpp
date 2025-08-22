#include "chip8.h"
#include <cstring>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>

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
    if (!file)
    {
        throw std::runtime_error("Failed to open ROM file.");
    }
    size_t bytesRead = fread(&memory[0x200], 1, 4096 - 0x200, file);
    fclose(file);
    if (bytesRead == 0)
    {
        throw std::runtime_error("Failed to read ROM file or file is empty.");
    }
}

void Chip8::emulateCycle()
{
    // Fetch Opcode:
    if (pc + 1 >= 4096)
    {
        throw std::out_of_range("Program counter out of memory bounds.");
    }
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
            if (sp == 0)
                throw std::underflow_error("Stack underflow on RET.");
            pc = stack[--sp];
            pc += 2; // FIX: Advance to next instruction after return
            break;

        default:
            throw std::runtime_error(("Unknown 0x00XX opcode: 0x" + std::to_string(opcode)).c_str());
        }
        break;

    case 0x1000: // 1NNN (JP addr): Jump to location NNN
        if (nnn >= 4096)
            throw std::out_of_range("Jump address out of bounds.");
        pc = nnn;
        break;

    case 0x2000: // 2NNN (CALL addr): Calls subroutine at NNN
        if (sp >= 16)
            throw std::overflow_error("Stack overflow on CALL.");
        stack[sp] = pc;
        ++sp;
        if (nnn >= 4096)
            throw std::out_of_range("Call address out of bounds.");
        pc = nnn;
        break;

    case 0x3000: // 3XKK (SE Vx, byte): Skip next instruction if Vx = kk
        pc += (V[x] == kk) ? 4 : 2;
        break;

    case 0x4000: // 4XKK (SNE Vx, byte): Skip next instruction if Vx != kk
        pc += (V[x] != kk) ? 4 : 2;
        break;

    case 0x5000: // 5XY0 (SE Vx, Vy): Skip next instruction if Vx = Vy
        if ((opcode & 0x000F) != 0)
            throw std::runtime_error(("Unknown 0x5XY_ opcode: 0x" + std::to_string(opcode)).c_str());
        pc += (V[x] == V[y]) ? 4 : 2;
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
            throw std::runtime_error(("Unknown 0x8XY_ opcode: 0x" + std::to_string(opcode)).c_str());
        }
        break;

    case 0x9000: // 9XY0 (SNE Vx, Vy): Skip next instruction if Vx != Vy
        if ((opcode & 0x000F) != 0)
            throw std::runtime_error(("Unknown 0x9XY_ opcode: 0x" + std::to_string(opcode)).c_str());
        pc += (V[x] != V[y]) ? 4 : 2;
        break;

    case 0xA000: // ANNN (LD I, addr): Sets I to the address NNN
        I = opcode & 0x0FFF;
        pc += 2;
        break;

    case 0xB000: // BNNN (JP V0, addr): Sets PC to address NNN + V0
        pc = (opcode & 0x0FFF) + V[0x0];
        break;

    case 0xC000: // CXKK (RND Vx, byte): Sets Vx to AND of a random byte and kk
        V[x] = (rand() % 256) & kk;
        pc += 2;
        break;

    case 0xD000: // DXYN (DRW Vx, Vy, nibble): Draws a sprite at (Vx, Vy) with N bytes of data starting at memory location I
    {
        unsigned short xpos = V[x];
        unsigned short ypos = V[y];
        unsigned short height = opcode & 0x000F;
        unsigned short pixel;

        if (I + height > 4096)
            throw std::out_of_range("Sprite draw out of memory bounds.");

        V[0xF] = 0; // Reset collision flag

        for (int yline = 0; yline < height; yline++)
        {
            pixel = memory[I + yline];
            for (int xline = 0; xline < 8; xline++)
            {
                int gfxIndex = xpos + xline + ((ypos + yline) * 64);
                if (gfxIndex >= 64 * 32)
                    continue; // Prevent out-of-bounds
                if ((pixel & (0x80 >> xline)) != 0)
                {
                    if (gfx[gfxIndex] == 1)
                        V[0xF] = 1;     // Set collision flag
                    gfx[gfxIndex] ^= 1; // Toggle pixel
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
        case 0x009E: // EX9E (SKP Vx): Skips the next instruction if the key stored in Vx is pressed
            pc += (key[V[x]] != 0) ? 4 : 2;
            break;

        case 0x00A1: // EXA1 (SKNP Vx): Skips the next instruction if the key stored of Vx is not pressed
            pc += (key[V[x]] == 0) ? 4 : 2;
            break;

        default:
            printf("Unknown 0xEX__ opcode: 0x%X\n", opcode);
            break;
        }
        break;

    case 0xF000:

        switch (opcode & 0x00FF)
        {
        case 0x0007: // FX07 (LD Vx, DT): The value of DT is placed into Vx
            V[x] = delay_timer;
            pc += 2;
            break;

        case 0x000A: // FX0A (LD Vx, K): Wait (all execution is stopped) for a key press, store the value of the key in Vx
        {
            bool keyPressed = false;
            for (int i = 0; i < 16; ++i)
            {
                if (key[i] != 0)
                {
                    V[x] = i;
                    keyPressed = true;
                    break;
                }
            }
            if (!keyPressed) // Repeat this opcode next cycle until a key is pressed
                return;
            pc += 2;
            break;
        }

        case 0x0015: // FX15 (LD DT, Vx): Set DT to Vx
            delay_timer = V[x];
            pc += 2;
            break;

        case 0x0018: // FX18 (LD ST, Vx): Set ST to Vx
            sound_timer = V[x];
            pc += 2;
            break;

        case 0x001E: // FX1E (ADD I, Vx): Add Vx to I
            if (I + V[x] >= 4096)
                throw std::out_of_range("I register addition out of bounds.");
            I += V[x];
            pc += 2;
            break;

        case 0x0029: // FX29 (LD F, Vx): Set I to the location of sprite for Digit Vx
            if (V[x] > 0xF)
                throw std::out_of_range("Sprite digit out of range.");
            I = V[x] * SPRITE_LENGTH;
            pc += 2;
            break;

        case 0x0033: // FX33 (LD B, Vx): Stores BCD representation of VX in memory locations I, I+1, and I+2
        {
            if (I + 2 >= 4096)
                throw std::out_of_range("BCD store out of memory bounds.");
            memory[I] = V[x] / 100;
            memory[I + 1] = (V[x] / 10) % 10;
            memory[I + 2] = V[x] % 10;
            pc += 2;
            break;
        }

        case 0x0055: // FX55 (LD [I], Vx): Stores V0 to Vx in memory starting at address I.
        {
            if (I + x >= 4096)
                throw std::out_of_range("Memory store out of bounds.");
            for (int i = 0; i <= x; ++i)
            {
                memory[I + i] = V[i];
            }
            pc += 2;
            break;
        }

        case 0x0065: // FX65 (LD Vx, [I]): Stores memory starting at address I in V0 to Vx.
        {
            if (I + x >= 4096)
                throw std::out_of_range("Memory load out of bounds.");
            for (int i = 0; i <= x; ++i)
            {
                V[i] = memory[I + i];
            }
            pc += 2;
            break;
        }
        default:
            printf("Unknown 0xFX__ opcode: 0x%X\n", opcode);
            break;
        }
        break;

    default:
        printf("Unknown opcode: 0x%X\n", opcode);
    }

    // Update timers
    if (delay_timer > 0)
        --delay_timer;

    if (sound_timer > 0)
        --sound_timer;
}
