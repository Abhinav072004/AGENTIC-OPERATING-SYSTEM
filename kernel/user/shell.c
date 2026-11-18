#include "user.h"
#include "fs/vfs.h"
#include "scheduler/task.h"
#include "memory/init.h"
#include "debug/panic.h"
#include "arch/x86_64/serial.h"
extern unsigned char user_shell_start[], user_shell_end[];
void shell_boot(void);
void shell_session(void);
static void run_shell(bool interactive) {
    uint64_t frames = memory_frames()->free_frames;
    long pid = user_spawn_path("/bin/shell",interactive ? 0 : 1,1,interactive); KASSERT(pid > 0);
    scheduler_run(true,2,interactive ? UINT64_MAX : 1000);
    int status; KASSERT(user_process_reap((uint64_t)pid,&status) && status == 0);
    KASSERT(memory_frames()->free_frames == frames);
}
void shell_boot(void) {
    KASSERT(fs_seed_program("/bin/shell",user_shell_start,(size_t)(user_shell_end-user_shell_start)) == 0);
    run_shell(false);
    serial_write("AGENTICOS: SHELL USER PASS\n");
    serial_write("AGENTICOS: PHASE9 PASS\n");
}
void shell_session(void) { run_shell(true); serial_write("AGENTICOS: SESSION EXIT\n"); }
