#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <raylib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

#define RAYGUI_IMPLEMENTATION
#include "raygui.h"

#define MEM_SIZE 0XFFF
#define FONT_ADDR 0x050
#define STACK_SIZE 16
#define DISPLAY_WIDTH 64
#define DISPLAY_HEIGHT 32
#define SCALING 15
#define KEYPAD_LENGTH 16
#define PIXEL_ON = 255;
#define PIXEL_JUST_CLEARED = 254;

typedef enum {
    REG_V0, REG_V1, REG_V2, REG_V3,
    REG_V4, REG_V5, REG_V6, REG_V7,
    REG_V8, REG_V9, REG_VA, REG_VB,
    REG_VC, REG_VD, REG_VE, REG_VF,
    REG_COUNT,
} Register;

typedef enum {
    OP_SPECIALS,
    OP_JUMP,
    OP_SET_REG,
} OpType;

typedef struct {
    uint8_t memory[MEM_SIZE];
    uint8_t display[DISPLAY_WIDTH * DISPLAY_HEIGHT];
    uint16_t pc;
    uint16_t index_register;
    uint16_t* stack_ptr;
    uint16_t stack[STACK_SIZE];
    uint8_t delay_timer;
    uint8_t sound_timer;
    uint8_t keypad[KEYPAD_LENGTH];
    uint8_t var_registers[REG_COUNT];
} Chip8;

uint8_t font[] = {
        0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
        0x20, 0x60, 0x20, 0x20, 0x70, // 1
        0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
        0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
        0x90, 0x90, 0xF0, 0x10, 0x10, // 4
        0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
        0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
        0xF0, 0x10, 0x20, 0x40, 0x40, // 7
        0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
        0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
        0xF0, 0x90, 0xF0, 0x90, 0x90, // A
        0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
        0xF0, 0x80, 0x80, 0x80, 0xF0, // C
        0xE0, 0x90, 0x90, 0x90, 0xE0, // D
        0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
        0xF0, 0x80, 0xF0, 0x80, 0x80  // F
};

void chip8Init(Chip8* chip8) {
    memset(chip8->memory, 0, sizeof(chip8->memory));
    memset(chip8->display, 0, sizeof(chip8->display));

    memset(chip8->var_registers, 0, sizeof(chip8->var_registers));
    chip8->index_register = 0;

    memset(chip8->stack, 0, sizeof(chip8->stack));
    chip8->stack_ptr = chip8->stack;

    chip8->pc = 0x200;

    chip8->delay_timer = 0;
    chip8->sound_timer = 0;

    memset(chip8->keypad, 0, sizeof(chip8->keypad));

    // Load the font to the correct address
    memcpy(chip8->memory + 0x050, font, sizeof(font));
}

void chip8LoadRom(Chip8* chip8, const char* path) {
    FILE* file = fopen(path, "rb");

    fseek(file, 0L, SEEK_END);
    size_t fileSize = ftell(file);
    rewind(file);

    fread(chip8->memory + 0x200, sizeof(uint16_t), fileSize, file);

    fclose(file);

    // Make sure the program counter is at the beginning of the loaded memory
    chip8->pc = 0x200;
}

