#include <stdint.h>
#include <stddef.h>
#include "include/limine.h"
#include "include/flanterm.h"
#include "include/flanterm_backends/fb.h"
#include "include/string.h"
#include "include/gdt.h"
// Tell Limine we need a terminal to print to the screen
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID,
    .revision = 0
};

// This is your new entry point (replacing your old assembly call)
void _start(void) {
    gdt_init();
    // Ensure the bootloader actually gave us a terminal
    if (framebuffer_request.response == NULL || framebuffer_request.response->framebuffer_count < 0) {
        while(1); 
    }
    struct limine_framebuffer *fb = framebuffer_request.response->framebuffers[0];

    // Initialize Flanterm by passing it all the screen details
    struct flanterm_context *ft_ctx = flanterm_fb_init(
    NULL, NULL,                           // 1, 2: Allocation functions
    fb->address, fb->width, fb->height, fb->pitch, // 3, 4, 5, 6: Framebuffer dimensions
    fb->red_mask_size, fb->red_mask_shift, // 7, 8: Red mask
    fb->green_mask_size, fb->green_mask_shift, // 9, 10: Green mask
    fb->blue_mask_size, fb->blue_mask_shift, // 11, 12: Blue mask
    NULL, NULL, NULL, NULL, NULL, NULL,   // 13, 14, 15, 16, 17, 18: Font & canvas overrides
    NULL, NULL,                           // 19, 20: Color palette configuration
    0, 0,                                 // 21, 22: Margins
    1,                                    // 23: Text scale
    0, 0, 0, 0                            // 24, 25, 26, 27: Text spacing, tabs, and flags
    );
    const char *msg = "Hello World from modern Limine and Flanterm!\n";
    flanterm_write(ft_ctx, msg, 46); // length of string reminder pls make a wrapper later so this is jsut a kernel syscall da4qk 7/18/26
    
    // Halt the CPU
    while(1) {
        __asm__("hlt");
    }
}