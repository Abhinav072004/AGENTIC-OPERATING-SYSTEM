#include "vfs.h"
#include "user/user.h"
#include "scheduler/task.h"
#include "memory/init.h"
#include "debug/panic.h"
#include "debug/log.h"
#include "arch/x86_64/serial.h"
extern unsigned char user_hello_start[], user_hello_end[], user_probe_start[], user_probe_end[], user_fstest_start[], user_fstest_end[];
void fs_self_test(void) {
    fs_init();
    KASSERT(fs_mkdir("/bin") == 0 && fs_mkdir("/tmp") == 0 && fs_mkdir("/home") == 0 && fs_mkdir("/etc") == 0);
    KASSERT(fs_chown("/home", 1000) == 0 && fs_chmod("/home", 0700) == 0 && fs_chmod("/tmp", 01777) == 0 && fs_chmod("/bin", 0755) == 0 && fs_chmod("/etc", 0755) == 0);
    KASSERT(fs_seed("/etc/motd", "Welcome to AgenticOS\n", 20) == 0);
    KASSERT(fs_seed_program("/bin/hello", user_hello_start, (size_t)(user_hello_end - user_hello_start)) == 0);
    KASSERT(fs_seed_program("/bin/probe", user_probe_start, (size_t)(user_probe_end - user_probe_start)) == 0);
    size_t blocks = fs_used_blocks(); uint64_t frames = memory_frames()->free_frames;
    /* Load the native acceptance executable through the VFS itself. */
    KASSERT(fs_seed_program("/bin/fstest", user_fstest_start, (size_t)(user_fstest_end - user_fstest_start)) == 0);
    struct fs_handles files = {0};
    long fd = fs_open(&files, "/bin/fstest", FS_READ); KASSERT(fd >= 3);
    size_t length = (size_t)(user_fstest_end - user_fstest_start);
    void *image = kmalloc(length); KASSERT(image != NULL);
    KASSERT(fs_read(&files, (uint64_t)fd, image, length) == (long)length && fs_close(&files, (uint64_t)fd) == 0);
    for (uint64_t mode = 0; mode < 2; ++mode) {
        uint64_t pid, tid; int status;
        KASSERT(user_process_start(image, length, "filesystem", mode, false, &pid, &tid));
        scheduler_run(true, 2, 500);
        KASSERT(user_find(pid)->calls[SYS_OPEN] > 0 && user_process_reap(pid, &status));
        KASSERT(status == (mode == 0 ? 0 : USER_FAULT_STATUS(6)));
    }
    kfree(image);
    KASSERT(fs_unlink("/bin/fstest") == 0 && fs_used_blocks() == blocks && memory_frames()->free_frames == frames);
    klog(LOG_INFO, "fs-test", "native VFS operations, checked pointers, open-unlink and fault cleanup passed; quota=%u blocks, used=%u", FS_BLOCK_COUNT, (unsigned int)blocks);
    serial_write("AGENTICOS: FS USER PASS\n");
}
