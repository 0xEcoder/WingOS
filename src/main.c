#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "include/limine.h"
#include "include/gdt.h"
#include "include/idt.h"
#include "include/console.h"
#include "include/pmm.h"
#include "include/keyboard.h"
#include "include/vmm.h"
#include "include/heap.h"
#include "include/timer.h"
#include "include/shell.h"
#include "include/drivers/ahci.h"

bool g_cli_mode_enabled = false;

extern hba_port_t *boot_drive_port;

// 1. START MARKER
__attribute__((used, section(".requests_start_marker")))
static volatile uint64_t limine_requests_start_marker[] = LIMINE_REQUESTS_START_MARKER;

// 2. Base Revision Tag (MUST BE BETWEEN START & END MARKERS)
__attribute__((used, section(".requests")))
static volatile uint64_t limine_base_revision[3] = LIMINE_BASE_REVISION(3);

// 3. Requests
__attribute__((used, section(".requests")))
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".requests")))
volatile struct limine_executable_address_request exec_address_request = {
    .id = LIMINE_EXECUTABLE_ADDRESS_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".requests")))
volatile struct limine_executable_cmdline_request cmdline_request = {
    .id = LIMINE_EXECUTABLE_CMDLINE_REQUEST_ID,
    .revision = 0
};

// HHDM (Higher-Half Direct Map) Request
__attribute__((used, section(".requests")))
static volatile struct limine_hhdm_request hhdm_request = {
    .id = LIMINE_HHDM_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".requests")))
volatile struct limine_memmap_request memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST_ID,
    .revision = 0
};

// 4. END MARKER (Must immediately follow requests)
__attribute__((used, section(".requests_end_marker")))
static volatile uint64_t limine_requests_end_marker[] = LIMINE_REQUESTS_END_MARKER;

static bool str_contains(const char *str, const char *search) {
    if (!str || !search) return false;

    for (size_t i = 0; str[i] != '\0'; i++) {
        size_t j = 0;
        while (str[i + j] != '\0' && search[j] != '\0' && str[i + j] == search[j]) {
            j++;
        }
        if (search[j] == '\0') {
            return true; // Match found
        }
    }
    return false;
}

static inline uint32_t inl(uint16_t port) {
    uint32_t ret;
    __asm__ volatile ( "inl %1, %0" : "=a"(ret) : "Nd"(port) );
    return ret;
}

static inline void outl(uint16_t port, uint32_t val) {
    __asm__ volatile ( "outl %0, %1" : : "a"(val), "Nd"(port) );
}

uint32_t pci_read_config(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t address = (uint32_t)((1 << 31) | (bus << 16) | (slot << 11) | (func << 8) | (offset & 0xFC));
    outl(0xCF8, address);
    return inl(0xCFC);
}

// Scan PCI bus to locate the AHCI Mass Storage Controller and retrieve BAR5 (ABAR)
uintptr_t pci_find_ahci_controller(void) {
    for (int bus = 0; bus < 256; bus++) {
        for (int slot = 0; slot < 32; slot++) {
            for (int func = 0; func < 8; func++) {
                uint32_t vendor_device = pci_read_config(bus, slot, func, 0x00);
                uint16_t vendor_id = vendor_device & 0xFFFF;
                
                if (vendor_id == 0xFFFF) continue;

                uint32_t class_rev = pci_read_config(bus, slot, func, 0x08);
                uint8_t class_code = (class_rev >> 24) & 0xFF;
                uint8_t subclass = (class_rev >> 16) & 0xFF;
                uint8_t prog_if = (class_rev >> 8) & 0xFF;

                // Mass Storage (0x01), SATA subclass (0x06), AHCI programming interface (0x01)
                if (class_code == 0x01 && subclass == 0x06 && prog_if == 0x01) {
                    uint32_t bar5 = pci_read_config(bus, slot, func, 0x24);
                    return (uintptr_t)(bar5 & 0xFFFFFFF0);
                }
            }
        }
    }
    return 0;
}

