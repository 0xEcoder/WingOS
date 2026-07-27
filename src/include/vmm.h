#ifndef VMM_H
#define VMM_H

#include <stdint.h>
#include <stddef.h>

#define PAGE_PRESENT  (1ULL << 0)
#define PAGE_WRITABLE (1ULL << 1)
#define PAGE_USER     (1ULL << 2)

#define HHDM_OFFSET   0xFFFF800000000000ULL

typedef uint64_t pt_entry_t;

typedef struct page_table {
    pt_entry_t entries[512];
} page_table_t;

void vmm_init(void);
void vmm_map_page(page_table_t *pml4, uint64_t virt, uint64_t phys, uint64_t flags);
void vmm_switch_pml4(page_table_t *pml4);

extern page_table_t *kernel_pml4;

#endif