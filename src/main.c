#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "include/limine.h"
#include "include/gdt.h"
#include "include/idt.h"
#include "include/console.h"
#include "include/pmm.h"
#include "include/keyboard.h"
#include "include/vmm.h"
#include "include/heap.h"
#include "include/timer.h"

// 1. START MARKER
__attribute__((used, section(".requests_start_marker")))
static volatile uint64_t limine_requests_start_marker[] = LIMINE_REQUESTS_START_MARKER;

// 2. Base Revision Tag (MUST BE BETWEEN START & END MARKERS)
__attribute__((used, section(".requests")))
static volatile uint64_t limine_base_revision[3] = LIMINE_BASE_REVISION(3);

// 3. Requests
__attribute__((used, section(".requests")))
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".requests")))
volatile struct limine_executable_address_request exec_address_request = {
    .id = LIMINE_EXECUTABLE_ADDRESS_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".requests")))
volatile struct limine_executable_cmdline_request cmdline_request = {
    .id = LIMINE_EXECUTABLE_CMDLINE_REQUEST_ID,
    .revision = 0
};

// HHDM (Higher-Half Direct Map) Request
__attribute__((used, section(".requests")))
static volatile struct limine_hhdm_request hhdm_request = {
    .id = LIMINE_HHDM_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".requests")))
volatile struct limine_memmap_request memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST_ID,
    .revision = 0
};

// 4. END MARKER (Must immediately follow requests)
__attribute__((used, section(".requests_end_marker")))
static volatile uint64_t limine_requests_end_marker[] = LIMINE_REQUESTS_END_MARKER;

static bool str_contains(const char *str, const char *search) {
    if (!str || !search) return false;

    for (size_t i = 0; str[i] != '\0'; i++) {
        size_t j = 0;
        while (str[i + j] != '\0' && search[j] != '\0' && str[i + j] == search[j]) {
            j++;
        }
        if (search[j] == '\0') {
            return true; // Match found
        }
    }
    return false;
}

static inline uint64_t get_current_rip(void) {
    uint64_t rip;
    __asm__ volatile (
        "lea 0(%%rip), %0" 
        : "=r"(rip)
    );
    return rip;
}

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
    klogf("pic0: PIC remapped, IRQs 0-15 -> vectors 32-47\n");
}

void _start(void) {
    // Disable hardware interrupts during early core system initialization
    __asm__ volatile("cli");
    // --- Framebuffer & Console Setup ---
    if (framebuffer_request.response == NULL) {
        // If response is NULL, Limine didn't see or handle the request
        // Trigger a QEMU debug break or infinite halt
        __asm__ volatile ("cli; hlt");
    }

    if (framebuffer_request.response->framebuffer_count == 0) {
        __asm__ volatile ("cli; hlt");
    }
    struct limine_framebuffer *fb = framebuffer_request.response->framebuffers[0];
    console_init(fb);

    gdt_init();
    idt_init();
    pic_init();

    // --- Hardware PIT Timer Setup ---
    timer_init(1000); // Set 1000 Hz frequency (1 tick = 1ms)
    // Enable CPU Interrupts
    __asm__ volatile("sti");
    klogf("core: sti called. interrupts enabled. RIP=0x%x\n", get_current_rip());

    // --- PMM Logging ---
    pmm_init();

    // --- VMM Logging ---
    vmm_init();

    // --- Heap Logging ---
    heap_init();;

    // --- Keyboard Driver Logging ---
    keyboard_init();
    
    keyboard_enable();
    
    bool start_cli = false;

    __asm__ volatile("" : : : "memory");

    
    klogf("core: command line: %s\n", 
          (cmdline_request.response && cmdline_request.response->cmdline) ? 
          cmdline_request.response->cmdline : "(none)");

    // Parse Limine command line arguments
    if (cmdline_request.response != NULL && cmdline_request.response->cmdline != NULL) {
        const char *cmdline = cmdline_request.response->cmdline;

        if (str_contains(cmdline, "cli")) {
            start_cli = true;
        }
    }

    if (start_cli) {
        klogf("core: 'cli' argument detected. starting shell\n");
        // shell_run();
    } else {
        klogf("core: no 'cli' argument provided. continue\n");
    }

    while(1) {
        __asm__ volatile("hlt");
    }
}