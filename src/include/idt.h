#ifndef IDT_H
#define IDT_H

#include <stdint.h>

// CRITICAL: Packed to ensure exactly 16 bytes per entry
struct idt_entry {
    uint16_t isr_low;
    uint16_t kernel_cs;
    uint8_t  ist;
    uint8_t  attributes;
    uint16_t isr_mid;
    uint32_t isr_high;
    uint32_t reserved;
} __attribute__((packed));

// CRITICAL: Packed to prevent 6 bytes of padding between uint16_t and uint64_t
struct idt_pointer {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

void idt_init(void);
void idt_set_descriptor(uint8_t vector, void* isr, uint8_t attributes);

#endif