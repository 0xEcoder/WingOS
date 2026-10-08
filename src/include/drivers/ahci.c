#include <stdint.h>
#include <stddef.h>
#include "ahci.h"
#include <stdbool.h>
#include "../pmm.h"
#include "../vmm.h"
#include "../console.h"
#include "../string.h" 

// Definitions for AHCI Base Registers and Constants
#define AHCI_DEV_NULL 0
#define AHCI_DEV_SATA 1
#define AHCI_DEV_SEBP 2
#define AHCI_DEV_SATAPI 3
#define AHCI_DEV_EMB 4

#define HBA_PORT_IPM_ACTIVE 1
#define HBA_PORT_DET_PRESENT 3

// Generic Host Control Register structure
typedef volatile struct tag_hba_port {
    uint32_t clb;       // 0x00, command list base address, 32-bit, 1024-byte aligned
    uint32_t clbu;      // 0x04, command list base address upper 32-bit
    uint32_t fb;        // 0x08, FIS base address, 256-byte aligned
    uint32_t fbu;       // 0x0C, FIS base address upper 32-bit
    uint32_t is;        // 0x10, interrupt status
    uint32_t ie;        // 0x14, interrupt enable
    uint32_t cmd;       // 0x18, command and status
    uint32_t rsv0;      // 0x1C, reserved
    uint32_t tfd;       // 0x20, task file data
    uint32_t sig;       // 0x24, signature
    uint32_t ssts;      // 0x28, SATA status (SCR0:Status)
    uint32_t sctl;      // 0x2C, SATA control (SCR1:Control)
    uint32_t serr;      // 0x30, SATA error (SCR2:Error)
    uint32_t sact;      // 0x34, SATA active (SCR3:Active)
    uint32_t ci;        // 0x38, command issue
    uint32_t sntf;      // 0x3C, SATA notification
    uint32_t fbs;       // 0x40, FIS-based switch control
    uint32_t rsv1[11];  // 0x44-0x6F, reserved
    uint32_t vendor[4]; // 0x70-0x7F, vendor specific
} hba_port_t;

typedef volatile struct tag_hba_mem {
    uint32_t cap;       // 0x00, Host Capability
    uint32_t ghc;       // 0x04, Global Host Control
    uint32_t is;        // 0x08, Interrupt Status
    uint32_t pi;        // 0x0C, Ports Implemented
    uint32_t vs;        // 0x10, Version
    uint32_t cctl;      // 0x14, Command Completion Control
    uint32_t ccpts;     // 0x18, Command Completion Ports
    uint32_t em_loc;    // 0x1C, Enclosure Management Location
    uint32_t em_ctl;    // 0x20, Enclosure Management Control
    uint32_t cap2;      // 0x24, Host Capabilities Extended
    uint32_t bohc;      // 0x28, BIOS/OS Handoff Control and Status
    uint8_t  rsv[0xA0 - 0x2C];
    uint8_t  vendor[0x100 - 0xA0];
    hba_port_t ports[32]; // 1st port starts at 0x100
} hba_mem_t;

// --- AHCI Command & Read Structures ---
#define ATA_CMD_READ_DMA_EX  0x25
#define FIS_TYPE_REG_H2D     0x27

typedef struct tag_fis_reg_h2d {
    uint8_t  fis_type;
    uint8_t  pmport:4;
    uint8_t  rsv0:3;
    uint8_t  c:1;
    uint8_t  command;
    uint8_t  featurel;
    uint8_t  lba0, lba1, lba2;
    uint8_t  device;
    uint8_t  lba3, lba4, lba5;
    uint8_t  featureh;
    uint8_t  countl, counth;
    uint8_t  icc;
    uint8_t  control;
    uint32_t rsv1;
} __attribute__((packed)) fis_reg_h2d_t;

typedef struct tag_ahci_prdt_entry {
    uint32_t dba;
    uint32_t dbau;
    uint32_t rsv0;
    uint32_t dbc:22;
    uint32_t rsv1:9;
    uint32_t i:1;
} __attribute__((packed)) ahci_prdt_entry_t;

typedef struct tag_ahci_cmd_tbl {
    uint8_t  cfis[64];
    uint8_t  acmd[16];
    uint8_t  rsv[48];
    ahci_prdt_entry_t prdt_entry[8]; // Matching our 8 PRDTs from rebase
} __attribute__((packed)) ahci_cmd_tbl_t;

typedef struct tag_ahci_cmd_header {
    uint8_t  cfl:5;
    uint8_t  a:1;
    uint8_t  w:1;
    uint8_t  p:1;
    uint8_t  r:1;
    uint8_t  b:1;
    uint8_t  c:1;
    uint8_t  rsv0:1;
    uint8_t  pmp:4;
    uint16_t prdtl;
    uint32_t prdbc;
    uint32_t ctba;
    uint32_t ctbau;
    uint32_t rsv1[4];
} __attribute__((packed)) ahci_cmd_header_t;

