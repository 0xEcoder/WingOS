#ifndef GDT_H
#define GDT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif 

__attribute__((packed)) struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_middle;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
};

__attribute__((packed)) struct gdt_pointer {
    uint16_t size;
    uint64_t offset;
};

void gdt_init(void);

// "extern" tells the compiler these exist elsewhere (in gdt.c)
extern struct gdt_entry my_gdt[3];
extern struct gdt_pointer gdt_ptr;

#ifdef __cplusplus
}
#endif

#endif // GDT_H