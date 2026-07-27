#include "heap.h"
#include "vmm.h"
#include "pmm.h"
#include "console.h"
#include "string.h"

static heap_block_t *heap_head = NULL;

void heap_init(void) {
    // Allocate initial physical pages for the heap and map them via VMM
    for (size_t i = 0; i < HEAP_INITIAL_PAGES; i++) {
        void *phys = pmm_alloc_page();
        uint64_t virt = HEAP_START_VIRT + (i * PAGE_SIZE);
        vmm_map_page(kernel_pml4, virt, (uint64_t)phys, PAGE_WRITABLE);
    }

    // Setup initial master free block spanning the whole allocated space
    heap_head = (heap_block_t*)HEAP_START_VIRT;
    heap_head->size = (HEAP_INITIAL_PAGES * PAGE_SIZE) - sizeof(heap_block_t);
    heap_head->is_free = 1;
    heap_head->next = NULL;

    klogf("heap Initialized: %d KB heap available at %x\n", 
          (uint32_t)((HEAP_INITIAL_PAGES * PAGE_SIZE) / 1024), HEAP_START_VIRT);
    klogf("heap0: slab/block allocator ready at 0xFFFF900000000000 (%d KB initial)\n", 
          (uint32_t)((HEAP_INITIAL_PAGES * 4096) / 1024));
}

void* kmalloc(size_t size) {
    if (size == 0) return NULL;

    // Align request size to 8-byte boundary for CPU architecture efficiency
    size = (size + 7) & ~7ULL;

    heap_block_t *curr = heap_head;

    // Search free list for a block that fits requested size
    while (curr != NULL) {
        if (curr->is_free && curr->size >= size) {
            
            // Split block if remaining space is big enough for a new header + payload
            if (curr->size >= size + sizeof(heap_block_t) + 8) {
                heap_block_t *new_block = (heap_block_t*)((uint8_t*)curr + sizeof(heap_block_t) + size);
                new_block->size = curr->size - size - sizeof(heap_block_t);
                new_block->is_free = 1;
                new_block->next = curr->next;

                curr->size = size;
                curr->next = new_block;
            }

            curr->is_free = 0;
            // Return pointer directly to payload (skipping block header)
            return (void*)((uint8_t*)curr + sizeof(heap_block_t));
        }
        curr = curr->next;
    }

    klogf("KMALLOC ERROR: Out of heap memory! (Requested %d bytes)\n", (uint32_t)size);
    return NULL;
}

void kfree(void *ptr) {
    if (ptr == NULL) return;

    // Retrieve original block header located right before payload pointer
    heap_block_t *block = (heap_block_t*)((uint8_t*)ptr - sizeof(heap_block_t));
    block->is_free = 1;

    // Coalesce (merge) adjacent free blocks to eliminate fragmentation
    heap_block_t *curr = heap_head;
    while (curr != NULL && curr->next != NULL) {
        if (curr->is_free && curr->next->is_free) {
            curr->size += sizeof(heap_block_t) + curr->next->size;
            curr->next = curr->next->next;
        } else {
            curr = curr->next;
        }
    }
}