#ifndef HEAP_H
#define HEAP_H

#include <stdint.h>
#include <stddef.h>

#define HEAP_START_VIRT    0xFFFF900000000000ULL
#define HEAP_INITIAL_PAGES 32 // Starts with 128 KiB heap space

typedef struct heap_block {
    size_t size;                 // Usable payload size
    int is_free;                 // 1 = Free, 0 = In Use
    struct heap_block *next;     // Pointer to next block
} heap_block_t;

void heap_init(void);
void* kmalloc(size_t size);
void kfree(void *ptr);

#endif