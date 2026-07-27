#include "gdt.h"
#include "console.h"

// CRITICAL: Packed structure prevents compiler padding
struct gdt_pointer {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

// Global static array managed cleanly by the compiler
static uint64_t my_gdt[3] __attribute__((aligned(16))) = {
    0x0000000000000000, // 0x00: Null Descriptor
    0x00209A0000000000, // 0x08: Kernel Code Segment (64-bit, Ring 0)
    0x0000920000000000  // 0x10: Kernel Data Segment (64-bit, Ring 0)
};

void gdt_init(void) {
    struct gdt_pointer gdt_ptr;
    gdt_ptr.limit = sizeof(my_gdt) - 1;
    gdt_ptr.base  = (uint64_t)&my_gdt;

    __asm__ volatile (
        "lgdt %0\n\t"
        "mov $0x10, %%ax\n\t"
        "mov %%ax, %%ds\n\t"
        "mov %%ax, %%es\n\t"
        "mov %%ax, %%fs\n\t"
        "mov %%ax, %%gs\n\t"
        "mov %%ax, %%ss\n\t"
        "pushq $0x08\n\t"
        "leaq 1f(%%rip), %%rax\n\t"
        "pushq %%rax\n\t"
        "retfq\n\t"
        "1:\n\t"
        :
        : "m"(gdt_ptr)
        : "rax", "memory"
    );
    klogf("gdt0: loaded 64-bit kernel descriptors\n");
}