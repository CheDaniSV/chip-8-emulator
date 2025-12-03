// #include "raylib.h"
#include <stdio.h>
#include <stdbool.h>

typedef unsigned short ushort;
typedef unsigned char uchar;

// Stack Implementation
#define MAX_STACK_SIZE 32768

typedef struct
{
    ushort arr[MAX_STACK_SIZE];
    short top; // 32767 + 1 max size
} Stack;

bool initialize(Stack *stack) {
    return stack->top = -1;
}

bool isEmpty(Stack *stack) {
    return stack->top == -1;
}

bool isFull(Stack *stack) {
    return stack->top == MAX_STACK_SIZE - 1;
}

void push(Stack *stack, ushort value) {
    if (isFull(stack)) {
        printf("Stack Overflow\n"); // TODO: throw an error?
        return;
    }
    stack->arr[++(stack->top)] = value;
    printf("%u\n", stack->top);
}

ushort pop(Stack *stack) {
    if (isEmpty(stack)) {
        printf("Stack Underflow\n"); // TODO: Error
        return 0;
    }
    return stack->arr[(stack->top)--];
}

void renderToCLI(bool *screen, ushort width, ushort height) {
    for (ushort i = 0; i < height; ++i) {
        for (ushort j = 0; j < width; ++j) {
            const char* px = screen[width*i+j] ? "\x1b[42m \x1b[0m" : "\x1b[40m \x1b[0m";
            fwrite(px, 1, 10, stdout);
        }
        putchar('\n');
    }
}

// Instructions
void clearScreen(bool *screen, ushort width, ushort height) {
    for (ushort i = 0; i < width*height; i++)
        screen[i] = 0;
}

int main() {
    // InitWindow(600, 576, "raylib on w64devkit + VS Code");
    // SetTargetFPS(60);

    // while (!WindowShouldClose()) {
    //     BeginDrawing();
    //     ClearBackground(RED);

    //     DrawText("Raylib - The Grid", 10, 10, 20, WHITE);
    //     uchar info1[12]; sprintf(info1, "Passed: %.2f", GetTime());
    //     DrawText(info1, 10, 40, 20, WHITE);

    //     // DrawRectangleLines(30, 30, 40, 40, WHITE);

    //     EndDrawing();
    // }

    // CloseWindow();

    // TODO: argv, argc

    uchar memory[4096];
    uchar V[16]; // general-purpose varibale registers (0-F)
    ushort I; // index register (point to memory locations)
    ushort PC = 0x0; // program counter register
    Stack stack; // 16-bit stack of memory addresses
    initialize(&stack);

    // TODO: Temporary
    for (int i = 0; i < 4096; ++i)
        memory[i] = 0;

    uchar font_sprites[5*16] = {
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

    // load fonts in memory 050 - 09F
    for (ushort i = 0; i < 5*16; ++i) {
        memory[0x050+i] = font_sprites[i];
    }

    // uchar test = 0x42;
    // printf("%hhX -> %c \n", test, test);

    // printf("%zu %zu %zu %zu", sizeof(memory), sizeof(uchar), sizeof(I), sizeof(stack));

    bool screen[64*32];
    // for (int i = 0; i < 32; ++i) {
    //     for (int j = 0; j < 64; ++j) {
    //         if (i%2 == 0) {
    //             screen[64*i+j] = 0;
    //         } else {
    //             screen[64*i+j] = 1;
    //         }
    //     }
    // }

    memory[0x200] = 0x00;
    memory[0x201] = 0xE0;

    // fetch/decode/execute loop
    while (true) {
        if (PC < 0x201) {
            // fetch
            ushort b1 = memory[PC++];
            ushort nibble1 = b1/0x10; // 4-bit (half-byte part)
            ushort nibble2 = b1%0x10;
            ushort b2 = memory[PC++];
            ushort nibble3 = b2/0x10;
            ushort nibble4 = b2%0x10;
            ushort current_instruction = b1*0x100 + b2;

            // printf("%hu: %X %X %X | %X %X %X %X\n", PC-2, b1, b2, current_instruction, nibble1, nibble2, nibble3, nibble4);

            // decode & execute
            switch (nibble1) {
                case 0x0:
                    if (current_instruction == 0x00E0) {
                        printf("-> Clear screen instruction");
                    }
                    break;
                default:
                    printf("Unknown instruction\n");
                    break;
                }
        } else {
            break;
        }
    }

    // renderToCLI(screen, 64, 32);
    // clearScreen(screen, 64, 32);
    // renderToCLI(screen, 64, 32);

    // for (int i = 0; i < 64*32; ++i) {
    //     printf("%d", screen[i]);
    // }

    return 0;
}
