#include "../ahci.h"
#include "ext2.h"
#include "../../string.h"

extern hba_port_t *boot_drive_port;

static ext2_superblock_t sb;
static uint32_t block_size = 1024; // Default until parsed

bool ext2_init(void) {
    uint8_t buffer[1024]; // Two 512-byte sectors
    
    // Read sectors 2 and 3 (Bytes 1024 to 2047)
    if (!ahci_read(boot_drive_port, 2, 2, buffer)) {
        return false;
    }

    // Copy into our superblock structure
    sb = *(ext2_superblock_t *)buffer;

    // Validate Magic Number
    if (sb.s_magic != 0xEF53) {
        return false; // Not a valid Ext2 filesystem
    }

    // Calculate actual block size (1024 shifted left by s_log_block_size)
    block_size = 1024 << sb.s_log_block_size;
    return true;
}

bool ext2_read_inode(uint32_t inode_num, ext2_inode_t *out_inode) {
    // Determine which block group contains this inode
    uint32_t block_group = (inode_num - 1) / sb.s_inodes_per_group;
    
    // Find the index of the inode within its group
    uint32_t index = (inode_num - 1) % sb.s_inodes_per_group;

    // Read the Block Group Descriptor Table
    // The group descriptor table starts immediately after the superblock block.
    // If block_size is 1024, superblock is block 1, so descriptors start at block 2.
    uint32_t desc_block = (block_size == 1024) ? 2 : 1;
    uint8_t block_buf[4096];
    
    uint32_t sectors_per_block = block_size / 512;
    ahci_read(boot_drive_port, desc_block * sectors_per_block, sectors_per_block, block_buf);

    ext2_group_desc_t *desc = (ext2_group_desc_t *)block_buf;
    ext2_group_desc_t target_group = desc[block_group];

    // 4. Locate the Inode Table block for this group
    uint32_t inode_table_block = target_group.bg_inode_table;

    // 5. Calculate which block contains our specific inode
    uint32_t inode_size = sizeof(ext2_inode_t);
    uint32_t block_offset = (index * inode_size) / block_size;
    uint32_t inode_offset_in_block = (index * inode_size) % block_size;

    uint32_t target_sector = (inode_table_block + block_offset) * sectors_per_block;
    ahci_read(boot_drive_port, target_sector, sectors_per_block, block_buf);

    // Copy out the target inode
    *out_inode = *(ext2_inode_t *)(block_buf + inode_offset_in_block);
    return true;
}

bool ext2_read_file_data(ext2_inode_t *inode, void *buffer) {
    uint32_t sectors_per_block = block_size / 512;
    uint8_t *dest = (uint8_t *)buffer;
    uint32_t bytes_remaining = inode->i_size;

    for (int i = 0; i < 12 && bytes_remaining > 0; i++) {
        if (inode->i_block[i] == 0) break;

        uint32_t sector = inode->i_block[i] * sectors_per_block;
        uint32_t bytes_to_read = (bytes_remaining > block_size) ? block_size : bytes_remaining;

        // Read directly into our destination buffer using your AHCI driver
        ahci_read(boot_drive_port, sector, sectors_per_block, dest);

        dest += block_size;
        bytes_remaining -= bytes_to_read;
    }
    return true;
}

bool ext2_streq(const char *s1, const char *s2, size_t len) {
    for (size_t i = 0; i < len; i++) {
        if (s1[i] != s2[i]) return false;
        if (s1[i] == '\0') break;
    }
    return s2[len] == '\0';
}

// Find a file or folder inside a directory inode by name
uint32_t ext2_lookup(ext2_inode_t *dir_inode, const char *name) {
    uint32_t sectors_per_block = block_size / 512;
    uint8_t block_buf[4096]; // Assumes block size <= 4096

    // Loop through the directory's data blocks
    for (int i = 0; i < 12; i++) {
        if (dir_inode->i_block[i] == 0) break;

        uint32_t sector = dir_inode->i_block[i] * sectors_per_block;
        if (!ahci_read(boot_drive_port, sector, sectors_per_block, block_buf)) {
            return 0;
        }

        uint32_t current_offset = 0;
        while (current_offset < block_size) {
            ext2_dir_entry_t *entry = (ext2_dir_entry_t *)(block_buf + current_offset);

            if (entry->inode != 0 && entry->name_len > 0) {
                // Check if this entry matches the target name
                if (entry->name_len == strlen(name) && ext2_streq(entry->name, name, entry->name_len)) {
                    return entry->inode; // Found it! Return the Inode number.
                }
            }

            // Avoid infinite loops if rec_len is corrupted
            if (entry->rec_len == 0) break;
            current_offset += entry->rec_len;
        }
    }

    return 0; // Not found
}

bool ext2_read_file_path(const char *path, void *buffer) {
    // Initialize filesystem
    if (!ext2_init()) return false;

    // Start at the root directory inode (Inode 2)
    ext2_inode_t current_inode;
    if (!ext2_read_inode(2, &current_inode)) return false;

    // (Assuming a flat file lookup in root for simplicity, e.g., "hello.txt")
    uint32_t target_inode_num = ext2_lookup(&current_inode, path);
    if (target_inode_num == 0) return false;

    // Read the target file's inode
    if (!ext2_read_inode(target_inode_num, &current_inode)) return false;

    // Read the file contents directly into the buffer!
    return ext2_read_file_data(&current_inode, buffer);
}

// Resolve a full path (e.g., "/folder/file.txt") to an inode number
uint32_t ext2_resolve_path(const char *path) {
    if (path[0] != '/') return 0; // Must start at root

    ext2_inode_t current_inode;
    if (!ext2_read_inode(2, &current_inode)) return 0; // Start at root inode (2)

    const char *ptr = path + 1; // Skip leading slash
    if (*ptr == '\0') return 2; // Path is just "/"

    char name_buffer[256];
    while (*ptr != '\0') {
        // Extract the next segment name (up to '/' or end of string)
        int i = 0;
        while (*ptr != '/' && *ptr != '\0' && i < 255) {
            name_buffer[i++] = *ptr++;
        }
        name_buffer[i] = '\0';

        if (*ptr == '/') ptr++; // Skip the slash for the next loop

        // Look up this segment name in the current directory
        uint32_t next_inode_num = ext2_lookup(&current_inode, name_buffer);
        if (next_inode_num == 0) return 0; // Not found

        // Load the next inode to continue searching deeper
        if (!ext2_read_inode(next_inode_num, &current_inode)) return 0;

        // If we reached the end of the path string, return this final inode number
        if (*ptr == '\0') {
            return next_inode_num;
        }
    }

    return 0;
}