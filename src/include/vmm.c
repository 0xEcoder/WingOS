#include "vmm.h"
#include "pmm.h"
#include "console.h"
#include "string.h"
#include "limine.h"

extern struct limine_executable_address_request exec_address_request;
extern struct limine_memmap_request memmap_request;

page_table_t *kernel_pml4 = NULL;

#define PML4_INDEX(virt) (((virt) >> 39) & 0x1FF)
#define PDPT_INDEX(virt) (((virt) >> 30) & 0x1FF)
#define PD_INDEX(virt)   (((virt) >> 21) & 0x1FF)
#define PT_INDEX(virt)   (((virt) >> 12) & 0x1FF)

static page_table_t* vmm_get_next_table(page_table_t *current_table, size_t index, uint64_t flags) {
    pt_entry_t entry = current_table->entries[index];

    if (entry & PAGE_PRESENT) {
        uint64_t phys = entry & ~0xFFFULL;
        return (page_table_t*)(phys + HHDM_OFFSET);
    }

    void *phys_page = pmm_alloc_page();
    if (phys_page == NULL) return NULL;

    page_table_t *virt_table = (page_table_t*)((uint64_t)phys_page + HHDM_OFFSET);
    memset(virt_table, 0, sizeof(page_table_t));

    current_table->entries[index] = (uint64_t)phys_page | PAGE_PRESENT | PAGE_WRITABLE | (flags & PAGE_USER);

    return virt_table;
}

void vmm_map_page(page_table_t *pml4, uint64_t virt, uint64_t phys, uint64_t flags) {
    page_table_t *pdpt = vmm_get_next_table(pml4, PML4_INDEX(virt), flags);
    page_table_t *pd   = vmm_get_next_table(pdpt, PDPT_INDEX(virt), flags);
    page_table_t *pt   = vmm_get_next_table(pd, PD_INDEX(virt), flags);

    pt->entries[PT_INDEX(virt)] = (phys & ~0xFFFULL) | flags | PAGE_PRESENT;
}

void vmm_switch_pml4(page_table_t *pml4) {
    uint64_t phys = (uint64_t)pml4 - HHDM_OFFSET;
    __asm__ volatile ("mov %0, %%cr3" : : "r"(phys) : "memory");
}

void vmm_init(void) {
    if (exec_address_request.response == NULL || memmap_request.response == NULL) {
        klogf("VMM ERROR: Missing Limine requests!\n");
        while(1);
    }

    struct limine_executable_address_response *kaddr = exec_address_request.response;
    struct limine_memmap_response *memmap = memmap_request.response;

    // Allocate Kernel PML4
    void *phys_pml4 = pmm_alloc_page();
    kernel_pml4 = (page_table_t*)((uint64_t)phys_pml4 + HHDM_OFFSET);
    memset(kernel_pml4, 0, sizeof(page_table_t));

    // Map Kernel Executable Code & Data
    uint64_t kernel_phys = kaddr->physical_base;
    uint64_t kernel_virt = kaddr->virtual_base;

    // Map 16 MB for kernel image (.text, .data, .bss, etc.)
    for (uint64_t i = 0; i < 0x1000000; i += 0x1000) {
        vmm_map_page(kernel_pml4, kernel_virt + i, kernel_phys + i, PAGE_WRITABLE);
    }

    // Map Usable RAM Regions into HHDM & Identity Map Low Memory
    for (size_t entry_idx = 0; entry_idx < memmap->entry_count; entry_idx++) {
        struct limine_memmap_entry *entry = memmap->entries[entry_idx];

        for (uint64_t offset = 0; offset < entry->length; offset += 0x1000) {
            uint64_t phys = entry->base + offset;

            // Direct mapping into higher half (HHDM)
            vmm_map_page(kernel_pml4, phys + HHDM_OFFSET, phys, PAGE_WRITABLE);

            // Identity mapping for physical memory below 4GB
            if (phys < 0x100000000ULL) {
                vmm_map_page(kernel_pml4, phys, phys, PAGE_WRITABLE);
            }
        }
    }

    // Switch CR3 to new PML4 page table
    vmm_switch_pml4(kernel_pml4);

    klogf("VMM Initialized. CR3 reloaded.\n");
        
    // Read active CR3 register directly from CPU to log active PML4 physical address
    uint64_t active_cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(active_cr3));
    klogf("vmm0: 4-level paging active, active cr3=%x\n", active_cr3);
    klogf("vmm0: hhdm mapped at 0xFFFF800000000000\n");
}