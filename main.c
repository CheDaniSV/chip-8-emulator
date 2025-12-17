#include <raylib.h>
#define RAYGUI_IMPLEMENTATION
#include <raygui.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define MAX_STACK_SIZE      4096
#define TIMER_STEP          1.0 / 60.0
#define FONT_MEM_LOC        0x0
#define PROGRAM_START       0x200
#define KEYS_NUM            16

// Emulator related
uint16_t cpu_speed; // Instructions per second
uint8_t memory[4096];
uint8_t V[16]; // General-purpose varibale registers (0-F)
uint16_t I; // Index register (points to memory locations)
uint16_t PC; // Program Counter register
uint8_t delay_timer; // Delay timer
uint8_t sound_timer; // Sound timer
uint8_t currently_loaded_font_type; // 0 - lowres, 1 - hires
bool is_rom_loaded;
bool step_by_step_mode;
bool step_one_instruction;

// Screen, display, UI related
uint8_t memory_heatmap[4096];
uint8_t screen_w;
uint8_t screen_h;
uint8_t *screen;
uint16_t d_x; // Display x pos
uint16_t d_y; // Display y pos
int16_t memory_heatmap_start = 0;
bool fullscreen_mode;
char quirks_button_text[3];
int16_t d_margin;
const uint8_t styles_count = 12;
uint8_t current_style;
Color style_colors[12] = {
    YELLOW,
    GOLD,
    PINK,
    RED,
    GREEN,
    LIME,
    BLUE,
    SKYBLUE,
    PURPLE,
    VIOLET,
    WHITE,
    BLACK
};
bool dark_mode;
bool show_debug_info;
bool show_message_box;
bool show_instruction;
char *message_box_title;
char *message_box_message;
char *message_box_buttons;
int message_box_text_alignment;
uint16_t message_box_width = 0;
uint16_t message_box_height = 0;
int16_t message_box_x = 0;
int16_t message_box_y = 0;
Vector2 box_mouse_dragging_delta_pos = {0};
float md_mouse_dragging_delta_pos_y = 0;
uint16_t memory_heatmap_start_when_dragging = {0};

// Keypad input related
bool waiting_for_key;
int key_released_this_frame;
int keypad[KEYS_NUM];
int chip8_keymap[KEYS_NUM] = {
    KEY_X,     // 0
    KEY_ONE,   // 1
    KEY_TWO,   // 2
    KEY_THREE, // 3
    KEY_Q,     // 4
    KEY_W,     // 5
    KEY_E,     // 6
    KEY_A,     // 7
    KEY_S,     // 8
    KEY_D,     // 9
    KEY_Z,     // A
    KEY_C,     // B
    KEY_FOUR,  // C
    KEY_R,     // D
    KEY_F,     // E
    KEY_V      // F
};

// Configuration variables related to quirks of superchip
bool superchip_shift;
bool superchip_offset_jump;
bool superchip_reg_mem_load;
bool superchip_no_reset_vf_on_bit_ops;
bool superchip_instructions_set; // Additional instructions for superchip

const char *instruction_text = 
"Keyboard layout:\n\
CHIP:\n\
1 2 3 C\n\
4 5 6 D\n\
7 8 9 E\n\
A 0 B F\n\
\n\
Emulator (modern keyboard):\n\
1 2 3 4\n\
Q W E R\n\
A S D F\n\
Z X C V\n\
\n\
Hotkeys:\n\
- F3 - display FPS & Time;\n\
- L - reset emulator state (unload program);\n\
- K - restart program;\n\
- J - toggle fullscreen mode;\n\
- H - toggle cpu speed;\n\
- M - enter step-by-step mode / pause;\n\
- N - step forward in step-by-step mode;\n\
- LSHIFT - switch dark mode;\n\
- TAB - switch style.\n\
\n\
To open the ROM drag & drop file into the window.\n\
Some buttons can be controlled with the mousewheel.";

