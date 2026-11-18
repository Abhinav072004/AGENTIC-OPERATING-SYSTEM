#include "policy.h"
#include "user/user.h"
#include "scheduler/task.h"
#include "memory/init.h"
#include "debug/panic.h"
#include "debug/log.h"
#include "arch/x86_64/serial.h"
extern unsigned char user_security_start[], user_security_end[];
void security_self_test(void) {
    KASSERT(fs_seed_program("/bin/security",user_security_start,(size_t)(user_security_end-user_security_start)) == 0);
    KASSERT(fs_seed("/tmp/other","owned",5) == 0 && fs_chown("/tmp/other",1000) == 0);
    size_t blocks = fs_used_blocks(); uint64_t frames = memory_frames()->free_frames;
    for (uint64_t mode = 0; mode <= 5; ++mode) {
        uint64_t pid,tid; int status;
        KASSERT(user_process_start(user_security_start,(size_t)(user_security_end-user_security_start),"security",mode,false,&pid,&tid));
        struct user_process *process = user_find(pid);
        if (mode == 0) { process->capabilities = 0; }
        if (mode == 1) { process->uid = 1001; process->capabilities = CAP_FS_READ|CAP_FS_WRITE; }
        if (mode == 3) { process->syscall_budget = 100; }
        if (mode == 4) { process->cpu_budget = 8; }
        if (mode == 5) { process->display = true; process->capabilities |= CAP_DISPLAY; }
        scheduler_run(true,2,500);
        if (mode == 0) { KASSERT(process->rejected_calls >= 7); }
        KASSERT(user_process_reap(pid,&status));
        klog(LOG_INFO,"security-test","case=%u status=%d frames=%llu; capability/UID/lifecycle boundary checked",(unsigned int)mode,status,(unsigned long long)memory_frames()->free_frames);
        KASSERT(status == ((mode == 3 || mode == 4) ? USER_QUOTA_STATUS : 0));
        KASSERT(memory_frames()->free_frames == frames && fs_used_blocks() == blocks);
    }
    KASSERT(fs_unlink("/tmp/other") == 0);
    serial_write("AGENTICOS: SECURITY POLICY PASS\n");
    serial_write("AGENTICOS: PHASE12 PASS\n");
}
