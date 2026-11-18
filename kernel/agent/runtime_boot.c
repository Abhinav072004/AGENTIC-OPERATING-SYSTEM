#include "runtime.h"
#include "user/user.h"
#include "scheduler/task.h"
#include "memory/init.h"
#include "debug/panic.h"
#include "debug/log.h"
#include "arch/x86_64/serial.h"
extern unsigned char user_agent_start[],user_agent_end[],user_agenttest_start[],user_agenttest_end[];
void agent_runtime_test_timeout(uint64_t ticks);
void agent_runtime_boot(void) {
    KASSERT(fs_seed_program("/bin/agent",user_agent_start,(size_t)(user_agent_end-user_agent_start))==0);
    KASSERT(fs_seed_program("/bin/agenttest",user_agenttest_start,(size_t)(user_agenttest_end-user_agenttest_start))==0);
    uint64_t frames=memory_frames()->free_frames; size_t blocks=fs_used_blocks();
    for(uint64_t mode=0;mode<6;++mode) {
        agent_runtime_test_timeout(mode==3 ? 60 : 6000);
        long pid=user_spawn_path("/bin/agenttest",mode,1,false); KASSERT(pid>0);
        struct user_process *test=user_find((uint64_t)pid); test->cpu_budget=128; test->syscall_budget=4096;
        scheduler_run(true,2,1000); int status; KASSERT(user_process_reap((uint64_t)pid,&status));
        klog(LOG_INFO,"agent-test","case=%u status=%d",(unsigned int)mode,status); KASSERT(status==0);
        KASSERT(memory_frames()->free_frames==frames && fs_used_blocks()==blocks);
    }
    agent_runtime_test_timeout(6000); serial_write("AGENTICOS: AGENT3 PASS\n");
}
