#include "idt.h"
#include <stdint.h>
#include "console.h"
#include "keyboard.h"
#include "timer.h"

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
extern void irq1_keyboard(void);

// Helper function to acknowledge PIC interrupt
static inline void pic_send_eoi(uint8_t irq) {
    if (irq >= 8) {
        __asm__ volatile("outb %0, %1" :: "a"((uint8_t)0x20), "Nd"((uint16_t)0xA0));
    }
    __asm__ volatile("outb %0, %1" :: "a"((uint8_t)0x20), "Nd"((uint16_t)0x20));
}

void idt_handle_exception(uint64_t vector, uint64_t error_code, uint64_t rip) {
    // 1. Handle Hardware Timer (IRQ 0 -> Vector 32 / 0x20)
    if (vector == 32) {
        timer_handler();
        pic_send_eoi(0);
        return; 
    }

    // 2. Handle Hardware Keyboard (IRQ 1 -> Vector 33 / 0x21)
    if (vector == 33) {
        keyboard_handler();
        pic_send_eoi(1);
        return;
    }

    // 3. Handle standard CPU Exceptions (Vectors 0 - 31)
    if (vector < 32) {
        kprintf("\n==================================================\n");
        kprintf("!!! KERNEL PANIC: %s (Exception %d) !!!\n", exception_messages[vector], vector);
        kprintf("Error Code: %x\n", error_code);
        kprintf("Crashed at Instruction Pointer (RIP): %x\n", rip);
        kprintf("System Uptime: %d ticks\n", system_ticks);
        kprintf("==================================================\n");
        kprintf("System Halted Safely.");
        
        while(1) {
            __asm__ volatile("cli; hlt");
        }
    }
    
    kprintf("Unhandled vector fired: %d\n", vector);
    pic_send_eoi(vector - 32);
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

    // Register 32 Exceptions (0x8E = Present, Ring 0, Interrupt Gate)
    void* isrs[32] = {
        isr0,  isr1,  isr2,  isr3,  isr4,  isr5,  isr6,  isr7,
        isr8,  isr9,  isr10, isr11, isr12, isr13, isr14, isr15,
        isr16, isr17, isr18, isr19, isr20, isr21, isr22, isr23,
        isr24, isr25, isr26, isr27, isr28, isr29, isr30, isr31
    };

    for (int i = 0; i < 32; i++) {
        idt_set_descriptor(i, isrs[i], 0x8E);
    }

    // Register hardware IRQs
    idt_set_descriptor(32, irq0_timer,    0x8E); 
    idt_set_descriptor(33, irq1_keyboard, 0x8E);

    __asm__ volatile("lidt %0" : : "m"(idt_ptr));
    klogf("idt0: loaded IDT gates at %x\n", idt_ptr.base);
}