#include "inference.h"
#include "user/user.h"
#include "scheduler/task.h"
#include "memory/init.h"
#include "debug/panic.h"
#include "debug/log.h"
#include "arch/x86_64/serial.h"
#include "net/stack.h"
extern unsigned char user_agentinfer_start[],user_agentinfer_end[];
void agent_inference_boot(void) {
    KASSERT(fs_seed_program("/bin/agentinfer",user_agentinfer_start,(size_t)(user_agentinfer_end-user_agentinfer_start))==0);
    uint64_t frames=memory_frames()->free_frames; size_t blocks=fs_used_blocks();
#if CONFIG_SELFTEST == 26
    const uint64_t cases=2;
#else
    const uint64_t cases=1;
#endif
    for(uint64_t mode=0;mode<cases;++mode) {
        long pid=user_spawn_path("/bin/agentinfer",mode,1,false); KASSERT(pid>0); struct user_process *process=user_find((uint64_t)pid); process->cpu_budget=128; process->syscall_budget=12000;
        scheduler_run(true,2,2500); int status; KASSERT(user_process_reap((uint64_t)pid,&status)); klog(LOG_INFO,"agent-inference","case=%u status=%d",(unsigned int)mode,status); KASSERT(status==0);
        KASSERT(memory_frames()->free_frames==frames && fs_used_blocks()==blocks);
    }
    long exit_pid=user_spawn_path("/bin/agentinfer",2,1,false); KASSERT(exit_pid>0); scheduler_run(true,2,100); int exit_status; KASSERT(user_process_reap((uint64_t)exit_pid,&exit_status) && exit_status==-123);
    KASSERT(memory_frames()->free_frames==frames && fs_used_blocks()==blocks);
#if CONFIG_SELFTEST == 26
    long fd[NET_SOCKET_MAX]; for(size_t i=0;i<NET_SOCKET_MAX;++i) { fd[i]=net_socket(1,NET_TCP); KASSERT(fd[i]>=0); } KASSERT(net_socket(1,NET_TCP)==-24); for(size_t i=0;i<NET_SOCKET_MAX;++i) { KASSERT(net_close(1,(uint64_t)fd[i])==0); }
    serial_write("AGENTICOS: MODEL BRIDGE PASS\n");
#endif
    serial_write("AGENTICOS: AGENT4 PASS\n");
}
