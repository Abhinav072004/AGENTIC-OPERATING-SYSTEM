#include "user.h"
#include "fs/vfs.h"
#include "scheduler/task.h"
#include "memory/init.h"
#include "debug/panic.h"
#include "arch/x86_64/serial.h"
extern unsigned char user_shell_start[], user_shell_end[];
void shell_boot(void);
void shell_boot(void) {
    KASSERT(fs_seed("/bin/shell", user_shell_start, (size_t)(user_shell_end - user_shell_start)) == 0);
    uint64_t frames = memory_frames()->free_frames;
    long pid = user_spawn_path("/bin/shell", CONFIG_SELFTEST == 23 ? 0 : 1, 1, CONFIG_SELFTEST == 23);
    KASSERT(pid > 0);
    scheduler_run(true, 2, CONFIG_SELFTEST == 23 ? UINT64_MAX : 1000);
    int status; KASSERT(user_process_reap((uint64_t)pid, &status) && status == 0);
    KASSERT(memory_frames()->free_frames == frames);
    serial_write("AGENTICOS: SHELL USER PASS\n");
    serial_write("AGENTICOS: PHASE9 PASS\n");
}