int chip8ProcessInstruction(Chip8* chip8) {
    if (chip8->pc > MEM_SIZE) return -1;

    // Fetch
    uint8_t high_byte = chip8->memory[chip8->pc++];
    uint8_t low_byte = chip8->memory[chip8->pc++];
    uint16_t instruction = (high_byte << 8) | low_byte;

    uint8_t type = (instruction >> 12) & 0xF;
    uint8_t x = (instruction >> 8) & 0xF;
    uint8_t y = (instruction >> 4) & 0xF;
    uint8_t n = instruction & 0xF;
    uint8_t nn = instruction & 0xFF;
    uint16_t nnn = instruction & 0xFFF;

    switch (type) {
        case 0x0: {
            switch (instruction) {
                case 0x00E0:
                    for (int i = 0; i < DISPLAY_WIDTH; i++)
                    {
                        for (int j = 0; j < DISPLAY_HEIGHT; j++)
                        {
                            uint8_t pixel = chip8->display[j * DISPLAY_WIDTH + i];
                            if (pixel == 255) {
                                chip8->display[j * DISPLAY_WIDTH + i] = 254;
                            }
                        }
                    }
                    break;
                case 0x00EE:
                    if (chip8->stack_ptr - chip8->stack <= 0) {
                        printf("Stack underflow at [%X]", chip8->pc);
                        return -1;
                    }

                    chip8->stack_ptr--;
                    chip8->pc = *chip8->stack_ptr;
                    break;
                default:
                    break; // Do nothing on 0NNN
            }
            break;
        }
        case 0x1:
            chip8->pc = nnn;
            break;
        case 0x2:
            if (chip8->stack_ptr - chip8->stack >= STACK_SIZE) {
                printf("Stack overflow at [%X]", chip8->pc);
                return -1;
            }

            *(chip8->stack_ptr) = chip8->pc;
            chip8->stack_ptr++;
            chip8->pc = nnn;
            break;
        case 0x3: {
            if (chip8->var_registers[x] == nn) chip8->pc += 2;
            break;
        }
        case 0x4: {
            if (chip8->var_registers[x] != nn) chip8->pc += 2;
            break;
        }
        case 0x5: {
            if (chip8->var_registers[x] == chip8->var_registers[y]) chip8->pc += 2;
            break;
        }
        case 0x9: {
            if (chip8->var_registers[x] != chip8->var_registers[y]) chip8->pc += 2;
            break;
        }
        case 0x6: {
            chip8->var_registers[x] = nn;
            break;
        }
        case 0x7: {
            chip8->var_registers[x] += nn;
            break;
        }
        case 0x8: {
            switch (n) {
                case 0x0:
                    chip8->var_registers[x] = chip8->var_registers[y];
                    break;
                case 0x1:
                    chip8->var_registers[x] |= chip8->var_registers[y];
                    break;
                case 0x2:
                    chip8->var_registers[x] &= chip8->var_registers[y];
                    break;
                case 0x3:
                    chip8->var_registers[x] ^= chip8->var_registers[y];
                    break;
                case 0x4: {
                    uint8_t vx = chip8->var_registers[x];
                    uint8_t vy = chip8->var_registers[y];

                    chip8->var_registers[x] = vx + vy;

                    if (vx > 0xFF - vy) {
                        chip8->var_registers[REG_VF] = 1;
                    } else {
                        chip8->var_registers[REG_VF] = 0;
                    }

                    break;
                }
                case 0x5: {
                    uint8_t vx = chip8->var_registers[x];
                    uint8_t vy = chip8->var_registers[y];

                    chip8->var_registers[x] = vx - vy;

                    if (vx > vy) {
                        chip8->var_registers[REG_VF] = 1;
                    } else {
                        chip8->var_registers[REG_VF] = 0;
                    }

                    break;
                }
                case 0x7: {
                    uint8_t vx = chip8->var_registers[x];
                    uint8_t vy = chip8->var_registers[y];

                    chip8->var_registers[x] = vy - vx;

                    if (vy > vx) {
                        chip8->var_registers[REG_VF] = 1;
                    } else {
                        chip8->var_registers[REG_VF] = 0;
                    }

                    break;
                }
                case 0x6: {
                    uint8_t shifted_bit = chip8->var_registers[x] & 1;
                    chip8->var_registers[x] >>= 1;
                    chip8->var_registers[REG_VF] = shifted_bit;
                    break;
                }
                case 0xE: {
                    uint8_t shifted_bit = (chip8->var_registers[x] >> 7) & 1;
                    chip8->var_registers[x] <<= 1;
                    chip8->var_registers[REG_VF] = shifted_bit;
                    break;
                }
                default:
                    printf("Unknown opcode: 0x%X\n", instruction);
                    return -1;
            }
            break;
        }
        case 0xA:
            chip8->index_register = nnn;
            break;
        case 0xB:
            chip8->pc = nnn + chip8->var_registers[REG_V0];
            break;
        case 0xC:
            chip8->var_registers[x] = (rand() % 256) & nn;
            break;
        case 0xD: {
            uint8_t x_coord_init = chip8->var_registers[x] % DISPLAY_WIDTH;

            uint8_t x_coord = x_coord_init;
            uint8_t y_coord = chip8->var_registers[y] % DISPLAY_HEIGHT;

            chip8->var_registers[REG_VF] = 0;

            for (int i = 0; i < n; i++) {
                uint8_t sprite_byte = chip8->memory[chip8->index_register + i];
                for (int j = 7; j >= 0; j--) {
                    uint8_t pixel = (sprite_byte >> j) & 1;
                    int screen_pixel_index = y_coord * DISPLAY_WIDTH + x_coord;

                    if ((pixel == 1) && (chip8->display[screen_pixel_index] < 255)) {
                        chip8->display[screen_pixel_index] = 255;
                        chip8->var_registers[REG_VF] = 1;
                    } else if ((pixel == 1) && (chip8->display[screen_pixel_index] == 255)) {
                        chip8->display[screen_pixel_index] = 254;
                    }

                    x_coord++;
                    if (x_coord >= DISPLAY_WIDTH) break;
                }

                x_coord = x_coord_init;
                y_coord++;

                if (y_coord >= DISPLAY_HEIGHT) break;
            };
            break;
        }
        case 0xE:
            switch (nn) {
                case 0x9E: {
                    if (chip8->keypad[chip8->var_registers[x]]) {
                        chip8->pc += 2;
                    }
                    break;
                }
                case 0xA1: {
                    if (!chip8->keypad[chip8->var_registers[x]]) {
                        chip8->pc += 2;
                    }
                    break;
                }
                default:
                    printf("Unknown opcode: 0x%X\n", instruction);
                    return -1;
            }
            break;
        case 0xF:
            switch (nn) {
                case 0x07:
                    chip8->var_registers[x] = chip8->delay_timer;
                    break;
                case 0x15:
                    chip8->delay_timer = chip8->var_registers[x];
                    break;
                case 0x18:
                    chip8->sound_timer = chip8->var_registers[x];
                    break;
                case 0x1E:
                    chip8->index_register += chip8->var_registers[x];
                    break;
                case 0x0A: {
                    bool found = false;
                    for (int i = 0; i < 16; i++) {
                        if (chip8->keypad[i] == 1) {
                            chip8->var_registers[x] = i;
                            found = true;
                            break;
                        }
                    }
                    if (!found) chip8->pc -= 2;
                    break;
                }
                case 0x29:
                    chip8->index_register = FONT_ADDR + chip8->var_registers[x] * 5;
                    break;
                case 0x33: {
                    uint32_t vx = chip8->var_registers[x];
                    chip8->memory[chip8->index_register] = vx / 100;
                    chip8->memory[chip8->index_register + 1] = (vx / 10) % 10;
                    chip8->memory[chip8->index_register + 2] = vx % 10;
                    break;
                }
                case 0x55:
                    for (uint8_t reg = REG_V0; reg < x + 1; reg++) {
                        chip8->memory[chip8->index_register + reg] = chip8->var_registers[reg];
                    }
                    break;
                case 0x65:
                    for (uint8_t reg = REG_V0; reg < x + 1; reg++) {
                        chip8->var_registers[reg] = chip8->memory[chip8->index_register + reg];
                    }
                    break;
                default:
                    printf("Unknown opcode: 0x%X\n", instruction);
                    return -1;
            }
            break;
        default:
            printf("Unknown opcode: 0x%X\n", instruction);
            return -1;
    }
    return 0;
}

