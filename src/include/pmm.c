#include "pmm.h"
#include "limine.h"
#include <stddef.h>

static volatile struct limine_memmap_request memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST_ID,
    .revision = 0
};

static uint8_t* bitmap = NULL;
static size_t highest_page_idx = 0;
static size_t free_memory = 0;

static inline void bitmap_set(size_t bit) {
    bitmap[bit / 8] |= (1 << (bit % 8));
}

static inline void bitmap_clear(size_t bit) {
    bitmap[bit / 8] &= ~(1 << (bit % 8));
}

static inline int bitmap_test(size_t bit) {
    return (bitmap[bit / 8] & (1 << (bit % 8))) != 0;
}

void pmm_init(void) {
    struct limine_memmap_response *memmap = memmap_request.response;
    if (memmap == NULL) {
        while(1); // Error handling
    }

    // 1. Find the highest physical address to size our bitmap
    uint64_t highest_addr = 0;
    for (size_t i = 0; i < memmap->entry_count; i++) {
        struct limine_memmap_entry *entry = memmap->entries[i];
        uint64_t top = entry->base + entry->length;
        if (top > highest_addr) {
            highest_addr = top;
        }
    }

    highest_page_idx = highest_addr / PAGE_SIZE;
    size_t bitmap_size = highest_page_idx / 8;

    // 2. Find a usable memory region big enough to store the bitmap itself
    for (size_t i = 0; i < memmap->entry_count; i++) {
        struct limine_memmap_entry *entry = memmap->entries[i];
        if (entry->type == LIMINE_MEMMAP_USABLE && entry->length >= bitmap_size) {
            bitmap = (uint8_t*)(entry->base + 0xFFFF800000000000); // Higher-half direct map offset
            
            // Mark entire bitmap memory as used by default (fill with 0xFF)
            for (size_t b = 0; b < bitmap_size; b++) {
                bitmap[b] = 0xFF;
            }
            break;
        }
    }

    // 3. Populate usable pages in the bitmap based on Limine's memmap
    for (size_t i = 0; i < memmap->entry_count; i++) {
        struct limine_memmap_entry *entry = memmap->entries[i];
        if (entry->type == LIMINE_MEMMAP_USABLE) {
            size_t start_page = entry->base / PAGE_SIZE;
            size_t page_count = entry->length / PAGE_SIZE;

            for (size_t p = 0; p < page_count; p++) {
                bitmap_clear(start_page + p);
                free_memory += PAGE_SIZE;
            }
        }
    }

    // 4. Mark the bitmap's own physical pages as used so we don't overwrite it!
    size_t bitmap_start_page = ((uint64_t)bitmap - 0xFFFF800000000000) / PAGE_SIZE;
    size_t bitmap_page_count = (bitmap_size + PAGE_SIZE - 1) / PAGE_SIZE;
    for (size_t p = 0; p < bitmap_page_count; p++) {
        bitmap_set(bitmap_start_page + p);
    }
}

void* pmm_alloc_page(void) {
    for (size_t i = 0; i < highest_page_idx; i++) {
        if (!bitmap_test(i)) {
            bitmap_set(i);
            free_memory -= PAGE_SIZE;
            return (void*)(i * PAGE_SIZE);
        }
    }
    return NULL; // Out of memory!
}

void pmm_free_page(void* ptr) {
    size_t page_idx = (uint64_t)ptr / PAGE_SIZE;
    if (bitmap_test(page_idx)) {
        bitmap_clear(page_idx);
        free_memory += PAGE_SIZE;
    }
}