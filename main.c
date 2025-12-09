#include <raylib.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define MAX_STACK_SIZE      32768
#define SCREEN_W            64
#define SCREEN_H            32
#define TIMER_STEP          1.0 / 60.0
#define FONT_MEM_LOC        0x050
#define PROGRAM_START       0x200
#define KEYS_NUM            16

uint16_t cpu_speed; // Instructions per second
uint8_t memory[4096] = {0};
uint8_t V[16] = {0}; // General-purpose varibale registers (0-F)
uint16_t I; // Index register (points to memory locations)
uint16_t PC; // Program Counter register
uint8_t delay_timer; // Delay timer
uint8_t sound_timer; // Sound timer

// Screen related
bool screen[SCREEN_W*SCREEN_H] = {0};
int screen_x_pos;
int screen_y_pos;
int screen_pixel_size;

// Keypad input related
bool waiting_for_key;
int key_released_this_frame;
uint8_t keypad[KEYS_NUM] = {0};
int chip8_keymap[KEYS_NUM] = {
    KEY_X,    // 0
    KEY_ONE,  // 1
    KEY_TWO,  // 2
    KEY_THREE,// 3
    KEY_Q,    // 4
    KEY_W,    // 5
    KEY_E,    // 6
    KEY_A,    // 7
    KEY_S,    // 8
    KEY_D,    // 9
    KEY_Z,    // A
    KEY_C,    // B
    KEY_FOUR, // C
    KEY_R,    // D
    KEY_F,    // E
    KEY_V     // F
};

// Configuration variables related to quirks and superchip mode
bool superchip_shift;
bool superchip_offset_jump;
bool superchip_reg_mem_load;
bool superchip_no_reset_vf_on_bit_ops;

