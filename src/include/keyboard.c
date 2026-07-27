#include "keyboard.h"
#include "console.h"
#include <stdint.h>

#define INPUT_BUFFER_SIZE 256

static char input_buffer[INPUT_BUFFER_SIZE];
static size_t buffer_idx = 0;
static int shift_pressed = 0;

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

// US QWERTY Scancode Set 1 Translation Table
static const char scancode_to_ascii[] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
  '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
     0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
     0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/',   0,
   '*',   0, ' '
};

void keyboard_enable(void) {
    uint8_t mask = inb(0x21);
    outb(0x21, mask & ~(1 << 1)); // Clear bit 1 on Master PIC (Unmask IRQ 1)
    outb(0x64, 0xAE);
    klogf("ps2: keyboard controller listening for scancodes\n");             // Command PS/2 controller: Enable Port 1
}

void keyboard_disable(void) {
    uint8_t mask = inb(0x21);
    outb(0x21, mask | (1 << 1));  // Set bit 1 on Master PIC (Mask IRQ 1)
    outb(0x64, 0xAD);             // Command PS/2 controller: Disable Port 1
    klogf("ps2: keyboard controller stopped listening for scancodes\n");
}

void keyboard_handler(void) {
    uint8_t scancode = inb(0x60);

    // 1. Key Release Events (scancode >= 0x80)
    if (scancode & 0x80) {
        if (scancode == 0xAA || scancode == 0xB6) { // Left / Right Shift Released
            shift_pressed = 0;
        }
        outb(0x20, 0x20); // EOI to PIC
        return;
    }

    // 2. Key Press Events (scancode < 0x80)
    if (scancode == 0x2A || scancode == 0x36) { // Left / Right Shift Pressed
        shift_pressed = 1;
    } 
    else if (scancode < sizeof(scancode_to_ascii)) {
        char c = scancode_to_ascii[scancode];

        if (c == '\b') {
            // BACKSPACE: Only erase if the buffer is not empty
            if (buffer_idx > 0) {
                buffer_idx--;
                input_buffer[buffer_idx] = '\0';
                
                // Send backspace character to Flanterm (erases letter on screen)
                kprintf("\b");
            }
        } 
        else if (c == '\n') {
            // ENTER: Advance line & process buffer
            kprintf("\n");
            input_buffer[buffer_idx] = '\0';

            // Reset buffer for the next command prompt line
            buffer_idx = 0;
        } 
        else if (c != 0) {
            // REGULAR PRINTABLE CHARACTER
            if (shift_pressed && c >= 'a' && c <= 'z') {
                c -= 32; // Uppercase
            }

            if (buffer_idx < INPUT_BUFFER_SIZE - 1) {
                input_buffer[buffer_idx++] = c;

                char str[2] = { c, '\0' };
                kprintf(str);
            }
        }
    }

    // Send End of Interrupt (EOI) signal to Master PIC
    outb(0x20, 0x20);
}

void keyboard_init(void) {
    // Flush PS/2 controller output buffer
    while (inb(0x64) & 1) {
        inb(0x60);
    }
    klogf("ps2: keyboard driver initialized (IRQ 1 mapped)\n");
}