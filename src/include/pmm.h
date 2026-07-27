#ifndef PMM_H
#define PMM_H

#include <stdint.h>
#include <stddef.h>

#define PAGE_SIZE 4096

void pmm_init(void);
void* pmm_alloc_page(void);
void pmm_free_page(void* ptr);

size_t pmm_get_free_memory(void);
size_t pmm_get_total_memory(void);

#endif