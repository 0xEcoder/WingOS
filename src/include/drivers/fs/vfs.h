#ifndef VFS_H
#define VFS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

struct vfs_node;

// File operations structure (vtable) for filesystem drivers
typedef struct file_operations {
    int (*read)(struct vfs_node *node, uint64_t offset, uint32_t size, void *buffer);
    int (*write)(struct vfs_node *node, uint64_t offset, uint32_t size, void *buffer);
    int (*lookup)(struct vfs_node *node, const char *name, struct vfs_node *out_result);
    int (*readdir)(struct vfs_node *node, uint32_t index, char *out_name_buf);
} file_operations_t;

// Generic VFS Node representing a file, folder, or device
typedef struct vfs_node {
    char name[128];
    uint32_t flags;         // File type (directory, file, mountpoint, etc.)
    uint32_t inode;         // Underlying filesystem inode number
    uint32_t length;        // File size in bytes
    file_operations_t *ops; // Pointer to filesystem-specific operations
    void *device_data;      // Driver-specific data (e.g., ext2 structure reference)
} vfs_node_t;

// Mount point tracking structure
typedef struct mount_point {
    char path[128];         // Mount path (e.g., "/")
    vfs_node_t *root_node;  // Root node of the mounted filesystem
    struct mount_point *next;
} mount_point_t;

// Core VFS Initialization and System Calls
void vfs_init(void);
int vfs_mount(const char *path, vfs_node_t *root);
vfs_node_t *vfs_resolve_path(const char *path);

#endif