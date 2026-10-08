#include "vfs.h"
#include "../../string.h"

static mount_point_t *mount_list = NULL;

void vfs_init(void) {
    mount_list = NULL;
}

// Mount a filesystem root node to a specific virtual path
int vfs_mount(const char *path, vfs_node_t *root) {
    mount_point_t *new_mount = (mount_point_t *)kmalloc(sizeof(mount_point_t));
    if (!new_mount) return -1;

    strncpy(new_mount->path, path, 127);
    new_mount->root_node = root;
    new_mount->next = mount_list;
    mount_list = new_mount;

    return 0;
}