uint8_t font_sprites[80] = {
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

// Stack Implementation
typedef struct
{
    uint16_t arr[MAX_STACK_SIZE];
    short top; // 32767 + 1 max size
} Stack;

Stack stack; // 16-bit stack of memory addresses

bool isStackEmpty(void) {
    stack.top == -1;
}

bool isStackFull(void) {
    return stack.top == MAX_STACK_SIZE - 1;
}

void pushToStack(uint16_t value) {
    if (isStackFull()) {
        return;
    }
    stack.arr[++(stack.top)] = value;
}

uint16_t popFromStack(void) {
    if (isStackEmpty()) {
        return 1;
    }
    return stack.arr[(stack.top)--];
}

// CLI render
// #define SCREEN_BUFFER_SIZE SCREEN_W * SCREEN_H * 11 * 2 + SCREEN_H + 3
// char screen_buffer[SCREEN_BUFFER_SIZE];
// const char pixel_on_esc_color_code[5] = "\x1b[44m";
// const char pixel_off_esc_color_code[5] = "\x1b[40m";
// void renderToCLI(void) {
//     memset(screen_buffer, 0, SCREEN_BUFFER_SIZE);
//     char *p = screen_buffer;
//     memcpy(p, "\x1b[H", 3);
//     p += 3;
//     for (int y = 0; y < SCREEN_H; ++y) {
//         for (int x = 0; x < SCREEN_W; ++x) {
//             memcpy(p, screen[SCREEN_W*y+x] ? pixel_on_esc_color_code : pixel_off_esc_color_code, 5);
//             p += 5;
//             memcpy(p, "  ", 2);
//             p += 2;
//             memcpy(p, "\x1b[0m", 4);
//             p += 4;
//         }
//         *p++ = '\n';
//     }
//     fwrite(screen_buffer, 1, p - screen_buffer, stdout);
// }

void loadROM(const char* path) {
    FILE *rom = fopen(path, "rb");
    if (rom == NULL) {
        perror("Error opening the file"); // TODO: make them raylib message-box
    }
    fseek(rom, 0, SEEK_END);
    size_t rom_size = ftell(rom);
    fseek(rom, 0, SEEK_SET);
    if (rom_size > sizeof(memory) - PROGRAM_START) {
        fprintf(stderr, "ROM is too big...\n"); // TODO: make them raylib message-box
    }
    fread(memory + PROGRAM_START, 1, rom_size, rom);
    fclose(rom);
}

// Instructions

// 00E0
void clearScreen(void) {
    memset(screen, 0, sizeof(screen));
}

// 00EE
void returnFromSubRoutine(void) {
    PC = popFromStack();
}

// 1NNN
void jump(uint16_t addr) {
    PC = addr;
}

// 2NNN
void execSubroutine(uint16_t addr) {
    pushToStack(PC);
    PC = addr;
}

// 3XNN
void skipIfVxEqNN(uint8_t reg_index, uint8_t num) {
    if (V[reg_index] == num) {
        PC += 2;
    }
}

// 4XNN
void skipIfVxNotEqNN(uint8_t reg_index, uint8_t num) {
    if (V[reg_index] != num) {
        PC += 2;
    }
}

// 5XY0
void skipIfVxEqVy(uint8_t regx_index, uint8_t regy_index) {
    if (V[regx_index] == V[regy_index]) {
        PC += 2;
    }
}

// 6XNN
void setVXNN(uint8_t reg_index, uint8_t num) {
    V[reg_index] = num;
}

// 7XNN
void addNNToVX(uint8_t reg_index, uint8_t num) {
    V[reg_index] += num; // vF isn't changed in case of overflow
}

// 8XY0
void setVxVy(uint8_t regx_index, uint8_t regy_index) {
    V[regx_index] = V[regy_index];
}

// 8XY1
void orVxVy(uint8_t regx_index, uint8_t regy_index) {
    V[regx_index] |= V[regy_index];
    if (!superchip_no_reset_vf_on_bit_ops) {
        V[0xF] = 0;
    }
}

// 8XY2
void andVxVy(uint8_t regx_index, uint8_t regy_index) {
    V[regx_index] &= V[regy_index];
    if (!superchip_no_reset_vf_on_bit_ops) {
        V[0xF] = 0;
    }
}

// 8XY3
void xorVxVy(uint8_t regx_index, uint8_t regy_index) {
    V[regx_index] ^= V[regy_index];
    if (!superchip_no_reset_vf_on_bit_ops) {
        V[0xF] = 0;
    }
}

// 8XY4
void addVxVy(uint8_t regx_index, uint8_t regy_index) {
    uint8_t vF;
    if ((V[regx_index] + V[regy_index]) > 255) {
        vF = 1;
    } else {
        vF = 0;
    }
    V[regx_index] += V[regy_index];
    V[0xF] = vF;
}

// 8XY5
void subtractVxVy(uint8_t regx_index, uint8_t regy_index) {
    uint8_t vF;
    if (V[regx_index] < V[regy_index]) {
        vF = 0;
    } else {
        vF = 1;
    }
    V[regx_index] -= V[regy_index];
    V[0xF] = vF;
}

// 8XY6
void shiftVxRight(uint8_t regx_index, uint8_t regy_index) {
    if (!superchip_shift) {
        V[regx_index] = V[regy_index];
    }
    uint8_t vF;
    if (V[regx_index] & 0x1) {
        vF = 1;
    } else {
        vF = 0;
    }
    V[regx_index] >>= 1;
    V[0xF] = vF;
}

// 8XY7
void subtractVyVx(uint8_t regx_index, uint8_t regy_index) {
    uint8_t vF;
    if (V[regy_index] < V[regx_index]) {
        vF = 0;
    } else {
        vF = 1;
    }
    V[regx_index] = V[regy_index] - V[regx_index];
    V[0xF] = vF;
}

// 8XYE
void shiftVxLeft(uint8_t regx_index, uint8_t regy_index) {
    if (!superchip_shift) {
        V[regx_index] = V[regy_index];
    }
    uint8_t vF;
    if ((V[regx_index] & 0x80) >> 7) {
        vF = 1;
    } else {
        vF = 0;
    }
    V[regx_index] <<= 1;
    V[0xF] = vF;
}

// 9XY0
void skipIfVXNotEqVy(uint8_t regx_index, uint8_t regy_index) {
    if (V[regx_index] != V[regy_index]) {
        PC += 2;
    }
}

// ANNN
void setINNN(uint16_t addr) {
    I = addr;
}

// BNNN
void offsetJump(uint16_t addr) {
    PC = addr + V[0];
}

// BXNN (BNNN) superchip quirk behaviour
// XNN address + VX register
void offsetJumpSC(uint8_t reg_index, uint8_t addr) {
    PC = (reg_index << 8) + addr + V[reg_index];
}

// CXNN
void randomNNToVx(uint8_t reg_index, uint8_t num) {
    V[reg_index] = (GetRandomValue(0, UINT16_MAX) & num);
}

// DXYN
void draw(uint8_t regx_index, uint8_t regy_index, uint8_t length) {
    uint16_t x0 = V[regx_index] & (SCREEN_W - 1);
    uint16_t y0 = V[regy_index] & (SCREEN_H - 1);
    V[0xF] = 0;
    for (uint8_t i = 0; i < length; ++i) {
        uint8_t line = memory[(I + i) & 0xFFF];
        for (uint8_t j = 0; j < 8; ++j) {
            uint8_t pixel = (line >> (7 - j)) & 1;
            uint16_t x = x0 + j;
            uint16_t y = y0 + i;
            if (x >= SCREEN_W || y >= SCREEN_H) {
                break;
            }
            uint16_t px = SCREEN_W * y + x;
            if (screen[px] && pixel) {
                V[0xF] = 1;
            }
            screen[px] ^= pixel;
        }
    }
}

// EX9E
void skipIfKeyPressed(uint8_t reg_index) {
    if(keypad[V[reg_index]]) {
        PC += 2;
    }
}

// EXA1
void skipIfKeyNotPressed(uint8_t reg_index) {
    if(!keypad[V[reg_index]]) {
        PC += 2;
    }
}

// FX07
void setVxToDTimer(uint8_t reg_index) {
    V[reg_index] = delay_timer;
}

// FX0A
void getKey(uint8_t reg_index) {
    if (!waiting_for_key) {
        waiting_for_key = true;
    }
    if (key_released_this_frame != -1) {
        V[reg_index] = key_released_this_frame;
        waiting_for_key = false;
        key_released_this_frame = -1;
    }
    if (waiting_for_key) {
        PC -= 2;
    }
}

// FX15
void setDTimerToVx(uint8_t reg_index) {
    delay_timer = V[reg_index];
}

// FX18
void setSTimerToVx(uint8_t reg_index) {
    sound_timer = V[reg_index];
}

// FX1E
void addToI(uint8_t reg_index) {
    uint8_t vF;
    if (I + V[reg_index] > 0xFFF) {
        vF = 1;
    } else {
        vF = 0;
    }
    I += V[reg_index];
    V[0xF] = vF;
}

// FX29
void setIToFontChar(uint8_t reg_index) {
    I = FONT_MEM_LOC + (V[reg_index] & 0xF) * 5;
}

// FX33
void binCodedDecimalConversion(uint8_t reg_index) {
    memory[I] = V[reg_index] / 100;
    memory[I + 1] = (V[reg_index] % 100) / 10;
    memory[I + 2] = V[reg_index] % 10;
}

// FX55 - Store registers V0 to VX in memory starting at I
void storeRegistersInMemory(uint8_t reg_index) {
    memcpy(memory + I, V, reg_index + 1);
    if (!superchip_reg_mem_load) {
        I += reg_index + 1;
    }
}

// FX65 - Load registers V0 to VX from memory starting at I
void loadRegistersFromMemory(uint8_t reg_index) {
    memcpy(V, memory + I, reg_index + 1);
    if (!superchip_reg_mem_load) {
        I += reg_index + 1;
    }
}

// Fetch / Decode / Execute Loop
void step_one_cycle(void) {
    // Fetch
    uint8_t b1 = memory[PC++ & 0xFFF];
    uint8_t nibble1 = b1 >> 4;
    uint8_t nibble2 = b1 & 0xF;
    uint8_t b2 = memory[PC++ & 0xFFF];
    uint8_t nibble3 = b2 >> 4;
    uint8_t nibble4 = b2 & 0xF;
    uint16_t opcode = (b1 << 8) | b2;
    uint16_t addr = (nibble2 << 8) | b2; // NNN
    // printf("%X: %X %X %X | %X %X %X %X\n", PC-2, b1, b2, opcode, nibble1, nibble2, nibble3, nibble4);
    // printf("%X: %X \n", PC-2, opcode);

    // Decode & Execute
    switch (nibble1) {
        case 0x0:
            switch(opcode) {
                case 0x00E0:
                    clearScreen(); // 00E0
                    break;
                case 0x00EE:
                    returnFromSubRoutine(); // 00EE
                    break;
            }
            break;
        case 0x1:
            jump(addr); // 1NNN
            break;
        case 0x2:
            execSubroutine(addr); // 2NNN
            break;
        case 0x3:
            skipIfVxEqNN(nibble2, b2); // 3XNN
            break;
        case 0x4:
            skipIfVxNotEqNN(nibble2, b2); // 4XNN
            break;
        case 0x5:
            if (nibble4 == 0)
                skipIfVxEqVy(nibble2, nibble3); // 5XY0
            break;
        case 0x6:
            setVXNN(nibble2, b2); // 6XNN
            break;
        case 0x7:
            addNNToVX(nibble2, b2); // 7XNN
            break;
        case 0x8:
            switch (nibble4) {
                case 0x0:
                    setVxVy(nibble2, nibble3); // 8XY0
                    break;
                case 0x1:
                    orVxVy(nibble2, nibble3); // 8XY1
                    break;
                case 0x2:
                    andVxVy(nibble2, nibble3); // 8XY2
                    break;
                case 0x3:
                    xorVxVy(nibble2, nibble3); // 8XY3
                    break;
                case 0x4:
                    addVxVy(nibble2, nibble3); // 8XY4
                    break;
                case 0x5:
                    subtractVxVy(nibble2, nibble3); // 8XY5
                    break;
                case 0x6:
                    shiftVxRight(nibble2, nibble3); // 8XY6
                    break;
                case 0x7:
                    subtractVyVx(nibble2, nibble3); // 8XY7
                    break;
                case 0xE:
                    shiftVxLeft(nibble2, nibble3); // 8XYE
                    break;
            }
            break;
        case 0x9:
            if (nibble4 == 0)
                skipIfVXNotEqVy(nibble2, nibble3); // 9XY0
            break;
        case 0xA:
            setINNN(addr); // ANNN
            break;
        case 0xB:
            if (superchip_offset_jump) {
                offsetJumpSC(nibble2, b2); // BXNN
            } else {
                offsetJump(addr); // BNNN
            }
            break;
        case 0xC:
            randomNNToVx(nibble2, b2); // CXNN
            break;
        case 0xD:
            draw(nibble2, nibble3, nibble4); // DXYN
            break;
        case 0xE:
            switch(b2) {
                case 0x9E:
                    skipIfKeyPressed(nibble2); // EX9E
                    break;
                case 0xA1:
                    skipIfKeyNotPressed(nibble2); // EXA1
                    break;
            }
            break;
        case 0xF:
            switch(b2) {
                case 0x07:
                    setVxToDTimer(nibble2); // FX07
                    break;
                case 0x0A:
                    getKey(nibble2); // FX0A
                    break;
                case 0x15:
                    setDTimerToVx(nibble2); // FX15
                    break;
                case 0x18:
                    setSTimerToVx(nibble2); // FX18
                    break;
                case 0x1E:
                    addToI(nibble2); // FX1E
                    break;
                case 0x29:
                    setIToFontChar(nibble2); // FX29
                    break;
                case 0x33:
                    binCodedDecimalConversion(nibble2); // FX33
                    break;
                case 0x55:
                    storeRegistersInMemory(nibble2); // FX55
                    break;
                case 0x65:
                    loadRegistersFromMemory(nibble2); // FX65
                    break;
            }
            break;
        default:
            break;
    }
}

void pollRaylibKeypadInput(void) {
    key_released_this_frame = -1;

    // 1. Detect held keys for EX9E & EXA1
    for (int i = 0; i < KEYS_NUM; ++i) {
        keypad[i] = IsKeyDown(chip8_keymap[i]);
    }

    // 2. Detect key release for FX0A
    for (int i = 0; i < KEYS_NUM; ++i) {
        if (IsKeyReleased(chip8_keymap[i])) {
            key_released_this_frame = i;
        }
    }

}

Sound generateBeep(int frequency) {
    const int sample_rate = 44000;
    const int sample_size = 16;
    const float duration_sec = 1.0f;

    int num_samples = (int)(duration_sec * sample_rate);
    Wave wave = {0};
    wave.frameCount = num_samples;
    wave.sampleRate = sample_rate;
    wave.sampleSize = sample_size;
    wave.channels = 1;

    short *samples = (short *)malloc(num_samples * sizeof(short));
    int period = sample_rate / frequency;

    for (int i = 0; i < num_samples; i++) {
        samples[i] = ((i % period) < (period / 2)) ? 32767 : -32768;
    }

    wave.data = samples;
    Sound beep = LoadSoundFromWave(wave);
    UnloadWave(wave); // frees samples as well

    return beep;
}

void superchipConfig(bool flag) {
    superchip_shift = flag;
    superchip_offset_jump = flag;
    superchip_reg_mem_load = flag;
    superchip_no_reset_vf_on_bit_ops = flag;
}

int main(void) {
    // Initialization
    cpu_speed = 700;
    I = PROGRAM_START;
    PC = PROGRAM_START;
    delay_timer = 0;
    sound_timer = 0;
    stack.top = -1;
    superchipConfig(true);
    

    // Load fonts into memory at 0x050 - 0x09F
    memcpy(memory + FONT_MEM_LOC, font_sprites, sizeof(font_sprites));

    // loadROM("IBM Logo.ch8");
    // loadROM("ROMs/chip-8-roms/programs/Chip8 Picture.ch8");
    // loadROM("ROMs/chip-8-roms/programs/Chip8 emulator Logo [Garstyciuks].ch8");
    // loadROM("ROMs/chip-8-roms/programs/Random Number Test [Matthew Mikolay, 2010].ch8");
    // loadROM("ROMs/chip-8-roms/demos/Particle Demo [zeroZshadow, 2008].ch8");
    // loadROM("ROMs/chip-8-roms/programs/Division Test [Sergey Naydenov, 2010].ch8");
    // loadROM("ROMs/chip-8-roms/programs/SQRT Test [Sergey Naydenov, 2010].ch8");
    loadROM("ROMs/chip-8-roms/programs/Delay Timer Test [Matthew Mikolay, 2010].ch8");
    
    // loadROM("ROMs/chip8-test-rom-master/test_opcode.ch8");
    // loadROM("ROMs/test-suite/1-chip8-logo.ch8");
    // loadROM("ROMs/test-suite/2-ibm-logo.ch8");
    // loadROM("ROMs/test-suite/3-corax+.ch8");
    // loadROM("ROMs/test-suite/4-flags.ch8");
    // loadROM("ROMs/test-suite/5-quirks.ch8");
    // loadROM("ROMs/test-suite/6-keypad.ch8");
    // loadROM("ROMs/test-suite/7-beep.ch8");
    // loadROM("ROMs/test-suite/8-scrolling.ch8");
    
    // loadROM("tests.ch8");

    // TODO: add argv, argc handling

    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(900, 600, "Chip8 Emulator");
    SetWindowMinSize(600, 400);
    // SetTargetFPS(60);
    InitAudioDevice();
    Sound beep = generateBeep(440); // Classic CHIP-8 beep (A4 note — tune if you want)
    SetSoundVolume(beep, 0.25f); // 50% volume — adjust as needed

    double last_cycle_time = GetTime();
    double timer_accumulator = 0.0;
    double cpu_accumulator = 0.0;

    while (!WindowShouldClose()) {
        double current_cycle_time = GetTime();
        double delta_seconds = (current_cycle_time - last_cycle_time);
        last_cycle_time = current_cycle_time;
        timer_accumulator += delta_seconds;

        pollRaylibKeypadInput();

        while (timer_accumulator >= TIMER_STEP) {
            if (delay_timer > 0) {
                --delay_timer;
            }
            if (sound_timer > 0) {
                --sound_timer;
                if (!IsSoundPlaying(beep)) {
                    PlaySound(beep);
                }
            } else {
                if (IsSoundPlaying(beep)) {
                    StopSound(beep);
                }
            }
            timer_accumulator -= TIMER_STEP;
        }

        cpu_accumulator += delta_seconds * cpu_speed;
        while (cpu_accumulator >= 1.0) {
            step_one_cycle();
            cpu_accumulator -= 1.0;
        }

        BeginDrawing();
        ClearBackground(BLACK);
        DrawText("Raylib - Chip8 EMULATOR", 10, 10, 20, WHITE);
        DrawFPS(10, 40);
        char info1[32]; sprintf(info1, "Time: %.3f s", GetTime());
        char info3[32]; sprintf(info3, "Stimer: %d", sound_timer);
        char info4[32]; sprintf(info4, "Dtimer: %d", sound_timer);
        // uint8_t info5[32]; sprintf(info5, "CPU_ACC: %.2f", cpu_accumulator);
        char info6[32]; sprintf(info6, "CPU SPEED: %d hz", cpu_speed);
        DrawText(info1, 10, 70, 20, WHITE);
        // DrawText(info2, 10, 70, 20, WHITE);
        DrawText(info3, 10, 100, 20, WHITE);
        DrawText(info4, 10, 130, 20, WHITE);
        // DrawText(info5, 10, 160, 20, WHITE);
        DrawText(info6, 10, 190, 20, WHITE);

        cpu_speed -= (int)GetMouseWheelMove() * 10;

        int margin = 0;
        int border_width = 3;
        int border_margin = 1;

        screen_pixel_size = GetScreenWidth() / 100;
        screen_x_pos = GetScreenWidth() - screen_pixel_size * SCREEN_W - 16;
        screen_y_pos = GetScreenHeight() - screen_pixel_size * SCREEN_H - 16;

        Color border_color = LIME;
        Color pixel_on_color = LIME;
        Color pixel_off_color = BLACK;
        DrawRectangle(screen_x_pos - border_width - border_margin, screen_y_pos - border_width - border_margin, SCREEN_W * screen_pixel_size + 2 * border_width + 2 * border_margin, border_width, border_color);
        DrawRectangle(screen_x_pos - border_width - border_margin, screen_y_pos + SCREEN_H * screen_pixel_size + border_margin, SCREEN_W * screen_pixel_size + 2 * border_width + 2 * border_margin, border_width, border_color);
        DrawRectangle(screen_x_pos - border_width - border_margin, screen_y_pos - border_margin, border_width, SCREEN_H * screen_pixel_size + 2 * border_margin, border_color);
        DrawRectangle(screen_x_pos + SCREEN_W * screen_pixel_size + border_margin, screen_y_pos - border_margin, border_width, SCREEN_H * screen_pixel_size + 2 * border_margin, border_color);
        for (int y = 0; y < SCREEN_H; ++y) {
            for (int x = 0; x < SCREEN_W; ++x) {
                DrawRectangle(screen_x_pos + x*screen_pixel_size + margin, screen_y_pos + y*screen_pixel_size + margin, screen_pixel_size - margin*2, screen_pixel_size - margin*2, screen[SCREEN_W*y+x] ? pixel_on_color : pixel_off_color);
            }
        }

        EndDrawing();
    }

    CloseAudioDevice();
    CloseWindow();
    return 0;
}
