#ifndef AHCI_H
#define AHCI_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
// 1. Constants
#define AHCI_DEV_NULL 0
#define AHCI_DEV_SATA 1
#define AHCI_DEV_SEBP 2
#define AHCI_DEV_SATAPI 3
#define AHCI_DEV_EMB 4

#define HBA_PORT_IPM_ACTIVE 1
#define HBA_PORT_DET_PRESENT 3

// 2. Type definitions FIRST so the compiler knows them
typedef volatile struct tag_hba_port {
    uint32_t clb;
    uint32_t clbu;
    uint32_t fb;
    uint32_t fbu;
    uint32_t is;
    uint32_t ie;
    uint32_t cmd;
    uint32_t rsv0;
    uint32_t tfd;
    uint32_t sig;
    uint32_t ssts;
    uint32_t sctl;
    uint32_t serr;
    uint32_t sact;
    uint32_t ci;
    uint32_t sntf;
    uint32_t fbs;
    uint32_t rsv1[11];
    uint32_t vendor[4];
} hba_port_t;

typedef volatile struct tag_hba_mem {
    uint32_t cap;
    uint32_t ghc;
    uint32_t is;
    uint32_t pi;
    uint32_t vs;
    uint32_t cctl;
    uint32_t ccpts;
    uint32_t em_loc;
    uint32_t em_ctl;
    uint32_t cap2;
    uint32_t bohc;
    uint8_t  rsv[0xA0 - 0x2C];
    uint8_t  vendor[0x100 - 0xA0];
    hba_port_t ports[32];
} hba_mem_t;

// 3. Function prototypes AFTER the types are declared
int check_device_type(hba_port_t *port);
void probe_ahci_ports(hba_mem_t *abar);
bool ahci_read(hba_port_t *port, uint64_t start_lba, uint32_t count, void *buffer);
int find_cmdslot(hba_port_t *port);
int check_device_type(hba_port_t *port);
void stop_cmd(hba_port_t *port);
void start_cmd(hba_port_t *port);
void port_rebase(hba_port_t *port, int portno);

#endif