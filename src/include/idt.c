#include "idt.h"
#include <stdint.h>
#include "console.h"
#include "keyboard.h" // Added header for keyboard_handler()

// Define the 256 entry table and its pointer
struct idt_entry my_idt[256] __attribute__((aligned(16)));
struct idt_pointer idt_ptr;

volatile uint64_t system_ticks = 0;

const char* exception_messages[] = {
    "Division By Zero", "Debug", "Non-Maskable Interrupt", "Breakpoint",
    "Into Detected Overflow", "Out of Bounds", "Invalid Opcode", "No Coprocessor",
    "Double Fault", "Coprocessor Segment Overrun", "Bad TSS", "Segment Not Present",
    "Stack Fault", "General Protection Fault", "Page Fault", "Unknown Interrupt",
    "x87 FPU Floating-Point Error", "Alignment Check", "Machine Check", "SIMD Floating-Point Exception",
    "Virtualization Exception", "Control Protection Exception", "Reserved", "Reserved",
    "Reserved", "Reserved", "Reserved", "Reserved",
    "Hypervisor Injection Exception", "VMM Communication Exception", "Security Exception", "Reserved"
};

// External assembly ISR stubs
extern void isr0(void);  extern void isr1(void);  extern void isr2(void);  extern void isr3(void);
extern void isr4(void);  extern void isr5(void);  extern void isr6(void);  extern void isr7(void);
extern void isr8(void);  extern void isr9(void);  extern void isr10(void); extern void isr11(void);
extern void isr12(void); extern void isr13(void); extern void isr14(void); extern void isr15(void);
extern void isr16(void); extern void isr17(void); extern void isr18(void); extern void isr19(void);
extern void isr20(void); extern void isr21(void); extern void isr22(void); extern void isr23(void);
extern void isr24(void); extern void isr25(void); extern void isr26(void); extern void isr27(void);
extern void isr28(void); extern void isr29(void); extern void isr30(void); extern void isr31(void);

// Hardware IRQ stubs
extern void irq0_timer(void);
extern void irq1_keyboard(void); // Vector 33 / IRQ 1 assembly stub

void idt_handle_exception(uint64_t vector, uint64_t error_code, uint64_t rip) {
    // 1. Handle Hardware Timer (IRQ 0 remapped to Vector 32 / 0x20)
    if (vector == 0x20) {
        system_ticks++;
        __asm__ volatile("outb %%al, %%dx" :: "a"(0x20), "d"(0x20));
        return; 
    }

    // 2. Handle Hardware Keyboard (IRQ 1 remapped to Vector 33 / 0x21)
    if (vector == 0x21) {
        keyboard_handler();
        __asm__ volatile("outb %%al, %%dx" :: "a"(0x20), "d"(0x20)); // EOI to PIC
        return;
    }

    // 3. Handle standard CPU Exceptions (Vectors 0 - 31)
    if (vector < 32) {
        kprintf("\n==================================================\n");
        kprintf("!!! KERNEL PANIC: %s (Exception %d) !!!\n", exception_messages[vector], vector);
        kprintf("Error Code: 0x%x\n", error_code);
        kprintf("Crashed at Instruction Pointer (RIP): 0x%x\n", rip);
        kprintf("System Uptime: %d ticks\n", system_ticks);
        kprintf("==================================================\n");
        kprintf("System Halted Safely.");
        
        while(1) {
            __asm__ volatile("hlt");
        }
    }
    
    kprintf("Unhandled vector fired: %d\n", vector);
}

void idt_set_descriptor(uint8_t vector, void* isr, uint8_t attributes) {
    uint64_t addr = (uint64_t)isr;

    my_idt[vector].isr_low    = (uint16_t)(addr & 0xFFFF);
    my_idt[vector].kernel_cs  = 0x08; 
    my_idt[vector].ist        = 0;
    my_idt[vector].attributes = attributes;
    my_idt[vector].isr_mid    = (uint16_t)((addr >> 16) & 0xFFFF);
    my_idt[vector].isr_high   = (uint32_t)((addr >> 32) & 0xFFFFFFFF);
    my_idt[vector].reserved   = 0;
}

void idt_init(void) {
    idt_ptr.limit = (sizeof(struct idt_entry) * 256) - 1;
    idt_ptr.base  = (uint64_t)&my_idt;

    for (int i = 0; i < 256; i++) {
        my_idt[i] = (struct idt_entry){0};
    }

    // Register all 32 Core Exceptions (0x8E = Present, Ring 0, 64-bit Interrupt Gate)
    idt_set_descriptor(0,  isr0,  0x8E); idt_set_descriptor(1,  isr1,  0x8E);
    idt_set_descriptor(2,  isr2,  0x8E); idt_set_descriptor(3,  isr3,  0x8E); 
    idt_set_descriptor(4,  isr4,  0x8E); idt_set_descriptor(5,  isr5,  0x8E);
    idt_set_descriptor(6,  isr6,  0x8E); idt_set_descriptor(7,  isr7,  0x8E);
    idt_set_descriptor(8,  isr8,  0x8E); idt_set_descriptor(9,  isr9,  0x8E);
    idt_set_descriptor(10, isr10, 0x8E); idt_set_descriptor(11, isr11, 0x8E);
    idt_set_descriptor(12, isr12, 0x8E); idt_set_descriptor(13, isr13, 0x8E); 
    idt_set_descriptor(14, isr14, 0x8E); idt_set_descriptor(15, isr15, 0x8E);
    idt_set_descriptor(16, isr16, 0x8E); idt_set_descriptor(17, isr17, 0x8E);
    idt_set_descriptor(18, isr18, 0x8E); idt_set_descriptor(19, isr19, 0x8E);
    idt_set_descriptor(20, isr20, 0x8E); idt_set_descriptor(21, isr21, 0x8E);
    idt_set_descriptor(22, isr22, 0x8E); idt_set_descriptor(23, isr23, 0x8E);
    idt_set_descriptor(24, isr24, 0x8E); idt_set_descriptor(25, isr25, 0x8E);
    idt_set_descriptor(26, isr26, 0x8E); idt_set_descriptor(27, isr27, 0x8E);
    idt_set_descriptor(28, isr28, 0x8E); idt_set_descriptor(29, isr29, 0x8E);
    idt_set_descriptor(30, isr30, 0x8E); idt_set_descriptor(31, isr31, 0x8E);

    // Register hardware IRQs
    idt_set_descriptor(32, irq0_timer,    0x8E); 
    idt_set_descriptor(33, irq1_keyboard, 0x8E); // Registered Vector 33 (0x21)

    __asm__ volatile("lidt %0" : : "m"(idt_ptr));
}