// 4x5 font
uint8_t lowres_font_sprites[80] = {
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

// 8x10 font
uint8_t hires_font_sprites[512] = {
    0x00, 0x00, 0x3C, 0x00, 0x7E, 0x00, 0xE7, 0x00, 0xC3, 0x00, 0xC3, 0x00, 0xC3, 0x00, 0xC3, 0x00, 0xE7, 0x00, 0x7E, 0x00, 0x3C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0
    0x00, 0x00, 0x18, 0x00, 0x38, 0x00, 0x78, 0x00, 0x58, 0x00, 0x18, 0x00, 0x18, 0x00, 0x18, 0x00, 0x18, 0x00, 0x7E, 0x00, 0x7E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 1
    0x00, 0x00, 0x3C, 0x00, 0x7E, 0x00, 0xE7, 0x00, 0x03, 0x00, 0x06, 0x00, 0x0C, 0x00, 0x18, 0x00, 0x30, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 2
    0x00, 0x00, 0x3C, 0x00, 0x7E, 0x00, 0xE7, 0x00, 0x03, 0x00, 0x1E, 0x00, 0x1E, 0x00, 0x03, 0x00, 0xE7, 0x00, 0x7E, 0x00, 0x3C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 3
    0x00, 0x00, 0x06, 0x00, 0x0E, 0x00, 0x1E, 0x00, 0x36, 0x00, 0x66, 0x00, 0xC6, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0x06, 0x00, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 4
    0x00, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xC0, 0x00, 0xFC, 0x00, 0xFE, 0x00, 0x03, 0x00, 0x03, 0x00, 0xE7, 0x00, 0x7E, 0x00, 0x3C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 5
    0x00, 0x00, 0x3C, 0x00, 0x7E, 0x00, 0xE7, 0x00, 0xC0, 0x00, 0xFC, 0x00, 0xFE, 0x00, 0xE7, 0x00, 0xE7, 0x00, 0x7E, 0x00, 0x3C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 6
    0x00, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0x03, 0x00, 0x06, 0x00, 0x0C, 0x00, 0x18, 0x00, 0x30, 0x00, 0x60, 0x00, 0x60, 0x00, 0x60, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 7
    0x00, 0x00, 0x3C, 0x00, 0x7E, 0x00, 0xE7, 0x00, 0xE7, 0x00, 0x7E, 0x00, 0x7E, 0x00, 0xE7, 0x00, 0xE7, 0x00, 0x7E, 0x00, 0x3C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 8
    0x00, 0x00, 0x3C, 0x00, 0x7E, 0x00, 0xE7, 0x00, 0xE7, 0x00, 0x7F, 0x00, 0x3F, 0x00, 0x07, 0x00, 0xE7, 0x00, 0x7E, 0x00, 0x3C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 9
    0x00, 0x00, 0x3C, 0x00, 0x7E, 0x00, 0xE7, 0x00, 0xC3, 0x00, 0xC3, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xC3, 0x00, 0xC3, 0x00, 0xC3, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // A
    0x00, 0x00, 0xFC, 0x00, 0xFE, 0x00, 0xE7, 0x00, 0xE7, 0x00, 0xFE, 0x00, 0xFC, 0x00, 0xE7, 0x00, 0xE7, 0x00, 0xFE, 0x00, 0xFC, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // B
    0x00, 0x00, 0x3C, 0x00, 0x7E, 0x00, 0xE7, 0x00, 0xC0, 0x00, 0xC0, 0x00, 0xC0, 0x00, 0xC0, 0x00, 0xE7, 0x00, 0x7E, 0x00, 0x3C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // C
    0x00, 0x00, 0xFC, 0x00, 0xFE, 0x00, 0xE7, 0x00, 0xE7, 0x00, 0xE7, 0x00, 0xE7, 0x00, 0xE7, 0x00, 0xE7, 0x00, 0xFE, 0x00, 0xFC, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // D
    0x00, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xC0, 0x00, 0xC0, 0x00, 0xFE, 0x00, 0xFE, 0x00, 0xC0, 0x00, 0xC0, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // E
    0x00, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xC0, 0x00, 0xC0, 0x00, 0xFE, 0x00, 0xFE, 0x00, 0xC0, 0x00, 0xC0, 0x00, 0xC0, 0x00, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00  // F
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

void showMessageBox(const char *title, const char *message, const char *buttons, int textAlignment);

void loadROM(const char* path) {
    FILE *rom = fopen(path, "rb");
    if (rom == NULL) {
        showMessageBox("ERROR", "Cound't open the file.", "Close", TEXT_ALIGN_CENTER);
        return;
    }
    fseek(rom, 0, SEEK_END);
    size_t rom_size = ftell(rom);
    fseek(rom, 0, SEEK_SET);
    if (rom_size > sizeof(memory) - PROGRAM_START) {
        showMessageBox("ERROR", "ROM is too big.", "Close", TEXT_ALIGN_CENTER);
        return;
    }
    fread(memory + PROGRAM_START, 1, rom_size, rom);
    fclose(rom);
    is_rom_loaded = true;
}

// Instructions

// 00E0
void clearScreen(void) {
    memset(screen, 0, screen_w * screen_h);
}

// 00EE
void returnFromSubRoutine(void) {
    PC = popFromStack();
}

// 00CN
void scrollDisplayDownN(uint8_t N) {
    for (int16_t y = screen_h - 1; y >= 0; --y) {
        for (int16_t x = 0; x < screen_w; ++x) {
            if (y >= N) {
                screen[screen_w * y + x] = screen[screen_w * (y - N) + x];
            }
            else {
                screen[screen_w * y + x] = 0;
            }
        }
    }
};

// 00FB
void scrollDisplayRight(void) {
    for (int16_t y = 0; y < screen_h; ++y) {
        for (int16_t x = screen_w - 1; x >= 0; --x) {
            if (x > 3) {
                screen[screen_w * y + x] = screen[screen_w * y + (x - 4)];
            } else {
                screen[screen_w * y + x] = 0;
            }
        }
    }
}

// 00FC
void scrollDisplayLeft(void) {
    for (uint16_t y = 0; y < screen_h; ++y) {
        for (int16_t x = 0; x < screen_w; ++x) {
            if (x < screen_w - 4) {
                screen[screen_w * y + x] = screen[screen_w * y + (x + 4)];
            } else {
                screen[screen_w * y + x] = 0;
            }
        }
    }
}

// 1NNN
void jump(uint16_t addr) {
    PC = addr & 0xFFF;
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
    I = addr & 0xFFF;
}

// BNNN
void offsetJump(uint16_t addr) {
    PC = (addr + V[0]) & 0xFFF;
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

// DXY0
void drawHighRes(uint8_t regx_index, uint8_t regy_index) {
    uint16_t x0 = V[regx_index] & (screen_w - 1);
    uint16_t y0 = V[regy_index] & (screen_h - 1);
    V[0xF] = 0;
    for (uint8_t i = 0; i < 16; ++i) {
        uint16_t line = (memory[(I + i + i) & 0xFFF] << 8) | memory[(I + i + i + 1) & 0xFFF];
        memory_heatmap[(I + i + i) & 0xFFF] = 0xFF;
        memory_heatmap[(I + i + i + 1) & 0xFFF] = 0xFF;
        for (uint16_t j = 0; j < 16; ++j) {
            uint16_t pixel = (line >> (15 - j)) & 1;
            uint8_t x = x0 + j;
            uint8_t y = y0 + i;
            if (x >= screen_w || y >= screen_h) {
                break;
            }
            uint16_t px = screen_w * y + x;
            if (screen[px] && pixel) {
                V[0xF] = 1;
            }
            screen[px] ^= pixel;
        }
    }
}

// DXYN
void draw(uint8_t regx_index, uint8_t regy_index, uint8_t length) {
    uint16_t x0 = V[regx_index] & (screen_w - 1);
    uint16_t y0 = V[regy_index] & (screen_h - 1);
    V[0xF] = 0;
    for (uint8_t i = 0; i < length; ++i) {
        uint8_t line = memory[(I + i) & 0xFFF];
        memory_heatmap[(I + i) & 0xFFF] = 0xFF;
        for (uint8_t j = 0; j < 8; ++j) {
            uint8_t pixel = (line >> (7 - j)) & 1;
            uint16_t x = x0 + j;
            uint16_t y = y0 + i;
            if (x >= screen_w || y >= screen_h) {
                break;
            }
            uint16_t px = screen_w * y + x;
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
void setIToLowResFontChar(uint8_t reg_index) {
    I = FONT_MEM_LOC + (V[reg_index] & 0xF) * 5;
}

// FX30
void setIToHighResFontChar(uint8_t reg_index) {
    I = FONT_MEM_LOC + (V[reg_index] & 0xF) * 32;
}

// FX33
void binCodedDecimalConversion(uint8_t reg_index) {
    memory[I] = V[reg_index] / 100;
    memory[I + 1] = (V[reg_index] % 100) / 10;
    memory[I + 2] = V[reg_index] % 10;
    memory_heatmap[I] = 0xFF;
    memory_heatmap[I + 1] = 0xFF;
    memory_heatmap[I + 2] = 0xFF;
}

// FX55
void storeRegistersInMemory(uint8_t reg_index) {
    memcpy(memory + I, V, reg_index + 1);
    if (!superchip_reg_mem_load) {
        I += reg_index + 1;
    }
}

// FX65
void loadRegistersFromMemory(uint8_t reg_index) {
    memcpy(V, memory + I, reg_index + 1);
    if (!superchip_reg_mem_load) {
        I += reg_index + 1;
    }
}

// FX75
// Saves registers state into local storage (instead of real flag registers)
void saveRegStateToLocalStorage(uint8_t reg_index) {
    FILE *flag_registers = fopen("chipdata", "wb");
    if (flag_registers == NULL) {
        showMessageBox("ERROR", "Error opening/creating the chipdata file\nduring\nsaveRegStateToLocalStorage operation.", "Close", TEXT_ALIGN_CENTER);
        return;
    }
    fwrite(V, sizeof(uint8_t), reg_index + 1, flag_registers);
    fclose(flag_registers);
}

// FX85
// Loads registers state from local storage if possible
void loadRegStateFromLocalStorage(uint8_t reg_index) {
    FILE *flag_registers = fopen("chipdata", "rb");
    if (flag_registers == NULL) {
        memset(V, 0, reg_index + 1); // If there's no chipdata file, just set registers to 0
    }
    fseek(flag_registers, 0, SEEK_END);
    size_t bytes_in_file = ftell(flag_registers);
    fseek(flag_registers, 0, SEEK_SET);
    if (bytes_in_file > 16) {
        showMessageBox("INFO", "chipdata file this program is trying to use\nis too big,\npossibly corrupted.", "Close", TEXT_ALIGN_CENTER);
    }
    fread(V, sizeof(uint8_t), reg_index + 1, flag_registers);
    fclose(flag_registers);
}

void pollRaylibKeypadInput(void) {
    key_released_this_frame = -1;

    // 1. Detect held keys for EX9E & EXA1
    for (uint8_t i = 0; i < KEYS_NUM; ++i) {
        keypad[i] = IsKeyDown(chip8_keymap[i]);
    }

    // 2. Detect key release for FX0A
    for (uint8_t i = 0; i < KEYS_NUM; ++i) {
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

// Type:
// 0 - CHIP-8
// 1 - SUPER-CHIP
void setQuirks(uint8_t type) {
    if (type != 0) type = 1;
    superchip_shift = type;
    superchip_offset_jump = type;
    superchip_reg_mem_load = type;
    superchip_no_reset_vf_on_bit_ops = type;
}

// Set screen resolution
// Type:
// 0 or any number - 64x32
// 1 - 64x64
// 2 - 128x64
void setScreenMode(uint8_t type) {
    switch (type) {
        case 1:
            screen_w = 64;
            screen_h = 64;
            break;
        case 2:
            screen_w = 128;
            screen_h = 64;
            break;
        default: // default - CHIP-8
            screen_w = 64;
            screen_h = 32;
            break;
    }
    if (screen == 0) {
        screen = (uint8_t *)malloc(256 * 256 * sizeof(uint8_t));
    }
    clearScreen();
}

// Activates SUPER-CHIP instructions
// Type:
// 0 - CHIP-8
// 1 or any other - SUPER-CHIP
void setInstructions(uint8_t type) {
    if (type)
        superchip_instructions_set = true;
    else
        superchip_instructions_set = false;
}

// Loads fonts into the memory at 0x000 - 0x09F/0x1FF (depending on size)
// Type:
// 0 - lowres (80 bytes)
// 1 - hires (512 bytes)
// any other number - set font mem space to 0
void setFontType(uint8_t type) {
    memset(memory + FONT_MEM_LOC, 0, PROGRAM_START - 1);
    if (type == 0) {
        currently_loaded_font_type = 0;
        memcpy(memory + FONT_MEM_LOC, lowres_font_sprites, sizeof(lowres_font_sprites));
    } else if (type == 1) {
        currently_loaded_font_type = 1;
        memcpy(memory + FONT_MEM_LOC, hires_font_sprites, sizeof(hires_font_sprites));
    }
}

// Reset emulator or program to initial state
// Type:
// 0 - reset program
// 1 - unload program
// 2 or any number - reset emulator
void resetState(uint8_t type) {
    I = PROGRAM_START;
    PC = PROGRAM_START;
    delay_timer = 0;
    sound_timer = 0;
    stack.top = -1;
    memset(stack.arr, 0, MAX_STACK_SIZE);
    memset(memory_heatmap, 0, 4096);
    memset(V, 0, 16);
    memset(keypad, 0, KEYS_NUM);
    setInstructions(1);
    setFontType(0);
    if (screen != 0) {
        clearScreen();
    }
    if (type >= 1 || type < 0) {
        is_rom_loaded = false;
        memset(memory, 0, 4096);
        setScreenMode(0);
    }
    if (type >= 2 || type < 0) {
        show_instruction = false;
        fullscreen_mode = false;
        dark_mode = true;
        current_style = 5;
        d_margin = 0;
        setQuirks(0);
        cpu_speed = 700;
        step_by_step_mode = false;
        step_one_instruction = false;
        show_message_box = false;
        show_debug_info = false;
        strcpy(quirks_button_text, "CH");
    }
}

// Fetch / Decode / Execute Loop
void step_one_cycle(void) {
    // Fetch
    uint8_t b1 = memory[PC++];
    uint8_t nibble1 = b1 >> 4;
    uint8_t nibble2 = b1 & 0xF;
    uint8_t b2 = memory[PC++];
    uint8_t nibble3 = b2 >> 4;
    uint8_t nibble4 = b2 & 0xF;
    uint16_t opcode = (b1 << 8) | b2;
    uint16_t addr = (nibble2 << 8) | b2; // NNN

    memory_heatmap[PC - 2] = 0xFF;
    memory_heatmap[PC - 1] = 0xFF;

    // If end of the memory is reached
    if (PC - 1 >= 0xFFF) {
        jump(PROGRAM_START);
        return;
    }

    // Init 64x64 hires mode
    if (PC == (PROGRAM_START + 2) && opcode == 0x1260) {
        setScreenMode(1);
        jump(0x2C0);
        return;
    }

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
                case 0x00FE:
                    // lores mode
                    if (superchip_instructions_set) {
                        setScreenMode(0); // 00FE
                    }
                    break;
                case 0x00FB:
                    if (superchip_instructions_set) {
                        scrollDisplayRight(); // 00FB
                    }
                    break;
                case 0x00FC:
                    if (superchip_instructions_set) {
                        scrollDisplayLeft(); // 00FC
                    }
                    break;
                case 0x00FF:
                    // hires mode
                    if (superchip_instructions_set) {
                        setScreenMode(2); // 00FF
                    }
                    break;
                case 0x00FD:
                    if (superchip_instructions_set) {
                        exit(0);
                    }
                    break;
            }
            if (b1 == 0x0 && nibble3 == 0xC && superchip_instructions_set) {
                scrollDisplayDownN(nibble4); // 00CN
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
            if (nibble4 == 0x0 && superchip_instructions_set) {
                drawHighRes(nibble2, nibble3);
            } else {
                draw(nibble2, nibble3, nibble4); // DXYN
            }
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
                    if (currently_loaded_font_type != 0) {
                        setFontType(0);
                    }
                    setIToLowResFontChar(nibble2); // FX29
                    break;
                case 0x30:
                    if (superchip_instructions_set) {
                        if (currently_loaded_font_type == 0) {
                            setFontType(1);
                        }
                        setIToHighResFontChar(nibble2); // FX30
                    }
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
                case 0x75:
                    if (superchip_instructions_set) {
                        saveRegStateToLocalStorage(nibble2); // FX75
                    }
                    break;
                case 0x85:
                    if (superchip_instructions_set) {
                        loadRegStateFromLocalStorage(nibble2); // FX85
                    }
                    break;
            }
            break;
        default:
            break;
    }
}

void showMessageBox(const char *title, const char *message, const char *buttons, int textAlignment) {
    message_box_text_alignment = textAlignment;
    show_message_box = true;

    message_box_width = 300;
    message_box_height = 200;
    uint16_t line_count = 0;
    for (uint16_t i = 0; i < strlen(message); ++i)
        if (message[i] == '\n')
            ++line_count;

    if (message_box_width - 2 * RAYGUI_MESSAGEBOX_BUTTON_PADDING <= GetTextWidth(message))
        message_box_width = GetTextWidth(message) + 2 * RAYGUI_MESSAGEBOX_BUTTON_PADDING;

    float fontSize = (float)GuiGetStyle(DEFAULT, TEXT_SIZE);
    if ((message_box_height - RAYGUI_WINDOWBOX_STATUSBAR_HEIGHT - 3 * RAYGUI_MESSAGEBOX_BUTTON_PADDING - RAYGUI_MESSAGEBOX_BUTTON_HEIGHT) < (fontSize * line_count))
        message_box_height = line_count * fontSize + line_count * GuiGetStyle(DEFAULT, TEXT_SIZE) / 2 + RAYGUI_WINDOWBOX_STATUSBAR_HEIGHT + 3 * RAYGUI_MESSAGEBOX_BUTTON_PADDING + RAYGUI_MESSAGEBOX_BUTTON_HEIGHT;

    message_box_x = GetScreenWidth() / 2 - message_box_width / 2;
    message_box_y = GetScreenHeight() / 2 - message_box_height / 2;

    unsigned long long char_size = sizeof(char);
    message_box_title = realloc(message_box_title, (strlen(title) + 1) * char_size);
    message_box_message = realloc(message_box_message, (strlen(message) + 1) * char_size);
    message_box_buttons = realloc(message_box_buttons, (strlen(buttons) + 1) * char_size);
    strcpy(message_box_title, title);
    strcpy(message_box_message, message);
    strcpy(message_box_buttons, buttons);
}

// Almost the same as GuiMessageBox(), but adds textAlignment, and supports only one button
int customMessageBox(Rectangle bounds, const char *title, const char *message, const char *button, int textAlignment) {
    int result = -1;

    Rectangle buttonBound = { 0 };
    buttonBound.x = bounds.x + RAYGUI_MESSAGEBOX_BUTTON_PADDING;
    buttonBound.y = bounds.y + bounds.height - RAYGUI_MESSAGEBOX_BUTTON_HEIGHT - RAYGUI_MESSAGEBOX_BUTTON_PADDING;
    buttonBound.width = bounds.width - 2 * RAYGUI_MESSAGEBOX_BUTTON_PADDING;
    buttonBound.height = RAYGUI_MESSAGEBOX_BUTTON_HEIGHT;

    int textWidth = GetTextWidth(message) + 2;

    Rectangle textBounds = { 0 };
    textBounds.x = bounds.x + GuiGetStyle(DEFAULT, TEXT_SIZE);
    textBounds.y = bounds.y + RAYGUI_WINDOWBOX_STATUSBAR_HEIGHT + RAYGUI_MESSAGEBOX_BUTTON_PADDING;
    textBounds.width = bounds.width - 2 * GuiGetStyle(DEFAULT, TEXT_SIZE);
    textBounds.height = bounds.height - RAYGUI_WINDOWBOX_STATUSBAR_HEIGHT - 3 * RAYGUI_MESSAGEBOX_BUTTON_PADDING - RAYGUI_MESSAGEBOX_BUTTON_HEIGHT;
    if (GuiWindowBox(bounds, title)) result = 0;

    int prevTextAlignment = GuiGetStyle(LABEL, TEXT_ALIGNMENT);
    GuiSetStyle(LABEL, TEXT_ALIGNMENT, textAlignment);
    GuiLabel(textBounds, message);
    GuiSetStyle(LABEL, TEXT_ALIGNMENT, prevTextAlignment);

    prevTextAlignment = GuiGetStyle(BUTTON, TEXT_ALIGNMENT);
    GuiSetStyle(BUTTON, TEXT_ALIGNMENT, TEXT_ALIGN_CENTER);

    if (GuiButton(buttonBound, button))
        result = 1;

    GuiSetStyle(BUTTON, TEXT_ALIGNMENT, prevTextAlignment);

    return result;
}

void raylibProcess() {

    // Raylib events (not all events are here, some are inline in UI code)
    if (IsFileDropped()) {
        FilePathList droppedFiles = LoadDroppedFiles();
        resetState(1);
        loadROM(droppedFiles.paths[0]);
        UnloadDroppedFiles(droppedFiles);
    }

    if (IsKeyPressed(KEY_F3)) show_debug_info = !show_debug_info;
    if (IsKeyPressed(KEY_L)) resetState(1);
    if (IsKeyPressed(KEY_K)) resetState(0);
    if (IsKeyPressed(KEY_J)) fullscreen_mode = !fullscreen_mode;
    if (IsKeyPressed(KEY_M)) step_by_step_mode = !step_by_step_mode;
    if (IsKeyDown(KEY_N)) step_one_instruction = true;
    if (IsKeyPressed(KEY_TAB)) {
        // Yes, it's a of code from button, but I don't want to make a function for that
        if (current_style + 1 < styles_count)
                    ++current_style;
                else
                    current_style = 0;
                if (current_style == 11)
                    dark_mode = false;
                else if (current_style == 10)
                    dark_mode = true;
                else if (current_style == 0)
                    dark_mode = false;
    }
    if (IsKeyPressed(KEY_LEFT_SHIFT))
        if (current_style != 10 && current_style != 11)
            dark_mode = !dark_mode;

    // Drawing
    BeginDrawing();
        int16_t global_margin = 16;
        int16_t border_width = 2;
        int16_t border_margin = 1;
        uint16_t d_px_size = 0;

        // Styles
        Color main_foreground = style_colors[current_style];
        Color main_background;
        if (dark_mode)
            main_background = ColorBrightness(main_foreground, -0.95);
        else
            main_background = ColorBrightness(main_foreground, 0.9);
        Color secondary_color = Fade(main_foreground, 0.1);
        ClearBackground(main_background);
        Color main_text_color;
        if (main_background.r + main_background.g + main_background.b < 128)
            main_text_color = WHITE;
        else 
            main_text_color = BLACK;

        // Generate style base on theme color
        GuiSetStyle(BUTTON, BORDER_COLOR_PRESSED, ColorToInt(ColorBrightness(main_foreground, 0.3)));
        GuiSetStyle(BUTTON, BORDER_COLOR_FOCUSED, ColorToInt(ColorBrightness(main_foreground, -0.3)));
        GuiSetStyle(BUTTON, BORDER_COLOR_NORMAL, ColorToInt(main_foreground));
        
        GuiSetStyle(BUTTON, BASE_COLOR_PRESSED, ColorToInt(ColorBrightness(secondary_color, 0.5)));
        GuiSetStyle(BUTTON, BASE_COLOR_FOCUSED, ColorToInt(ColorBrightness(secondary_color, -0.5)));
        GuiSetStyle(BUTTON, BASE_COLOR_NORMAL, ColorToInt(secondary_color));
        
        if (dark_mode)
            GuiSetStyle(BUTTON, TEXT_COLOR_PRESSED, ColorToInt(ColorBrightness(main_text_color, 0.3)));
        else 
            GuiSetStyle(BUTTON, TEXT_COLOR_PRESSED, ColorToInt(ColorBrightness(main_text_color, -0.3)));
        GuiSetStyle(BUTTON, TEXT_COLOR_FOCUSED, ColorToInt(ColorBrightness(main_text_color, -0.3)));
        GuiSetStyle(BUTTON, TEXT_COLOR_NORMAL, ColorToInt(main_text_color));

        // Style for messagebox, instruction window // TODO: It that correct?
        GuiSetStyle(STATUSBAR, BORDER_COLOR_NORMAL, ColorToInt(main_foreground));
        GuiSetStyle(STATUSBAR, BASE_COLOR_NORMAL, ColorToInt(main_background));
        GuiSetStyle(STATUSBAR, TEXT_COLOR_NORMAL, ColorToInt(main_text_color));
        GuiSetStyle(DEFAULT, BACKGROUND_COLOR, ColorToInt(main_background));
        GuiSetStyle(DEFAULT, LINE_COLOR, ColorToInt(main_foreground));
        GuiSetStyle(LABEL, BORDER_COLOR_NORMAL, ColorToInt(main_foreground));
        GuiSetStyle(LABEL, BASE_COLOR_NORMAL, ColorToInt(secondary_color));
        GuiSetStyle(LABEL, TEXT_COLOR_NORMAL, ColorToInt(main_text_color));

        // Emulator display
        if (fullscreen_mode) {
            uint16_t scaleX = (GetScreenWidth() - 2 * border_margin - 2 * border_width) / screen_w;
            uint16_t scaleY = (GetScreenHeight() - 2 * border_margin - 2 * border_width) / screen_h;
            d_px_size = (scaleX < scaleY) ? scaleX : scaleY;
            d_x = (GetScreenWidth() - d_px_size * screen_w - 2 * border_margin - 2 * border_width) / 2;
            d_y = (GetScreenHeight() - d_px_size * screen_h - 2 * border_margin - 2 * border_width) / 2;
        } else {
            d_px_size = GetScreenWidth() * 0.7 / screen_w;
            if ((d_px_size * screen_h + 2 * border_margin + 2 * border_width) > GetScreenHeight()) {
                d_px_size = (GetScreenHeight() - 2 * border_width - 2 * border_margin) / screen_h;
            }
            d_x = GetScreenWidth() - d_px_size * screen_w - border_margin - border_width - global_margin;
            d_y = global_margin + border_margin + border_width;
        }

        DrawRectangle(d_x - border_margin, d_y - border_margin, screen_w * d_px_size + 2 * border_margin - d_margin, screen_h * d_px_size + 2 * border_margin - d_margin, secondary_color);
        DrawRectangle(d_x - border_width - border_margin, d_y - border_width - border_margin, screen_w * d_px_size + 2 * border_width + 2 * border_margin - d_margin, border_width, main_foreground);
        DrawRectangle(d_x - border_width - border_margin, d_y + screen_h * d_px_size + border_margin - d_margin, screen_w * d_px_size + 2 * border_width + 2 * border_margin - d_margin, border_width, main_foreground);
        DrawRectangle(d_x - border_width - border_margin, d_y - border_margin, border_width, screen_h * d_px_size + 2 * border_margin - d_margin, main_foreground);
        DrawRectangle(d_x + screen_w * d_px_size + border_margin - d_margin, d_y - border_margin, border_width, screen_h * d_px_size + 2 * border_margin - d_margin, main_foreground);

        for (int16_t y = 0; y < screen_h; ++y) {
            for (int16_t x = 0; x < screen_w; ++x) {
                if (screen[screen_w*y+x]) DrawRectangle(d_x + x * d_px_size, d_y + y * d_px_size, d_px_size - d_margin, d_px_size - d_margin, main_foreground);
            }
        }

        int16_t md_row_length = 32;
        int16_t md_row_num = 32;
        int16_t md_x = global_margin + border_margin + border_width;
        int16_t md_margin = 1;
        int16_t md_cell_size = (d_x - 32) / md_row_length;
        if ((md_cell_size * md_row_length + 2 * border_margin + 2 * border_width) > GetScreenWidth() * 0.3)
            md_cell_size = GetScreenWidth() * 0.3 / md_row_length;
        // int16_t md_y = GetScreenHeight() - md_row_num * md_cell_size - 2 * border_margin - 2 * border_width - 8;
        // int16_t md_y = d_y + d_px_size * screen_h - md_row_num * md_cell_size;
        int16_t md_y = global_margin + border_margin + border_width;
        int16_t md_lud_x = md_x - border_width - border_margin; // left, up, down border Xpos
        int16_t md_r_x = md_x + md_row_length * md_cell_size + border_margin - md_margin; // right border Xpos
        int16_t md_d_y = md_y + md_row_num * md_cell_size + border_margin - md_margin; // bottom border Ypos
        int16_t md_u_y = md_y - border_width - border_margin; // upper border Ypos
        int16_t md_lr_y = md_y - border_margin; // left, right border Ypos
        int16_t md_ud_w = md_row_length * md_cell_size + 2 * border_width + 2 * border_margin - md_margin; // up, down border width
        int16_t md_lr_h = md_row_num * md_cell_size + 2 * border_margin - md_margin; // left, right border hight

        // Memory heatmap display
        if (!fullscreen_mode) {
            DrawRectangle(md_x - border_margin, md_y - border_margin, md_cell_size * md_row_length + 2 * border_margin - md_margin, md_cell_size * md_row_num + 2 * border_margin - md_margin, secondary_color);
            DrawRectangle(md_lud_x, md_u_y, md_ud_w, border_width, main_foreground);
            DrawRectangle(md_lud_x, md_d_y, md_ud_w, border_width, main_foreground);
            DrawRectangle(md_lud_x, md_lr_y, border_width, md_lr_h, main_foreground);
            DrawRectangle(md_r_x, md_lr_y, border_width, md_lr_h, main_foreground);

            if (GetMouseX() > md_lud_x && GetMouseX() < md_r_x && GetMouseY() < md_d_y && GetMouseY() > md_u_y) {
                memory_heatmap_start -= (int16_t)GetMouseWheelMove() * md_row_length;
            }

            if (GetMouseX() > md_lud_x && GetMouseX() < md_r_x && GetMouseY() < md_d_y && GetMouseY() > md_u_y 
                && IsMouseButtonDown(MOUSE_BUTTON_LEFT) && box_mouse_dragging_delta_pos.x == 0 && box_mouse_dragging_delta_pos.y == 0
                && (((GetMouseX() < message_box_x || GetMouseX() > message_box_x + message_box_width) && (GetMouseY() < message_box_y || GetMouseY() > message_box_y + message_box_height)) || !show_message_box
                    || ((GetMouseX() < message_box_x || GetMouseX() > message_box_x + message_box_width) && GetMouseY() > message_box_y && GetMouseY() < message_box_y + message_box_height)
                    || ((GetMouseY() < message_box_y || GetMouseY() > message_box_y + message_box_height) && GetMouseX() > message_box_x && GetMouseX() < message_box_x + message_box_width))) {
                if (md_mouse_dragging_delta_pos_y == 0 && memory_heatmap_start_when_dragging == 0) {
                    md_mouse_dragging_delta_pos_y = GetMouseY();
                    memory_heatmap_start_when_dragging = memory_heatmap_start;
                }
                memory_heatmap_start = memory_heatmap_start_when_dragging - (GetMouseY() - md_mouse_dragging_delta_pos_y) * md_row_length;
            } else {
                    md_mouse_dragging_delta_pos_y = 0;
                    memory_heatmap_start_when_dragging = 0;
            }

            if (memory_heatmap_start < 0) {
                memory_heatmap_start = 0;
            } else if (memory_heatmap_start >= 4096 - md_row_num * md_row_length) {
                memory_heatmap_start = 4096 - md_row_num * md_row_length;
            }

            for (int16_t i = memory_heatmap_start; i < memory_heatmap_start + md_row_num * md_row_length; ++i) {
                Color cell_color;
                if (i == PC) {
                    cell_color = RED;
                } else if (i == PROGRAM_START) {
                    cell_color = DARKBLUE;
                } else if ((currently_loaded_font_type == 0 && i < 0x50) || (currently_loaded_font_type == 1 && i < PROGRAM_START)) {
                    cell_color = BLUE;
                } else if (memory[i] == 0x00) {
                    cell_color = GRAY;
                } else {
                    cell_color = GREEN;
                }
                double brightness = (memory_heatmap[i] / 255.0) - 0.3;
                if (brightness < 0)
                    brightness = 0;
                DrawRectangle(md_x + (i - (int) memory_heatmap_start) % md_row_length * md_cell_size,
                            md_y + (i - (int) memory_heatmap_start) / md_row_length * md_cell_size,
                            md_cell_size - md_margin, md_cell_size - md_margin, memory_heatmap[i] == 0 ? cell_color : ColorBrightness(cell_color, brightness));
            }
        }

        if (!is_rom_loaded) {
            uint16_t font_size = d_px_size * 4;
            DrawText("Drag & Drop", d_x + d_px_size * screen_w / 2.0 - font_size * 3.0, d_y + d_px_size * screen_h / 2.0 - font_size / 2.0, font_size, main_foreground);
        }

        if (!fullscreen_mode) {
            // Draw registers
            for (int i = 0; i < 16; ++i) {
                char reg_info[16];
                if (V[i] < 0x10) {
                    sprintf(reg_info, "%X: 0%X", i, V[i]);
                } else {
                    sprintf(reg_info, "%X: %X", i, V[i]);
                }
                if (i < 8)
                    DrawText(reg_info, md_x + i * md_cell_size * 4, md_y + md_lr_h + 8, md_cell_size, main_text_color);
                else 
                    DrawText(reg_info, md_x + i % 8 * md_cell_size * 4, md_y + md_lr_h + 2 * 8 + md_cell_size, md_cell_size, main_text_color);
            }
            char pc_info[16];
            char i_info[16];
            char opcode_info[16];
            if (I < 0x10)
                sprintf(i_info, "i: 00%X", I);
            else if (I < 0x100)
                sprintf(i_info, "i: 0%X", I);
            else
                sprintf(i_info, "i: %X", I);
            if (PC < 0x10)
                sprintf(pc_info, "i: 00%X", PC);
            else if (PC < 0x100)
                sprintf(pc_info, "i: 0%X", PC);
            else
                sprintf(pc_info, "i: %X", PC);
            sprintf(opcode_info, "OP: %X", ((memory[PC] << 8) | memory[PC + 1]));
            DrawText(i_info, md_x, md_y + md_lr_h + 3 * 8 + 2 * md_cell_size, md_cell_size, main_text_color);
            DrawText(pc_info, md_x + md_cell_size * 4, md_y + md_lr_h + 3 * 8 + 2 * md_cell_size, md_cell_size, main_text_color);
            DrawText(opcode_info, md_x + md_cell_size * 8, md_y + md_lr_h + 3 * 8 + 2 * md_cell_size, md_cell_size, main_text_color);

            uint16_t button_size_with_margin = md_ud_w / 5;
            if (button_size_with_margin > 64)
                button_size_with_margin = 64;
            uint16_t button_y_dest = md_y + md_lr_h + 4 * 8 + 3 * md_cell_size;
            uint16_t button_x_dest = md_x - border_margin - border_width;
            uint16_t button_margin = 4;
            uint16_t button_size = button_size_with_margin - button_margin;

            if (GuiButton((Rectangle){ button_x_dest, button_y_dest, button_size, button_size}, "help")) {
                showMessageBox("Instruction", instruction_text, "Close", TEXT_ALIGN_LEFT);
            }
            if (GuiButton((Rectangle){ button_x_dest + button_size_with_margin, button_y_dest, button_size, button_size}, "#107#"))
                fullscreen_mode = true;
            if (GuiButton((Rectangle){ button_x_dest + 2 * button_size_with_margin, button_y_dest, button_size, button_size}, dark_mode ? "#157#" : "#156#")) {
                if (current_style != 10 && current_style != 11)
                    dark_mode = !dark_mode;
            }
            if (GuiButton((Rectangle){ button_x_dest + 3 * button_size_with_margin, button_y_dest, button_size, button_size}, "#29#")) {
                if (current_style + 1 < styles_count)
                    ++current_style;
                else
                    current_style = 0;
                if (current_style == 11)
                    dark_mode = false;
                else if (current_style == 10)
                    dark_mode = true;
                else if (current_style == 0)
                    dark_mode = false;
            }

            char d_margin_text[4]; sprintf(d_margin_text, "%dpx", d_margin);
            if (GuiButton((Rectangle){ button_x_dest + 4 * button_size_with_margin, button_y_dest, button_size, button_size}, d_margin_text)) {
                if (d_margin == 0)
                    d_margin = 1;
                else if (d_margin == 1)
                    d_margin = 2;
                else if (d_margin == 2)
                    d_margin = 0;
            }

            if (GuiButton((Rectangle){ button_x_dest, button_y_dest + button_size + button_margin, button_size, button_size}, quirks_button_text)) {
                if (quirks_button_text[0] == 'S') {
                    setQuirks(0);
                    strcpy(quirks_button_text, "CH");
                } else {
                    setQuirks(1);
                    strcpy(quirks_button_text, "SC");
                }
            }

            if (GetMouseX() >= button_x_dest + button_size_with_margin && GetMouseX() <= button_x_dest + button_size_with_margin + button_size
                && GetMouseY() >= button_y_dest + button_size + button_margin && GetMouseY() <= button_y_dest + 2 * button_size + button_margin
                ) {
                cpu_speed += (uint16_t)GetMouseWheelMove();
            }

            char cpu_speed_text[16]; sprintf(cpu_speed_text, "%dhz", cpu_speed);
            if (GuiButton((Rectangle){ button_x_dest + button_size_with_margin, button_y_dest + button_size + button_margin, button_size, button_size}, cpu_speed_text) || IsKeyPressed(KEY_H)) {
                if (cpu_speed < 700)
                    cpu_speed = 700;
                else if (cpu_speed < 1000 && cpu_speed >= 700)
                    cpu_speed = 1000;
                else if (cpu_speed < 1500 && cpu_speed >= 1000)
                    cpu_speed = 1500;
                else if (cpu_speed < 2100 && cpu_speed >= 1500)
                    cpu_speed = 2100;
                else if (cpu_speed >= 2100)
                    cpu_speed = 0;
            };

            if (GuiButton((Rectangle){ button_x_dest + 2 * button_size_with_margin, button_y_dest + button_size + button_margin, button_size, button_size}, "#76#")) {
                resetState(0);
            };

            if (GuiButton((Rectangle){ button_x_dest + 3 * button_size_with_margin, button_y_dest + button_size + button_margin, button_size, button_size}, step_by_step_mode ? "#131#" : "#132#")) {
                step_by_step_mode = !step_by_step_mode;
                step_one_instruction = false;
            };

            if ((GetMouseX() >= button_x_dest + 4 * button_size_with_margin && GetMouseX() <= button_x_dest + 4 * button_size_with_margin + button_size
                && GetMouseY() >= button_y_dest + button_size + button_margin && GetMouseY() <= button_y_dest + 2 * button_size + button_margin)) {
                if (step_by_step_mode) step_one_instruction = (GetMouseWheelMove() != 0);
            }

            if (GuiButton((Rectangle){ button_x_dest + 4 * button_size_with_margin, button_y_dest + button_size + button_margin, button_size, button_size}, "#119#")) {
                if (step_by_step_mode) step_one_instruction = true;
            }
        } else {
            GuiSetStyle(BUTTON, BASE_COLOR_NORMAL, ColorToInt(main_background)); // To keep base_color opaque
            GuiSetStyle(BUTTON, BASE_COLOR_FOCUSED, ColorToInt(main_background));
            GuiSetStyle(BUTTON, BASE_COLOR_PRESSED, ColorToInt(main_background));
            if (GuiButton((Rectangle){ 16, GetScreenHeight() - 48, 32, 32}, "#103#"))
                fullscreen_mode = false;
        }

        if (show_message_box) {
            if (GetMouseX() >= message_box_x && GetMouseX() <= message_box_x + message_box_width - 20
            && GetMouseY() >= message_box_y &&  GetMouseY() <= message_box_y + RAYGUI_WINDOWBOX_STATUSBAR_HEIGHT
            && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
                if (box_mouse_dragging_delta_pos.x == 0 && box_mouse_dragging_delta_pos.y == 0) {
                    box_mouse_dragging_delta_pos.x = GetMouseX() - message_box_x;
                    box_mouse_dragging_delta_pos.y = GetMouseY() - message_box_y;
                }
                message_box_x = GetMouseX() - box_mouse_dragging_delta_pos.x;
                message_box_y = GetMouseY() - box_mouse_dragging_delta_pos.y;
            } else if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
                if (box_mouse_dragging_delta_pos.x != 0 && box_mouse_dragging_delta_pos.y != 0) {
                    message_box_x = GetMouseX() - box_mouse_dragging_delta_pos.x;
                    message_box_y = GetMouseY() - box_mouse_dragging_delta_pos.y;
                }
            } else {
                box_mouse_dragging_delta_pos.x = 0;
                box_mouse_dragging_delta_pos.y = 0;
            }
            if (customMessageBox((Rectangle){ message_box_x, message_box_y, message_box_width, message_box_height},
                message_box_title, message_box_message, message_box_buttons, message_box_text_alignment) >= 0)
                show_message_box = false;
        }

        if(show_debug_info) {
            DrawFPS(global_margin, GetScreenHeight() - 32);
            char info1[32]; sprintf(info1, "Time: %.3f", GetTime());
            DrawText(info1, 2 * global_margin + 20 * 4, GetScreenHeight() - 32, 20, main_text_color);
        }
        EndDrawing();
}

int main() {
    resetState(2);

    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(900, 600, "CHIP Emulator");
    SetWindowMinSize(850, 500);
    SetTargetFPS(60);
    InitAudioDevice();
    Sound beep = generateBeep(440);
    SetSoundVolume(beep, 0.1f);

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
            for (uint16_t i = 0; i < 4096; ++i) {
                if (memory_heatmap[i] > 0 && memory_heatmap[i] - 5 >= 0) {
                    memory_heatmap[i] -= 5;
                } else {
                    memory_heatmap[i] = 0;
                }
            }
        }

        cpu_accumulator += delta_seconds * cpu_speed;

        if (step_by_step_mode) {
            cpu_accumulator = 0;
        }
        if (step_one_instruction && step_by_step_mode) {
            step_one_cycle();
            step_one_instruction = false;
        }

        while (cpu_accumulator >= 1.0) {
            step_one_cycle();
            cpu_accumulator -= 1.0;
        }

        raylibProcess();
    }

    CloseAudioDevice();
    CloseWindow();
    free(screen);
    free(message_box_title);
    free(message_box_message);
    free(message_box_buttons); 
    return 0;
}
