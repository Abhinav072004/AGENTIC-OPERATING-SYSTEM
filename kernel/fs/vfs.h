#ifndef AGENTICOS_VFS_H
#define AGENTICOS_VFS_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define FS_PATH_MAX 128u
#define FS_NAME_MAX 31u
#define FS_NODE_MAX 64u
#define FS_FD_MAX 16u
#define FS_FILE_MAX 65536u
#define FS_BLOCK_COUNT 128u
#define FS_READ 1u
#define FS_WRITE 2u
#define FS_CREATE 4u
#define FS_TRUNCATE 8u
#define FS_APPEND 16u
#define FS_EACCES (-13L)
#define FS_EPERM (-1L)
#define FS_ENOENT (-2L)
#define FS_EIO (-5L)
#define FS_EBADF (-9L)
#define FS_EEXIST (-17L)
#define FS_ENOTDIR (-20L)
#define FS_EISDIR (-21L)
#define FS_EINVAL (-22L)
#define FS_EMFILE (-24L)
#define FS_ENOSPC (-28L)
#define FS_ENAMETOOLONG (-36L)
#define FS_ENOTEMPTY (-39L)
struct fs_fd { uint64_t offset; uint16_t node; uint8_t flags; bool live; };
struct fs_handles { struct fs_fd fd[FS_FD_MAX]; };
struct fs_info { uint64_t size; uint32_t directory; uint32_t mode, uid, reserved; };
struct fs_entry { char name[32]; struct fs_info info; };
void fs_init(void);
void fs_set_uid(uint32_t uid);
long fs_chmod(const char *path, uint32_t mode);
long fs_chown(const char *path, uint32_t uid);
long fs_seed_program(const char *path, const void *bytes, size_t length);
long fs_mkdir(const char *path);
long fs_open(struct fs_handles *handles, const char *path, unsigned int flags);
long fs_close(struct fs_handles *handles, uint64_t fd);
void fs_close_all(struct fs_handles *handles);
long fs_read(struct fs_handles *handles, uint64_t fd, void *buffer, size_t length);
long fs_write(struct fs_handles *handles, uint64_t fd, const void *buffer, size_t length);
long fs_seek(struct fs_handles *handles, uint64_t fd, uint64_t offset);
long fs_unlink(const char *path);
long fs_rename(const char *source, const char *destination);
long fs_stat(const char *path, struct fs_info *info);
long fs_list(const char *path, uint64_t index, struct fs_entry *entry);
long fs_seed(const char *path, const void *bytes, size_t length);
size_t fs_used_blocks(void);
void fs_self_test(void);
#endif
