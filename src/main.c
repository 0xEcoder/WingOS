#include <stdint.h>
#include <stddef.h>
#include "include/limine.h"
#include "include/gdt.h"
#include "include/idt.h"
#include "include/console.h"

static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID,
    .revision = 0
};

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ( "outb %0, %1" : : "a"(val), "Nd"(port) );
}

void pic_init(void) {
    outb(0x20, 0x11);
    outb(0xA0, 0x11);
    outb(0x21, 0x20);
    outb(0xA1, 0x28);
    outb(0x21, 0x04);
    outb(0xA1, 0x02);
    outb(0x21, 0x01);
    outb(0xA1, 0x01);
    outb(0x21, 0xFE); 
    outb(0xA1, 0xFF); 
}

void _start(void) {
    // CRITICAL: Ensure hardware interrupts are off while building descriptor tables
    __asm__ volatile("cli");

    gdt_init();
    idt_init();
    pic_init(); 

    if (framebuffer_request.response == NULL || framebuffer_request.response->framebuffer_count == 0) {
        while(1); 
    }
    struct limine_framebuffer *fb = framebuffer_request.response->framebuffers[0];

    console_init(fb);
    kprintf("Init console context \033[32mSUCCESS\n");
      
    // Test 1: Software exception
    __asm__ volatile("int $3");
    kprintf("If you see this, your INT 3 handler returned successfully!\n");

    // Test 2: Enable Hardware Interrupts
    kprintf("Enabling hardware interrupts...\n");
    __asm__ volatile("sti");

    while(1) {
        __asm__ volatile("hlt");
    }
}