void chip8UpdateTimers(Chip8* chip8) {
    if (chip8->delay_timer > 0) chip8->delay_timer--;
    if (chip8->sound_timer > 0) chip8->sound_timer--;
}

void updateKeypad(Chip8* chip8) {
    if (IsKeyDown(KEY_X)) {
        chip8->keypad[0x0] = 1;
    } else if (IsKeyUp(KEY_X)) {
        chip8->keypad[0x0] = 0;
    }
    if (IsKeyDown(KEY_ONE)) {
        chip8->keypad[0x1] = 1;
    } else if (IsKeyUp(KEY_ONE)) {
        chip8->keypad[0x1] = 0;
    }
    if (IsKeyDown(KEY_TWO)) {
        chip8->keypad[0x2] = 1;
    } else if (IsKeyUp(KEY_TWO)) {
        chip8->keypad[0x2] = 0;
    }
    if (IsKeyDown(KEY_THREE)) {
        chip8->keypad[0x3] = 1;
    } else if (IsKeyUp(KEY_THREE)) {
        chip8->keypad[0x3] = 0;
    }
    if (IsKeyDown(KEY_Q)) {
        chip8->keypad[0x4] = 1;
    } else if (IsKeyUp(KEY_Q)) {
        chip8->keypad[0x4] = 0;
    }
    if (IsKeyDown(KEY_W)) {
        chip8->keypad[0x5] = 1;
    } else if (IsKeyUp(KEY_W)) {
        chip8->keypad[0x5] = 0;
    }
    if (IsKeyDown(KEY_E)) {
        chip8->keypad[0x6] = 1;
    } else if (IsKeyUp(KEY_E)) {
        chip8->keypad[0x6] = 0;
    }
    if (IsKeyDown(KEY_A)) {
        chip8->keypad[0x7] = 1;
    } else if (IsKeyUp(KEY_A)) {
        chip8->keypad[0x7] = 0;
    }
    if (IsKeyDown(KEY_S)) {
        chip8->keypad[0x8] = 1;
    } else if (IsKeyUp(KEY_S)) {
        chip8->keypad[0x8] = 0;
    }
    if (IsKeyDown(KEY_D)) {
        chip8->keypad[0x9] = 1;
    } else if (IsKeyUp(KEY_D)) {
        chip8->keypad[0x9] = 0;
    }
    if (IsKeyDown(KEY_Z)) {
        chip8->keypad[0xA] = 1;
    } else if (IsKeyUp(KEY_Z)) {
        chip8->keypad[0xA] = 0;
    }
    if (IsKeyDown(KEY_C)) {
        chip8->keypad[0xB] = 1;
    } else if (IsKeyUp(KEY_C)) {
        chip8->keypad[0xB] = 0;
    }
    if (IsKeyDown(KEY_FOUR)) {
        chip8->keypad[0xC] = 1;
    } else if (IsKeyUp(KEY_FOUR)) {
        chip8->keypad[0xC] = 0;
    }
    if (IsKeyDown(KEY_R)) {
        chip8->keypad[0xD] = 1;
    } else if (IsKeyUp(KEY_R)) {
        chip8->keypad[0xD] = 0;
    }
    if (IsKeyDown(KEY_F)) {
        chip8->keypad[0xE] = 1;
    } else if (IsKeyUp(KEY_F)) {
        chip8->keypad[0xE] = 0;
    }
    if (IsKeyDown(KEY_V)) {
        chip8->keypad[0xF] = 1;
    } else if (IsKeyUp(KEY_V)) {
        chip8->keypad[0xF] = 0;
    }
}

