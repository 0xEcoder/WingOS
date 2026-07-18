#include <stdint.h>

// Attribute macros to prevent the compiler from optimizing out layout alignment
__attribute__((packed)) struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_middle;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
};

__attribute__((packed)) struct gdt_pointer {
    uint16_t size;    // Size of the GDT table minus 1
    uint64_t offset;  // The linear virtual address of our GDT array
};

void gdt_init(void) {
    // 1. The Null Descriptor (All zeros)
    my_gdt[0] = (struct gdt_entry){0, 0, 0, 0, 0, 0};

    // 2. Kernel Code Segment
    // Access byte: 0x9A -> Present, Ring 0, Executable, Read/Write allowed
    // Granularity: 0x20 -> Long mode flag (64-bit) is set
    my_gdt[1].limit_low   = 0;
    my_gdt[1].base_low    = 0;
    my_gdt[1].base_middle = 0;
    my_gdt[1].access      = 0x9A; 
    my_gdt[1].granularity = 0x20; 
    my_gdt[1].base_high   = 0;

    // 3. Kernel Data Segment
    // Access byte: 0x92 -> Present, Ring 0, Non-executable, Read/Write allowed
    // Granularity: 0x00 -> Normal 64-bit data rules
    my_gdt[2].limit_low   = 0;
    my_gdt[2].base_low    = 0;
    my_gdt[2].base_middle = 0;
    my_gdt[2].access      = 0x92; 
    my_gdt[2].granularity = 0x00; 
    my_gdt[2].base_high   = 0;

    // Set up the pointer structure
    gdt_ptr.size = (sizeof(struct gdt_entry) * 3) - 1;
    gdt_ptr.offset = (uint64_t)&my_gdt;

    // Tell the CPU to load our table!
    // Load GDT and flush segment registers
    __asm__ volatile (
        "lgdt %0\n\t"               // Load the new GDT pointer
        "mov $0x10, %%ax\n\t"       // 0x10 is our data segment index (Entry 2 * 8 bytes)
        "mov %%ax, %%ds\n\t"        // Clear Data Segment
        "mov %%ax, %%es\n\t"        // Clear Extra Segment
        "mov %%ax, %%fs\n\t"        // Clear FS
        "mov %%ax, %%gs\n\t"        // Clear GS
        "mov %%ax, %%ss\n\t"        // Clear Stack Segment
        
        // Reload CS (Code Segment) using a 64-bit push/retfq sequence
        "pushq $0x08\n\t"           // Push our code segment index (Entry 1 * 8 bytes)
        "leaq 1f(%%rip), %%rax\n\t" // Get address of label '1' below
        "pushq %%rax\n\t"           // Push it onto the stack
        "retfq\n\t"                 // Perform Far Return to reload CS and jump to '1'
        "1:\n\t"
        :
        : "m"(gdt_ptr)
        : "rax"
}

struct gdt_entry my_gdt[3];
struct gdt_pointer gdt_ptr;