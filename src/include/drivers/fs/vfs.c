#include "vfs.h"
#include "../../heap.h"
#include "../../string.h" 

static mount_point_t *mount_list = NULL;

void vfs_init(void) {
    mount_list = NULL;
}

// Mount a filesystem root node dynamically using kmalloc
int vfs_mount(const char *path, vfs_node_t *root) {
    mount_point_t *new_mount = (mount_point_t *)kmalloc(sizeof(mount_point_t));
    if (!new_mount) return -1; // Out of memory

    strncpy(new_mount->path, path, 127);
    new_mount->root_node = root;
    new_mount->next = mount_list; // Prepend to list
    mount_list = new_mount;

    return 0;
}

// Resolve a path to its corresponding VFS node
vfs_node_t *vfs_resolve_path(const char *path) {
    // For a single-drive setup mounted at "/", anything matches the root mount for now
    if (mount_list != NULL && strcmp(path, "/") == 0) {
        return mount_list->root_node;
    }
    
    // If you want to look up files inside the mounted root:
    if (mount_list != NULL) {
        vfs_node_t *current = mount_list->root_node;
        // You can call your ext2_vfs_lookup or general vfs lookup here
        vfs_node_t *result_node = (vfs_node_t *)kmalloc(sizeof(vfs_node_t));
        if (current->ops && current->ops->lookup(current, path + 1, result_node) == 0) {
            return result_node;
        }
    }
    return NULL;
}