int main(int argc, char* argv[]) {
    Chip8 chip8;
    chip8Init(&chip8);
    chip8LoadRom(&chip8, "./roms/pong.rom");

    InitWindow(DISPLAY_WIDTH * SCALING, DISPLAY_HEIGHT * SCALING, "CHIP-8 emu");
    RenderTexture2D framebuffer = LoadRenderTexture(DISPLAY_WIDTH, DISPLAY_HEIGHT+1);

    BeginTextureMode(framebuffer);
    ClearBackground(BLACK);
    EndTextureMode();

    SetTargetFPS(60);

    Color colorPickerValue = RED;

    while (!WindowShouldClose()) {
        updateKeypad(&chip8);
        chip8UpdateTimers(&chip8);

        for (int throttling = 0; throttling < 10; throttling++) {
            chip8ProcessInstruction(&chip8);
        }

        BeginDrawing();

        BeginTextureMode(framebuffer);
        ClearBackground(BLACK);
        for (int i = 0; i < DISPLAY_WIDTH; i++)
        {
            for (int j = 0; j < DISPLAY_HEIGHT; j++)
            {
                uint8_t pixel = chip8.display[j * DISPLAY_WIDTH + i];
                if (pixel == 255) {
                    DrawPixel(i, DISPLAY_HEIGHT - j, WHITE);
                } else if (pixel < 255 && pixel > 60) {
                    uint32_t new_pixel = (uint32_t)((double)pixel * sqrt((double)pixel / 255));
                    Color color = {  new_pixel , new_pixel, new_pixel, 255 };
                    DrawPixel(i, DISPLAY_HEIGHT - j, color);
                    chip8.display[j * DISPLAY_WIDTH + i] -= 60;
                }
            }
        }
        EndTextureMode();

        DrawTextureEx(framebuffer.texture, (Vector2) {0, 0}, 0, SCALING, WHITE);
        DrawText(TextFormat("%0d fps", GetFPS()), 5, 5, 20, RED);

        EndDrawing();
    }

    UnloadRenderTexture(framebuffer);
    CloseWindow();


    return 0;
}