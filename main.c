// #include "raylib.h"
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#ifdef _WIN32
    #include <windows.h>
    #define sleep_ms(ms) Sleep(ms)
    uint64_t get_time_ms(void) { return GetTickCount64(); }
#elif defined(__linux__)
    #include <unistd.h>
    #define sleep_ms(ms) usleep(ms * 1000)
    #include <sys/time.h>
    uint64_t get_time_ms(void) {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        return (uint64_t)tv.tv_sec * 1000ULL + tv.tv_usec / 1000ULL;
#endif

#define MAX_STACK_SIZE 32768
#define SCREEN_W 64
#define SCREEN_H 32
#define SCREEN_BUFFER_SIZE SCREEN_W*SCREEN_H*6*2 + 128 // 6 byte max per char + 2x in x direction + 128 just in case
#define TIMER_STEP 1000.0 / 60.0 // for sound & delay timers
// #define FRAME_STEP 1000.0 / 60.0

double CPU_SPEED = 700.0; // instructions per second
uint8_t memory[4096];
uint8_t V[16]; // general-purpose varibale registers (0-F)
uint16_t I; // index register (point to memory locations)
uint16_t PC; // program counter register
uint8_t delay_timer; // delay timer
uint8_t sound_timer; // sound timer
bool screen[SCREEN_W*SCREEN_H];
bool is_render_needed;
uint16_t cycles_per_frame;

// Stack Implementation
typedef struct
{
    uint16_t arr[MAX_STACK_SIZE];
    short top; // 32767 + 1 max size
} Stack;

Stack stack; // 16-bit stack of memory addresses

// TODO: either move these functions (stack implementation) 
// into separate header
// or make them procedures (without args, on one global stack)
bool initialize(Stack *stack) {
    return stack->top = -1;
}

bool isEmpty(Stack *stack) {
    return stack->top == -1;
}

bool isFull(Stack *stack) {
    return stack->top == MAX_STACK_SIZE - 1;
}

void push(Stack *stack, uint16_t value) {
    if (isFull(stack)) {
        printf("Stack Overflow\n"); // TODO: throw an error?
        return;
    }
    stack->arr[++(stack->top)] = value;
    printf("%u\n", stack->top);
}

uint16_t pop(Stack *stack) {
    if (isEmpty(stack)) {
        printf("Stack Underflow\n"); // TODO: throw an error?
        return 0;
    }
    return stack->arr[(stack->top)--];
}

// CLI render
// As fast as it gets ¯\_(ツ)_/¯
char screen_buffer[SCREEN_BUFFER_SIZE];
const char pixel_on_esc_color_code[5] = "\x1b[42m";
const char pixel_off_esc_color_code[5] = "\x1b[0m";
void renderToCLI() {
    if (is_render_needed) {
        memset(screen_buffer, 0, SCREEN_BUFFER_SIZE);
        char *p = screen_buffer;
        memcpy(p, "\x1b[H", 3);
        p += 3;
        memcpy(p, pixel_off_esc_color_code, 5);
        p += 5;
        bool is_current_color_on = false;
        for (int y = 0; y < SCREEN_H; ++y) {
            for (int x = 0; x < SCREEN_W; ++x) {
                bool pixel_on = screen[SCREEN_W*y+x];
                // We change background only if it needed
                // it makes buffer smaller & display much faster but
                // can produce artifacts :(
                // FIX: later, for now pixel-off background is no color (terminal color)
                if (pixel_on != is_current_color_on) {
                    is_current_color_on = pixel_on;
                    memcpy(p, pixel_on ? pixel_on_esc_color_code : pixel_off_esc_color_code, 5);
                    p += 5;
                }
                // *p++ = ' ';
                memcpy(p, "  ", 2); // rendering 2x chars in x direction, to make them look square
                p += 2;
            }
            *p++ = '\n';
        }
        memcpy(p, "\x1b[0m", 4);
        p += 4;
        fwrite(screen_buffer, 1, p - screen_buffer, stdout);
        is_render_needed = false;
    }
}

void loadROM(const char* path) {
    FILE *rom = fopen(path, "r");
    if (rom == NULL) {
        perror("Error opening the file");
        exit(1);
    }
    fseek(rom, 0, SEEK_END);
    size_t rom_size = ftell(rom);
    fseek(rom, 0, SEEK_SET);
    if (rom_size > sizeof(memory) - 0x200) {
        fprintf(stderr, "ROM is too big...\n");
        exit(1);
    }
    fread(memory + 0x200, 1, rom_size, rom);
    fclose(rom);
}

// Instructions

// 1NNN
void jump(uint16_t address) {
    PC = address;
}

// 6XNN
void setVX(uint8_t reg_index, uint8_t number) {
    V[reg_index] = number;
}

// 7XNN
void addToVX(uint8_t reg_index, uint8_t number) {
    V[reg_index] += number; // VF isn't changed in case of overflow
}

// ANNN
void setI(uint16_t number) {
    I = number;
}

// 00E0
void clearScreen() {
    is_render_needed = true;
    memset(screen, 0, sizeof(screen));
}

// DXYN
void draw(uint8_t regx_index, uint8_t regy_index, uint8_t length) {
    is_render_needed = true;
    uint16_t x0 = (V[regx_index] & (SCREEN_W - 1));
    uint16_t y0 = (V[regy_index] & (SCREEN_H - 1));
    V[0xF] = 0;
    for (uint8_t i = 0; i < length; ++i) {
        uint8_t line = memory[(I + i) & 0xFFF];
        uint8_t output[8] = {0, 0, 0, 0, 0, 0, 0, 0}; // 8-bit :)
        for (uint8_t j = 0; line > 0; ++j) {
            output[7 - j] = line & 1; // line % 2
            line /= 2;
        }
        for (uint8_t j = 0; j < 8; ++j) {
            uint16_t x = x0 + j;
            uint16_t y = y0 + i;
            if (x >= SCREEN_W || y >= SCREEN_H) {
                break;
            }
            uint16_t px = SCREEN_W * y + x;
            if (screen[px] && output[j]) {
                V[0xF] = 1;
            }
            screen[px] ^= output[j];
        }
    }
}

// Fetch / Decode / Execute Loop
void step_one_cycle() {
    // Fetch
    uint16_t b1 = memory[PC++ & 0xFFF];
    uint16_t nibble1 = b1 >> 4;
    uint16_t nibble2 = b1 & 0xF;
    uint16_t b2 = memory[PC++ & 0xFFF];
    uint16_t nibble3 = b2 >> 4;
    uint16_t nibble4 = b2 & 0xF;
    uint16_t opcode = b1 << 8 | b2;
    // printf("%X: %X %X %X | %X %X %X %X\n", PC-2, b1, b2, opcode, nibble1, nibble2, nibble3, nibble4);
    // printf("%X: %X \n", PC-2, opcode);
    // Decode & Execute
    switch (nibble1) {
        case 0x0:
            switch(opcode) {
                case 0x00E0:
                    clearScreen();
                    break;
                case 0x000:
                    break;
                default:
                    printf("Unknown instruction: %X\n", opcode);
                    break;
            }
            break;
        case 0x1:
            jump(nibble2 * 0x100 + nibble3 * 0x10 + nibble4);
            break;
        case 0x6:
            setVX(nibble2, nibble3 * 0x10 + nibble4);
            break;
        case 0x7:
            addToVX(nibble2, nibble3 * 0x10 + nibble4);
            break;
        case 0xA:
            setI(nibble2 * 0x100 + nibble3 * 0x10 + nibble4);
            break;
        case 0xD:
            draw(nibble2, nibble3, nibble4);
            break;
        default:
            printf("Unknown instruction: %X\n", opcode);
            break;
    }
}

int main() {
    // Initial values
    cycles_per_frame = 150;
    memset(memory, 0, sizeof(memory));
    memset(V, 0, sizeof(V));
    I = 0x200;
    PC = 0x200;
    delay_timer = 0;
    sound_timer = 0;
    is_render_needed = false;
    initialize(&stack);

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

    // Load fonts into memory at 0x050 - 0x09F
    memcpy(memory + 0x050, font_sprites, sizeof(font_sprites));

    loadROM("IBM Logo.ch8");
    // loadROM("fonttest.ch8");

    // printf("%zu %zu %zu %zu", sizeof(memory), sizeof(uint8_t), sizeof(I), sizeof(stack));

    // memory[0x200] = 0x60; memory[0x201] = 0x01; // 0x6001 // set V0 = 1 
    // memory[0x202] = 0x61; memory[0x203] = 0x01; // 0x6101 // set V1 = 1
    // I = 0x050;

    // TODO: move into program as .ch8, when all instructions are implemented
    // Test draw char
    // for (int i = 0; i < 16; ++i) {
    //     I = 0x050+i*5;
    //     if (i < 8) {
    //         V[0] = 1 + i*5;
    //         V[1] = 1;
    //     } else {
    //         V[0] = 1 + (i-8)*5;
    //         V[1] = 7;
    //     }
    //     draw(0, 1, 5);
    // }
    // for (int i = 0; i < 16; ++i) {
    //     memory[PC++] = 0xA0;
    //     memory[PC++] = 0x50 + i*5;
    //     // I = 0x050+i*5;
    //     if (i < 8) {
    //         memory[PC++] = 0x60;
    //         memory[PC++] = 0x01 + i*5;
    //         // V[0] = 1 + i*5;
    //         memory[PC++] = 0x61;
    //         memory[PC++] = 0x01; 
    //         // V[1] = 1;
    //     } else {
    //         memory[PC++] = 0x60;
    //         memory[PC++] = 0x01 + (i-8)*5;
    //         // V[0] = 1 + (i-8)*5;
    //         memory[PC++] = 0x61;
    //         memory[PC++] = 0x07;
    //         // V[1] = 7;
    //     }
    //     memory[PC++] = 0xD0;
    //     memory[PC++] = 0x15;
    // }

    // InitWindow(600, 576, "raylib on w64devkit + VS Code");
    // SetTargetFPS(60);

    // while (!WindowShouldClose()) {
    //     BeginDrawing();
    //     ClearBackground(RED);

    //     DrawText("Raylib - The Grid", 10, 10, 20, WHITE);
    //     uint8_t info1[12]; sprintf(info1, "Passed: %.2f", GetTime());
    //     DrawText(info1, 10, 40, 20, WHITE);

    //     // DrawRectangleLines(30, 30, 40, 40, WHITE);

    //     EndDrawing();
    // }

    // CloseWindow();

    // TODO: argv, argc

    uint64_t last_cycle_time = get_time_ms();
    double timer_accumulator = 0.0;
    double cpu_accumulator = 0.0;

    while (true) {
        uint64_t current_cycle_time = get_time_ms();
        double delta_ms = (current_cycle_time - last_cycle_time);
        last_cycle_time = current_cycle_time;
        timer_accumulator += delta_ms;

        while (timer_accumulator >= TIMER_STEP) {
            if (delay_timer > 0) {
                --delay_timer;
            }
            if (sound_timer > 0) {
                --sound_timer;
                // if (sound_timer == 0) {
                    // TODO: beep here
                // }
            }
            timer_accumulator -= TIMER_STEP;
        }

        cpu_accumulator += delta_ms / 1000.0 * CPU_SPEED;
        while (cpu_accumulator >= 1.0) {
            step_one_cycle();
            cpu_accumulator -= 1.0;
        }

        renderToCLI(); // maybe lock to 60fps with accumulators too...

        sleep_ms(1); // prevent 100% CPU usage
    }
    return 0;
}
