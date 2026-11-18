#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "fs/vfs.h"
static unsigned char bytes[FS_FILE_MAX], actual[FS_FILE_MAX];
int main(void) {
    fs_init(); struct fs_handles files = {0};
    assert(fs_mkdir("/a") == 0 && fs_mkdir("/a/b") == 0);
    assert(fs_rename("/a", "/a/b/c") == FS_EINVAL);
    assert(fs_unlink("/") == FS_EINVAL && fs_unlink("/a") == FS_ENOTEMPTY);
    long fd = fs_open(&files, "/a/b/../file", FS_READ | FS_WRITE | FS_CREATE);
    assert(fd == 3 && fs_open(&files, "/a/file/x", FS_WRITE | FS_CREATE) == FS_ENOTDIR);
    assert(fs_open(&files, "/a/file/..", FS_READ) == FS_ENOTDIR);
    memset(bytes, 0x5a, sizeof(bytes));
    assert(fs_write(&files, 3, bytes, sizeof(bytes)) == FS_FILE_MAX);
    assert(fs_write(&files, 3, bytes, 1) == FS_ENOSPC);
    assert(fs_seek(&files, 3, 0) == 0 && fs_read(&files, 3, actual, sizeof(actual)) == FS_FILE_MAX);
    assert(memcmp(bytes, actual, sizeof(bytes)) == 0);
    assert(fs_rename("/a/file", "/a/b/renamed") == 0 && fs_unlink("/a/b/renamed") == 0);
    assert(fs_used_blocks() == 16 && fs_seek(&files, 3, 4095) == 4095 && fs_read(&files, 3, actual, 2) == 2);
    fs_close_all(&files); assert(fs_used_blocks() == 0);
    assert(fs_seed("/sparse", "", 0) == 0);
    fd = fs_open(&files, "/sparse", FS_READ | FS_WRITE); assert(fd == 3);
    assert(fs_seek(&files, 3, 8193) == 8193 && fs_write(&files, 3, "X", 1) == 1);
    assert(fs_seek(&files, 3, 0) == 0 && fs_read(&files, 3, actual, 8194) == 8194);
    for (size_t i = 0; i < 8193; ++i) { assert(actual[i] == 0); } assert(actual[8193] == 'X');
    fs_close_all(&files); assert(fs_unlink("/sparse") == 0);
    for (unsigned int i = 0; i < FS_FD_MAX; ++i) { assert(fs_open(&files, "/fds", FS_WRITE | FS_CREATE) == (long)(3 + i)); }
    assert(fs_open(&files, "/not-created", FS_CREATE | FS_WRITE) == FS_EMFILE);
    struct fs_info info; assert(fs_stat("/not-created", &info) == FS_ENOENT); fs_close_all(&files);
    for (unsigned int i = 0; i < 8; ++i) { char path[] = "/quota0"; path[6] = (char)('0' + i); assert(fs_seed(path, bytes, sizeof(bytes)) == 0); }
    assert(fs_used_blocks() == FS_BLOCK_COUNT);
    fd = fs_open(&files, "/overflow", FS_CREATE | FS_WRITE); assert(fd == 3);
    assert(fs_write(&files, 3, bytes, 4097) == FS_ENOSPC && fs_stat("/overflow", &info) == 0 && info.size == 0);
    fs_close_all(&files);
    for (unsigned int i = 0; i < 8; ++i) { char path[] = "/quota0"; path[6] = (char)('0' + i); assert(fs_unlink(path) == 0); }
    assert(fs_used_blocks() == 0);
    assert(fs_open(&files, "/../bad", FS_CREATE | FS_WRITE) == FS_EINVAL);
    char long_path[FS_PATH_MAX + 1]; memset(long_path, 'a', sizeof(long_path)); long_path[0] = '/'; long_path[FS_PATH_MAX] = 0;
    assert(fs_open(&files, long_path, FS_READ) == FS_ENAMETOOLONG);
    puts("PASS: RAM VFS hierarchy, cross-block/sparse data, descriptor exhaustion, atomic quota failure, rename cycles, open-unlink and cleanup");
    return 0;
}
