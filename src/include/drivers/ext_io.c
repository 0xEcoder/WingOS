#include <stdint.h>
#include <stdbool.h>
#include "ahci.h"

// External reference to the boot drive port you found during AHCI probe
extern hba_port_t *boot_drive_port;

// embext block read wrapper
int block_read(uint32_t lba, uint32_t count, void *buffer) {
    if (!boot_drive_port) return -1;
    
    // Call your working AHCI read function!
    bool success = ahci_read(boot_drive_port, lba, count, buffer);
    return success ? 0 : -1; // 0 for success, -1 for error
}

// embext block write wrapper (if you want write support later)
int block_write(uint32_t lba, uint32_t count, void *buffer) {
    if (!boot_drive_port) return -1;
    
    // You can write an ahci_write function mirroring ahci_read if needed
    // bool success = ahci_write(boot_drive_port, lba, count, buffer);
    return -1; 
}