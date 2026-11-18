#include "plan.h"
#include "fs/vfs.h"
#include "user/user.h"
#include "scheduler/task.h"
#include "memory/init.h"
#include "debug/panic.h"
#include "arch/x86_64/serial.h"
extern unsigned char user_planner_start[], user_planner_end[];
void agent_foundation_boot(void) {
    KASSERT(fs_seed_program("/bin/planner",user_planner_start,(size_t)(user_planner_end-user_planner_start)) == 0);
    uint64_t frames = memory_frames()->free_frames; size_t blocks = fs_used_blocks();
    long pid = user_spawn_path("/bin/planner",0,1,false); KASSERT(pid > 0);
    struct user_process *process = user_find((uint64_t)pid); process->capabilities = 0; process->cpu_budget = 32; process->syscall_budget = 64;
    scheduler_run(true,2,500); int status; KASSERT(user_process_reap((uint64_t)pid,&status) && status == 0);
    KASSERT(memory_frames()->free_frames == frames && fs_used_blocks() == blocks);
    serial_write("AGENTICOS: AGENT1 PASS\n");
}
