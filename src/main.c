#include <stdint.h>
#include <stddef.h>
#include "include/limine.h"
#include "include/gdt.h"
#include "include/idt.h"
#include "include/console.h"
#include "include/pmm.h"
#include "include/keyboard.h"

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
    outb(0x21, 0xFC); 
    outb(0xA1, 0xFF); 
}

void _start(void) {
    // CRITICAL: Ensure hardware interrupts are off while building descriptor tables
    __asm__ volatile("cli");

    gdt_init();
    idt_init();
    pic_init();
    __asm__ volatile("sti");
    if (framebuffer_request.response == NULL || framebuffer_request.response->framebuffer_count == 0) {
        while(1); 
    }
    struct limine_framebuffer *fb = framebuffer_request.response->framebuffers[0];
  
    console_init(fb);
    klogf("Init console context\n");
    pmm_init();
    klogf("PMM Init\n");  
    void* page1 = pmm_alloc_page();
    void* page2 = pmm_alloc_page();

    klogf("Allocated Page 1 Physical Address: 0x%x\n", (uint64_t)page1);
    klogf("Allocated Page 2 Physical Address: 0x%x\n", (uint64_t)page2);

    pmm_free_page(page1);
    klogf("Freed Page 1\n");
    pmm_free_page(page2);
    klogf("Freed Page 2\n");

    keyboard_init();
    klogf("PS/2 Keyboard init\n");
    keyboard_enable();
    klogf("Keyboard enabled\n");
    // testing sti later
    while(1) {
        __asm__ volatile("hlt");
    }
}