#include "vfs.h"
#include "runtime/memory.h"
#define BLOCK_SIZE 4096u
#define FILE_BLOCKS (FS_FILE_MAX / BLOCK_SIZE)
struct node {
    char name[FS_NAME_MAX + 1u];
    size_t size;
    uint32_t uid, mode;
    uint16_t blocks[FILE_BLOCKS], parent, references;
    bool live, linked, directory;
};
static struct node nodes[FS_NODE_MAX];
static unsigned char data[FS_BLOCK_COUNT][BLOCK_SIZE];
static bool allocated[FS_BLOCK_COUNT];
/* Current identity is scoped by syscall wrappers while IF=0. No operation
 * sleeps or retains this identity. Trusted boot/manager code uses uid 0. */
static uint32_t current_uid;
void fs_set_uid(uint32_t uid) { current_uid = uid; }
static bool allowed(const struct node *node, unsigned int rights) { return current_uid == 0 || ((node->mode >> (current_uid == node->uid ? 6 : 0)) & rights) == rights; }
static bool mutable_entry(const struct node *node) { const struct node *parent = &nodes[node->parent]; return allowed(parent, 3) && ((parent->mode & 01000u) == 0 || current_uid == 0 || current_uid == parent->uid || current_uid == node->uid); }

static bool equal(const char *a, const char *b) {
    for (size_t i = 0; ; ++i) { if (a[i] != b[i]) { return false; } if (a[i] == '\0') { return true; } }
}
/* All entry points are serialized by the bootstrap CPU (syscall interrupt gate
 * or boot manager). No live user pointer is retained by this RAM filesystem. */