static inline uint64_t get_current_rip(void) {
    uint64_t rip;
    __asm__ volatile (
        "lea 0(%%rip), %0" 
        : "=r"(rip)
    );
    return rip;
}

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ( "outb %0, %1" : : "a"(val), "Nd"(port) );
}

void pic_init(void) {
    outb(0x20, 0x11);
    outb(0xA0, 0x11);
    outb(0x21, 0x20);
    outb(0xA1, 0x28);
    outb(0x21, 0x04);
    outb(0xA1, 0x02);
    outb(0x21, 0x01);
    outb(0xA1, 0x01);
    outb(0x21, 0xFC); 
    outb(0xA1, 0xFF); 
    klogf("pic0: PIC remapped, IRQs 0-15 -> vectors 32-47\n");
}

void _start(void) {
    // Disable hardware interrupts during early core system initialization
    __asm__ volatile("cli");
    // --- Framebuffer & Console Setup ---
    if (framebuffer_request.response == NULL) {
        // If response is NULL, Limine didn't see or handle the request
        // Trigger a QEMU debug break or infinite halt
        __asm__ volatile ("cli; hlt");
    }

    if (framebuffer_request.response->framebuffer_count == 0) {
        __asm__ volatile ("cli; hlt");
    }
    struct limine_framebuffer *fb = framebuffer_request.response->framebuffers[0];
    console_init(fb);

    gdt_init();
    idt_init();
    pic_init();

    // --- Hardware PIT Timer Setup ---
    timer_init(1000); // Set 1000 Hz frequency (1 tick = 1ms)
    // Enable CPU Interrupts
    __asm__ volatile("sti");
    klogf("core: sti called. interrupts enabled. RIP=%x\n", get_current_rip());

    // --- PMM Logging ---
    pmm_init();

    // --- VMM Logging ---
    vmm_init();

    // --- Heap Logging ---
    heap_init();;

    // --- Keyboard Driver Logging ---
    keyboard_init();
    
    keyboard_enable();

    klogf("ahci: scanning PCI bus for AHCI controller...\n");
    uintptr_t abar_physical = pci_find_ahci_controller();

    if (abar_physical == 0) {
        klogf("ahci: warning - no AHCI controller found on PCI bus.\n");
    } else {
        klogf("ahci: found AHCI controller physical base address at %x\n", abar_physical);
        
        if (hhdm_request.response == NULL) {
            klogf("ahci: error - HHDM response is NULL! Cannot map MMIO registers safely.\n");
        } else {
            uint64_t hhdm_offset = hhdm_request.response->offset;
            uintptr_t abar_virtual = abar_physical + hhdm_offset;
            
            // 1. Map the first 4KB page
            vmm_map_page(kernel_pml4, 
                         abar_virtual, 
                         abar_physical, 
                         PAGE_WRITABLE | PAGE_CACHE_DISABLE);
            
            // 2. Map the second 4KB page (because hba_mem_t is ~4.3KB total)
            vmm_map_page(kernel_pml4, 
                         abar_virtual + 0x1000, 
                         abar_physical + 0x1000, 
                         PAGE_WRITABLE | PAGE_CACHE_DISABLE);

            klogf("ahci: mapped AHCI controller to virtual address %x\n", abar_virtual);

            // 3. Now it is safely in your page tables. Cast and probe!
            hba_mem_t *abar = (hba_mem_t *)abar_virtual;
            probe_ahci_ports(abar);
        }
    }
    
    // Allocate a temporary 512-byte buffer in kernel memory
    uint8_t *sector_buffer = (uint8_t *)pmm_alloc_page() + HHDM_OFFSET;
    

    if (cmdline_request.response != NULL && cmdline_request.response->cmdline != NULL) {
        const char *cmdline = cmdline_request.response->cmdline;
        
        // Use str_contains or strstr to see if "cli" is present anywhere in the arguments
        if (str_contains(cmdline, "cli")) {
            g_cli_mode_enabled = true;
            klogf("core: 'cli' argument detected. Shell enabled.\n");
            kprintf("# "); // lazy fix
        } else {
            klogf("core: no 'cli' argument provided. Shell disabled.\n");
        }
    }
    while(1) {
        __asm__ volatile("hlt");
    }
}