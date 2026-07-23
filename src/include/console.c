#include "console.h"
#include "flanterm.h"
#include "flanterm_backends/fb.h"
#include <stdarg.h>  
#include <stddef.h>
#include <stdint.h>


static struct flanterm_context *global_ctx = NULL;

void console_init(struct limine_framebuffer *fb) {
    global_ctx = flanterm_fb_init(
        NULL, NULL,
        fb->address, fb->width, fb->height, fb->pitch,
        fb->red_mask_size, fb->red_mask_shift,
        fb->green_mask_size, fb->green_mask_shift,
        fb->blue_mask_size, fb->blue_mask_shift,
        NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 
        0, 0, 1, 1, 1, 0, 0
    );
}

static void kprints(const char* text) {
    if (global_ctx == NULL) return;

    for (size_t i = 0; text[i] != '\0'; i++) {
        if (text[i] == '\n') {
            // Newline + Carriage Return
            flanterm_write(global_ctx, "\r\n", 2);
        } else if (text[i] == '\b') {
            // Visual erase sequence for terminal: move back, space over, move back
            flanterm_write(global_ctx, "\b \b", 3);
        } else {
            flanterm_write(global_ctx, &text[i], 1);
        }
    }
}
// Helper to convert numbers to strings inside console.c
static void kprint_num(uint64_t value, int base) {
    char chars[] = "0123456789ABCDEF";
    char buffer[32];
    int i = 30;
    buffer[31] = '\0';

    if (value == 0) {
        kprints("0");
        return;
    }

    while (value > 0) {
        buffer[i] = chars[value % base];
        value /= base;
        i--;
    }
    kprints(&buffer[i + 1]);
}

// Core formatting logic that accepts a va_list
void vkprintf(const char* format, va_list args) {
    for (size_t i = 0; format[i] != '\0'; i++) {
        if (format[i] != '%') {
            char c[2] = { format[i], '\0' };
            kprints(c);
            continue;
        }

        i++; 
        switch (format[i]) {
            case 's': {
                char* s = va_arg(args, char*);
                if (s == NULL) s = "(null)";
                kprints(s);
                break;
            }
            case 'd': {
                int d = va_arg(args, int);
                if (d < 0) {
                    kprints("-");
                    d = -d;
                }
                kprint_num(d, 10);
                break;
            }
            case 'x': {
                uint64_t x = va_arg(args, uint64_t);
                kprints("0x");
                kprint_num(x, 16);
                break;
            }
            case '%': {
                kprints("%");
                break;
            }
            default:
                kprints("?");
                break;
        }
    }
}

// Standard kprintf wrapper
void kprintf(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vkprintf(format, args);
    va_end(args);
}

#define TIMER_FREQ 100 // Adjust to match PIT frequency

// Timestamped logging wrapper
void klogf(const char *fmt, ...) {
    uint64_t sec = system_ticks / TIMER_FREQ;
    uint64_t ms  = (system_ticks % TIMER_FREQ) * (1000 / TIMER_FREQ);

    // Manual zero-padding for milliseconds (< 10 -> "00", < 100 -> "0")
    const char* ms_pad = (ms < 10) ? "00" : (ms < 100) ? "0" : "";

    // Print header using basic %d formatters that your kprintf already supports
    kprintf("[%d.%s%d] ", (uint32_t)sec, ms_pad, (uint32_t)ms);

    // Pass the rest of the log message to vkprintf
    va_list args;
    va_start(args, fmt);
    vkprintf(fmt, args); 
    va_end(args);
}