#include "syscall.h"
#include "agent/runtime.h"
int user_main(uint64_t pid, uint64_t id);
int user_main(uint64_t pid, uint64_t id) {
    (void)pid; struct self_info own; struct agent_request request; struct agent_plan plan;
    if(user_call(SYS_SELF,(uintptr_t)&own,0,0)!=0 || own.capabilities!=0) { return 1; }
    if(user_call(SYS_AGENT_APPROVE,id,AGENT_APPROVE_PLAN,0)!=SYS_EPERM || user_call(SYS_OPEN,(uintptr_t)"/home/escape",6,0)!=SYS_EPERM || user_call(SYS_NET_SOCKET,1,0,0)!=SYS_EPERM) { return 2; }
    long result=user_call(SYS_AGENT_CONTEXT,id,(uintptr_t)&request,0); if(result<0) { return (int)result; }
    if(user_call(SYS_AGENT_PROPOSE,id,UINT64_MAX,0)!=SYS_EFAULT) { return 3; }
    struct agent_plan forged={.abi=AGENT_PLAN_ABI,.count=1,.actions={{.tool=AGENT_WRITE,.path="/etc/escape",.text="forged"}}};
    if(user_call(SYS_AGENT_PROPOSE,id,(uintptr_t)&forged,0)>=0) { return 4; }
    result=agent_parse_intent(request.intent,&plan); if(result<0) { return (int)result; }
    result=user_call(SYS_AGENT_PROPOSE,id,(uintptr_t)&plan,0); if(result<0) { return (int)result; }
    result=user_call(SYS_AGENT_WAIT,id,0,0); if(result<0) { return (int)result; }
    do { result=user_call(SYS_AGENT_STEP,id,0,0); } while(result>0);
    return result<0 ? (int)result : 0;
}