static long child(unsigned int parent, const char *name) {
    for (unsigned int i = 1; i < FS_NODE_MAX; ++i) {
        if (nodes[i].live && nodes[i].linked && nodes[i].parent == parent && equal(nodes[i].name, name)) { return (long)i; }
    }
    return FS_ENOENT;
}
static long resolve(const char *path, bool parent_only, char *leaf) {
    if (path == NULL || path[0] != '/') { return FS_EINVAL; }
    size_t length = 0;
    while (length < FS_PATH_MAX && path[length] != '\0') { ++length; }
    if (length == FS_PATH_MAX) { return FS_ENAMETOOLONG; }
    unsigned int current = 0;
    size_t position = 1;
    while (position < length) {
        while (position < length && path[position] == '/') { ++position; }
        if (position == length) { break; }
        if (!nodes[current].directory) { return FS_ENOTDIR; }
        if (!allowed(&nodes[current], 1)) { return FS_EACCES; }
        char component[FS_NAME_MAX + 1u] = {0}; size_t count = 0;
        while (position < length && path[position] != '/') {
            if (count == FS_NAME_MAX) { return FS_ENAMETOOLONG; }
            component[count++] = path[position++];
        }
        component[count] = '\0';
        if (equal(component, ".")) { continue; }
        if (equal(component, "..")) { if (current == 0) { return FS_EINVAL; } current = nodes[current].parent; continue; }
        size_t next = position;
        while (next < length && path[next] == '/') { ++next; }
        if (!nodes[current].directory) { return FS_ENOTDIR; }
        if (parent_only && next == length) { memcpy(leaf, component, count + 1); return (long)current; }
        long found = child(current, component);
        if (found < 0) { return found; }
        current = (unsigned int)found;
    }
    return parent_only ? FS_EINVAL : (long)current;
}
static void truncate_node(struct node *node) {
    for (size_t i = 0; i < FILE_BLOCKS; ++i) {
        if (node->blocks[i] != 0) { allocated[node->blocks[i] - 1u] = false; node->blocks[i] = 0; }
    }
    node->size = 0;
}
static void release_node(struct node *node) {
    if (!node->linked && node->references == 0) { truncate_node(node); memset(node, 0, sizeof(*node)); }
}
static long create(const char *path, bool directory) {
    char name[FS_NAME_MAX + 1u];
    long parent = resolve(path, true, name);
    if (parent < 0) { return parent; }
    if (!allowed(&nodes[parent], 3)) { return FS_EACCES; }
    if (child((unsigned int)parent, name) >= 0) { return FS_EEXIST; }
    for (unsigned int i = 1; i < FS_NODE_MAX; ++i) {
        if (nodes[i].live) { continue; }
        memset(&nodes[i], 0, sizeof(nodes[i]));
        memcpy(nodes[i].name, name, sizeof(name));
        nodes[i].parent = (uint16_t)parent;
        nodes[i].uid = current_uid; nodes[i].mode = directory ? 0700u : 0600u;
        nodes[i].live = nodes[i].linked = true; nodes[i].directory = directory;
        return (long)i;
    }
    return FS_ENOSPC;
}
void fs_init(void) {
    memset(nodes, 0, sizeof(nodes)); memset(allocated, 0, sizeof(allocated));
    current_uid = 0; nodes[0].mode = 0755;
    nodes[0].live = nodes[0].linked = nodes[0].directory = true;
}
long fs_mkdir(const char *path) { long result = create(path, true); return result < 0 ? result : 0; }
static struct fs_fd *descriptor(struct fs_handles *handles, uint64_t fd) {
    return handles != NULL && fd >= 3 && fd < FS_FD_MAX + 3u && handles->fd[fd - 3].live ? &handles->fd[fd - 3] : NULL;
}
long fs_open(struct fs_handles *handles, const char *path, unsigned int flags) {
    if (handles == NULL || (flags & ~(FS_READ | FS_WRITE | FS_CREATE | FS_TRUNCATE | FS_APPEND)) != 0 ||
        (flags & (FS_READ | FS_WRITE)) == 0 || ((flags & (FS_CREATE | FS_TRUNCATE | FS_APPEND)) != 0 && (flags & FS_WRITE) == 0)) { return FS_EINVAL; }
    unsigned int slot = 0;
    while (slot < FS_FD_MAX && handles->fd[slot].live) { ++slot; }
    if (slot == FS_FD_MAX) { return FS_EMFILE; }
    long index = resolve(path, false, NULL);
    if (index == FS_ENOENT && (flags & FS_CREATE) != 0) { index = create(path, false); }
    if (index < 0) { return index; }
    struct node *node = &nodes[index];
    if (node->directory) { return FS_EISDIR; }
    unsigned int rights = ((flags & FS_READ) != 0 ? 4u : 0u) | ((flags & FS_WRITE) != 0 ? 2u : 0u);
    if (!allowed(node, rights)) { return FS_EACCES; }
    if ((flags & FS_TRUNCATE) != 0) { truncate_node(node); }
    ++node->references;
    handles->fd[slot] = (struct fs_fd){.node = (uint16_t)index, .flags = (uint8_t)flags, .live = true};
    return (long)(slot + 3u);
}
long fs_close(struct fs_handles *handles, uint64_t fd) {
    struct fs_fd *file = descriptor(handles, fd);
    if (file == NULL) { return FS_EBADF; }
    struct node *node = &nodes[file->node];
    --node->references; memset(file, 0, sizeof(*file)); release_node(node); return 0;
}
void fs_close_all(struct fs_handles *handles) { for (uint64_t i = 3; i < FS_FD_MAX + 3u; ++i) { (void)fs_close(handles, i); } }
long fs_seek(struct fs_handles *handles, uint64_t fd, uint64_t offset) {
    struct fs_fd *file = descriptor(handles, fd);
    if (file == NULL) { return FS_EBADF; }
    if (offset > FS_FILE_MAX) { return FS_EINVAL; }
    file->offset = offset; return (long)offset;
}
long fs_read(struct fs_handles *handles, uint64_t fd, void *buffer, size_t length) {
    struct fs_fd *file = descriptor(handles, fd);
    if (file == NULL || (file->flags & FS_READ) == 0) { return FS_EBADF; }
    struct node *node = &nodes[file->node];
    if (file->offset >= node->size) { return 0; }
    size_t count = node->size - (size_t)file->offset;
    if (count > length) { count = length; }
    for (size_t i = 0; i < count; ++i) {
        size_t offset = (size_t)file->offset + i;
        uint16_t block = node->blocks[offset / BLOCK_SIZE];
        ((unsigned char *)buffer)[i] = block == 0 ? 0 : data[block - 1u][offset % BLOCK_SIZE];
    }
    file->offset += count; return (long)count;
}
long fs_write(struct fs_handles *handles, uint64_t fd, const void *buffer, size_t length) {
    struct fs_fd *file = descriptor(handles, fd);
    if (file == NULL || (file->flags & FS_WRITE) == 0) { return FS_EBADF; }
    struct node *node = &nodes[file->node];
    uint64_t offset = (file->flags & FS_APPEND) != 0 ? node->size : file->offset;
    if (length > FS_FILE_MAX - offset) { return FS_ENOSPC; }
    if (length == 0) { return 0; }
    size_t first = (size_t)offset / BLOCK_SIZE, last = ((size_t)offset + length - 1u) / BLOCK_SIZE, needed = 0, free_count = 0;
    for (size_t i = first; i <= last; ++i) { if (node->blocks[i] == 0) { ++needed; } }
    for (size_t i = 0; i < FS_BLOCK_COUNT; ++i) { if (!allocated[i]) { ++free_count; } }
    if (free_count < needed) { return FS_ENOSPC; }
    for (size_t i = first; i <= last; ++i) {
        if (node->blocks[i] != 0) { continue; }
        for (size_t j = 0; j < FS_BLOCK_COUNT; ++j) {
            if (allocated[j]) { continue; }
            allocated[j] = true; memset(data[j], 0, BLOCK_SIZE); node->blocks[i] = (uint16_t)(j + 1u); break;
        }
    }
    for (size_t i = 0; i < length; ++i) {
        size_t target = (size_t)offset + i;
        data[node->blocks[target / BLOCK_SIZE] - 1u][target % BLOCK_SIZE] = ((const unsigned char *)buffer)[i];
    }
    file->offset = offset + length;
    if (file->offset > node->size) { node->size = (size_t)file->offset; }
    return (long)length;
}
long fs_unlink(const char *path) {
    long index = resolve(path, false, NULL);
    if (index < 0) { return index; }
    if (index == 0) { return FS_EINVAL; }
    if (!mutable_entry(&nodes[index])) { return FS_EACCES; }
    for (size_t i = 1; i < FS_NODE_MAX; ++i) { if (nodes[i].live && nodes[i].linked && nodes[i].parent == (unsigned long)index) { return FS_ENOTEMPTY; } }
    nodes[index].linked = false; release_node(&nodes[index]); return 0;
}
long fs_rename(const char *source, const char *destination) {
    long index = resolve(source, false, NULL);
    if (index < 0) { return index; }
    if (index == 0) { return FS_EINVAL; }
    char name[FS_NAME_MAX + 1u]; long parent = resolve(destination, true, name);
    if (parent < 0) { return parent; }
    if (!mutable_entry(&nodes[index]) || !allowed(&nodes[parent], 3)) { return FS_EACCES; }
    long exists = child((unsigned int)parent, name);
    if (exists == index) { return 0; }
    if (exists >= 0) { return FS_EEXIST; }
    for (unsigned int ancestor = (unsigned int)parent; ancestor != 0; ancestor = nodes[ancestor].parent) {
        if (ancestor == (unsigned long)index) { return FS_EINVAL; }
    }
    nodes[index].parent = (uint16_t)parent; memcpy(nodes[index].name, name, sizeof(name)); return 0;
}
static void info_for(const struct node *node, struct fs_info *info) {
    *info = (struct fs_info){.size = node->size, .directory = node->directory ? 1u : 0u, .mode = node->mode, .uid = node->uid};
}
long fs_stat(const char *path, struct fs_info *info) {
    long index = resolve(path, false, NULL); if (index < 0) { return index; }
    info_for(&nodes[index], info); return 0;
}
long fs_list(const char *path, uint64_t index, struct fs_entry *entry) {
    long parent = resolve(path, false, NULL); if (parent < 0) { return parent; }
    if (!nodes[parent].directory) { return FS_ENOTDIR; }
    if (!allowed(&nodes[parent], 5)) { return FS_EACCES; }
    uint64_t position = 0;
    for (size_t i = 1; i < FS_NODE_MAX; ++i) {
        if (!nodes[i].live || !nodes[i].linked || nodes[i].parent != (unsigned long)parent) { continue; }
        if (position++ == index) { memset(entry, 0, sizeof(*entry)); memcpy(entry->name, nodes[i].name, sizeof(entry->name)); info_for(&nodes[i], &entry->info); return 1; }
    }
    return 0;
}
long fs_seed(const char *path, const void *bytes, size_t length) {
    struct fs_handles handles = {0};
    long fd = fs_open(&handles, path, FS_CREATE | FS_WRITE | FS_TRUNCATE);
    if (fd < 0) { return fd; }
    long result = fs_write(&handles, (uint64_t)fd, bytes, length); (void)fs_close(&handles, (uint64_t)fd);
    return result < 0 ? result : fs_chmod(path, 0644);
}
long fs_chmod(const char *path, uint32_t mode) {
    if ((mode & ~01777u) != 0) { return FS_EINVAL; }
    long index = resolve(path, false, NULL); if (index < 0) { return index; }
    if (current_uid != 0 && current_uid != nodes[index].uid) { return FS_EPERM; }
    nodes[index].mode = mode; return 0;
}
long fs_chown(const char *path, uint32_t uid) {
    if (current_uid != 0) { return FS_EPERM; }
    long index = resolve(path, false, NULL); if (index < 0) { return index; }
    nodes[index].uid = uid; return 0;
}
long fs_seed_program(const char *path, const void *bytes, size_t length) {
    long result = fs_seed(path, bytes, length); return result < 0 ? result : fs_chmod(path, 0755);
}
size_t fs_used_blocks(void) { size_t used = 0; for (size_t i = 0; i < FS_BLOCK_COUNT; ++i) { if (allocated[i]) { ++used; } } return used; }