// Global tracker for our working drive
hba_port_t *boot_drive_port = NULL;

// Find an available command slot (0-31)
int find_cmdslot(hba_port_t *port) {
    uint32_t slots = (port->sact | port->ci);
    for (int i = 0; i < 32; i++) {
        if ((slots & (1 << i)) == 0) return i;
    }
    klogf("ahci: Error - no free command slots available.\n");
    return -1;
}

// Read sectors from the SATA drive via AHCI
bool ahci_read(hba_port_t *port, uint64_t start_lba, uint32_t count, void *buffer) {
    port->is = (uint32_t)-1; // Clear pending interrupts
    int slot = find_cmdslot(port);
    if (slot == -1) return false;

    // Get command header (using HHDM virtual offset)
    ahci_cmd_header_t *cmdheader = (ahci_cmd_header_t *)((uintptr_t)port->clb + HHDM_OFFSET);
    cmdheader += slot;
    cmdheader->cfl = sizeof(fis_reg_h2d_t) / sizeof(uint32_t); // 5 DWORDs
    cmdheader->w = 0; // Read operation
    cmdheader->prdtl = 1; // Using 1 PRDT entry for this transfer

    // Get command table associated with this header
    ahci_cmd_tbl_t *cmdtbl = (ahci_cmd_tbl_t *)((uintptr_t)cmdheader->ctba + HHDM_OFFSET);
    memset(cmdtbl, 0, sizeof(ahci_cmd_tbl_t));

    // 3. Setup PRDT entry (Hardware needs the PHYSICAL address of the destination buffer)
    uint64_t phys_buffer = (uint64_t)buffer - HHDM_OFFSET;
    cmdtbl->prdt_entry[0].dba = (uint32_t)phys_buffer;
    cmdtbl->prdt_entry[0].dbau = (uint32_t)(phys_buffer >> 32);
    cmdtbl->prdt_entry[0].dbc = (count * 512) - 1; // Byte count (0-indexed)
    cmdtbl->prdt_entry[0].i = 1; // Interrupt on completion

    // 4. Setup Host-to-Device Register FIS
    fis_reg_h2d_t *fis = (fis_reg_h2d_t *)(&cmdtbl->cfis);
    fis->fis_type = FIS_TYPE_REG_H2D;
    fis->c = 1; // Command packet
    fis->command = ATA_CMD_READ_DMA_EX;

    // 48-bit LBA addressing
    fis->lba0 = (uint8_t)start_lba;
    fis->lba1 = (uint8_t)(start_lba >> 8);
    fis->lba2 = (uint8_t)(start_lba >> 16);
    fis->device = 1 << 6; // LBA mode enable
    fis->lba3 = (uint8_t)(start_lba >> 24);
    fis->lba4 = (uint8_t)(start_lba >> 32);
    fis->lba5 = (uint8_t)(start_lba >> 40);
    fis->countl = (uint8_t)count;
    fis->counth = (uint8_t)(count >> 8);

    // Ring the doorbell to issue the command
    port->ci |= (1 << slot);

    // Wait for command completion
    while (1) {
        if ((port->ci & (1 << slot)) == 0) break; // Finished successfully
        if (port->is & (1 << 30)) {               // Task file error bit
            klogf("ahci: disk read error encountered.\n");
            return false;
        }
    }

    return true;
}

// Mirroring your ahci_read function for writing
bool ahci_write(hba_port_t *port, uint64_t start_lba, uint32_t count, void *buffer) {
    port->is = (uint32_t)-1;
    int slot = find_cmdslot(port);
    if (slot == -1) return false;

    ahci_cmd_header_t *cmdheader = (ahci_cmd_header_t *)((uintptr_t)port->clb + HHDM_OFFSET);
    cmdheader += slot;
    cmdheader->cfl = sizeof(fis_reg_h2d_t) / sizeof(uint32_t);
    cmdheader->w = 1; // 1 = WRITE operation (unlike 0 for read)
    cmdheader->prdtl = 1;

    ahci_cmd_tbl_t *cmdtbl = (ahci_cmd_tbl_t *)((uintptr_t)cmdheader->ctba + HHDM_OFFSET);
    memset(cmdtbl, 0, sizeof(ahci_cmd_tbl_t));

    uint64_t phys_buffer = (uint64_t)buffer - HHDM_OFFSET;
    cmdtbl->prdt_entry[0].dba = (uint32_t)phys_buffer;
    cmdtbl->prdt_entry[0].dbau = (uint32_t)(phys_buffer >> 32);
    cmdtbl->prdt_entry[0].dbc = (count * 512) - 1;
    cmdtbl->prdt_entry[0].i = 1;

    fis_reg_h2d_t *fis = (fis_reg_h2d_t *)(&cmdtbl->cfis);
    fis->fis_type = FIS_TYPE_REG_H2D;
    fis->c = 1;
    fis->command = 0x35; // ATA_CMD_WRITE_DMA_EX command opcode

    fis->lba0 = (uint8_t)start_lba;
    fis->lba1 = (uint8_t)(start_lba >> 8);
    fis->lba2 = (uint8_t)(start_lba >> 16);
    fis->device = 1 << 6;
    fis->lba3 = (uint8_t)(start_lba >> 24);
    fis->lba4 = (uint8_t)(start_lba >> 32);
    fis->lba5 = (uint8_t)(start_lba >> 40);
    fis->countl = (uint8_t)count;
    fis->counth = (uint8_t)(count >> 8);

    port->ci |= (1 << slot);

    while (1) {
        if ((port->ci & (1 << slot)) == 0) break;
        if (port->is & (1 << 30)) return false;
    }
    return true;
}

