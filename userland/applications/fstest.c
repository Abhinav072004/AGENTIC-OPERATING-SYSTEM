#include "syscall.h"
#include "fs/vfs.h"
int user_main(uint64_t pid, uint64_t mode);
#define CHECK(test) do { if (!(test)) { return __LINE__; } } while (0)
static long path_call(uint64_t number, const char *path, uint64_t arg, uint64_t last) { return user_call(number, (uintptr_t)path, arg, last); }
int user_main(uint64_t pid, uint64_t mode) {
    (void)pid;
    const char *path = "/tmp/user-file";
    long fd = path_call(SYS_OPEN, path, FS_CREATE | FS_READ | FS_WRITE | FS_TRUNCATE, 0);
    CHECK(fd >= 3);
    CHECK(user_write((uint64_t)fd, "AgenticOS", 9) == 9);
    if (mode == 1) {
        CHECK(path_call(SYS_UNLINK, path, 0, 0) == 0);
        __asm__ volatile ("ud2");
        return 1;
    }
    CHECK(user_call(SYS_SEEK, (uint64_t)fd, 0, 0) == 0);
    char buffer[16] = {0};
    CHECK(user_read((uint64_t)fd, buffer, sizeof(buffer)) == 9);
    CHECK(buffer[0] == 'A' && buffer[8] == 'S');
    CHECK(user_read((uint64_t)fd, buffer, 1) == 0);
    CHECK(user_call(SYS_SEEK, (uint64_t)fd, 0, 0) == 0);
    CHECK(user_read((uint64_t)fd, (void *)(uintptr_t)0xffffffff80000000, 9) == SYS_EFAULT);
    CHECK(user_read((uint64_t)fd, buffer, 9) == 9 && buffer[0] == 'A');
    CHECK(path_call(SYS_RENAME, path, (uintptr_t)"/tmp/renamed", 0) == 0);
    struct fs_info info;
    CHECK(path_call(SYS_STAT, path, (uintptr_t)&info, 0) == FS_ENOENT);
    CHECK(path_call(SYS_STAT, "/tmp/renamed", (uintptr_t)&info, 0) == 0 && info.size == 9 && info.directory == 0);
    struct fs_entry entry;
    CHECK(path_call(SYS_LIST, "/tmp", 0, (uintptr_t)&entry) == 1 && entry.name[0] == 'r');
    CHECK(path_call(SYS_UNLINK, "/tmp/renamed", 0, 0) == 0);
    CHECK(user_call(SYS_SEEK, (uint64_t)fd, 0, 0) == 0 && user_read((uint64_t)fd, buffer, 9) == 9);
    CHECK(user_call(SYS_CLOSE, (uint64_t)fd, 0, 0) == 0);
    CHECK(user_call(SYS_CLOSE, (uint64_t)fd, 0, 0) == FS_EBADF);
    CHECK(path_call(SYS_MKDIR, "/tmp/dir", 0, 0) == 0);
    CHECK(path_call(SYS_OPEN, "/tmp/dir", FS_READ, 0) == FS_EISDIR);
    CHECK(path_call(SYS_RENAME, "/tmp/dir", (uintptr_t)"/tmp/dir/child", 0) == FS_EINVAL);
    CHECK(path_call(SYS_UNLINK, "/tmp/dir", 0, 0) == 0);
    CHECK(path_call(SYS_OPEN, "/../../escape", FS_CREATE | FS_WRITE, 0) == FS_EINVAL);
    CHECK(path_call(SYS_OPEN, (const char *)(uintptr_t)0x800000000000, FS_READ, 0) == SYS_EFAULT);
    CHECK(user_write(1, "native filesystem checks passed", 31) == 31);
    return 0;
}