// Check which type of device is attached to a port
int check_device_type(hba_port_t *port) {
    uint32_t ssts = port->ssts;

    uint8_t ipm = (ssts >> 8) & 0x0F;
    uint8_t det = ssts & 0x07;

    if (det != HBA_PORT_DET_PRESENT) return AHCI_DEV_NULL;
    if (ipm != HBA_PORT_IPM_ACTIVE) return AHCI_DEV_NULL;

    switch (port->sig) {
        case 0xEB140101: return AHCI_DEV_SATAPI;
        case 0xC33C0101: return AHCI_DEV_SEBP;
        case 0x96690101: return AHCI_DEV_EMB;
        default:         return AHCI_DEV_SATA;
    }
}
// Probe AHCI ports mapped via ABAR (Base Address Register 5 pointer)
// Probe AHCI ports mapped via ABAR (Base Address Register 5 pointer)
void probe_ahci_ports(hba_mem_t *abar) {
    // Enable AHCI mode via Global Host Control
    abar->ghc |= (1 << 31);

    uint32_t pi = abar->pi;
    for (int i = 0; i < 32; i++) {
        if (pi & (1 << i)) {
            int dt = check_device_type(&abar->ports[i]);
            if (dt == AHCI_DEV_SATA) {
                klogf("ahci: Found live physical SATA drive on port %d!\n", i);
                
                // 1. Rebase the port to set up our custom command lists & memory spaces
                port_rebase(&abar->ports[i], i);
                
                // 2. Assign it as our active boot drive if we haven't already
                if (boot_drive_port == NULL) {
                    boot_drive_port = &abar->ports[i];
                    klogf("ahci: Assigned port %d as primary boot drive.\n", i);
                }
            }
        }
    }
}

// Stop command engine
void stop_cmd(hba_port_t *port) {
    port->cmd &= ~1; // Clear ST (Start)
    while (port->cmd & (1 << 15)); // Wait until CR (Command Running) is 0
}

// Start command engine
void start_cmd(hba_port_t *port) {
    while (port->cmd & (1 << 15)); // Wait until CR is 0
    port->cmd |= (1 << 4); // Set FRE (FIS Receive Enable)
    port->cmd |= 1;        // Set ST (Start)
}

// Rebase a port to use kernel-managed physical memory blocks
void port_rebase(hba_port_t *port, int portno) {
    stop_cmd(port);

    // 1. Allocate 1 physical page for the Command List (1024 bytes)
    void *clb_phys = pmm_alloc_page();
    if (!clb_phys) return;
    port->clb = (uint32_t)(uintptr_t)clb_phys;
    port->clbu = (uint32_t)((uintptr_t)clb_phys >> 32);
    memset((void *)((uintptr_t)clb_phys + HHDM_OFFSET), 0, 1024);

    // 2. Allocate 1 physical page for the FIS Receive Area (256 bytes)
    void *fb_phys = pmm_alloc_page();
    if (!fb_phys) return;
    port->fb = (uint32_t)(uintptr_t)fb_phys;
    port->fbu = (uint32_t)((uintptr_t)fb_phys >> 32);
    memset((void *)((uintptr_t)fb_phys + HHDM_OFFSET), 0, 256);

    // 3. Allocate 1 physical page for Command Tables
    void *ctb_phys = pmm_alloc_page();
    if (!ctb_phys) return;
    uint32_t ctb_phys_addr = (uint32_t)(uintptr_t)ctb_phys;
    memset((void *)((uintptr_t)ctb_phys + HHDM_OFFSET), 0, 4096);

    ahci_cmd_header_t *cmdheader = (ahci_cmd_header_t *)((uintptr_t)port->clb + HHDM_OFFSET);
    for (int i = 0; i < 32; i++) {
        cmdheader[i].prdtl = 8; // 8 PRDT entries per command table
        uintptr_t cmd_table_addr = (uintptr_t)ctb_phys_addr + (i * 128);
        cmdheader[i].ctba = (uint32_t)cmd_table_addr;
        cmdheader[i].ctbau = (uint32_t)(cmd_table_addr >> 32);
    }

    start_cmd(port);
    klogf("ahci: port %d rebased and command engine started.\n", portno);
}