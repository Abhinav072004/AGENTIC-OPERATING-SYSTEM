#include "syscall.h"
#include "agent/runtime.h"
#include "runtime/memory.h"
#include "fs/vfs.h"
int user_main(uint64_t pid, uint64_t mode);
#define CHECK(t) do { if(!(t)) { return __LINE__; } } while(0)
static long begin(const char *text) { struct agent_request request={0}; size_t n=0; while(text[n]!=0 && n+1<sizeof(request.intent)) { request.intent[n]=text[n]; ++n; } return user_call(SYS_AGENT_START,(uintptr_t)&request,0,0); }
static long status(uint64_t id, struct agent_job_info *info) { return user_call(SYS_AGENT_STATUS,id,(uintptr_t)info,0); }
static long wait_state(uint64_t id, uint32_t state, struct agent_job_info *info) { for(unsigned int i=0;i<500;++i) { if(status(id,info)<0) { return -1; } if(info->state==state || info->state>=AGENT_DONE) { return 0; } (void)user_call(SYS_SLEEP,1,0,0); } return -110; }
static long release(uint64_t id) { long result; do { (void)user_call(SYS_SLEEP,1,0,0); result=user_call(SYS_AGENT_RELEASE,id,0,0); } while(result==SYS_EAGAIN); return result; }
int user_main(uint64_t pid, uint64_t mode) {
    (void)pid; struct agent_job_info info; struct agent_plan plan; struct fs_info file;
    CHECK(user_call(SYS_AGENT_START,UINT64_MAX,0,0)==SYS_EFAULT);
    long id=begin(mode==1 ? "write /home/agent-cancel 'one'; write /home/agent-never 'two'" : "write /home/agent-check 'native worker'; read /home/agent-check"); CHECK(id>0);
    CHECK(user_call(SYS_AGENT_CONTEXT,(uint64_t)id,(uintptr_t)&plan,0)==SYS_EPERM);
    CHECK(user_call(SYS_AGENT_PROPOSE,(uint64_t)id,(uintptr_t)&plan,0)==SYS_EPERM);
    CHECK(wait_state((uint64_t)id,AGENT_REVIEW,&info)==0 && info.state==AGENT_REVIEW);
    if(mode==4) { return 0; } /* Owner death during review cleans up the worker. */
    if(mode==5) { long second=begin("list /home"); CHECK(second>0 && begin("list /home")==-24); CHECK(user_call(SYS_AGENT_CANCEL,(uint64_t)second,0,0)==0 && release((uint64_t)second)==0); CHECK(user_call(SYS_AGENT_CANCEL,(uint64_t)id,0,0)==0 && release((uint64_t)id)==0); return 0; }
    CHECK(user_call(SYS_AGENT_REVIEW,(uint64_t)id,UINT64_MAX,0)==SYS_EFAULT);
    CHECK(user_call(SYS_AGENT_REVIEW,(uint64_t)id,(uintptr_t)&plan,0)==0 && agent_validate_plan(&plan)==0);
    CHECK(user_call(SYS_STAT,(uintptr_t)"/home/agent-check",(uintptr_t)&file,0)==FS_ENOENT);
    if(mode==2) { CHECK(user_call(SYS_AGENT_CANCEL,(uint64_t)id,0,0)==0); CHECK(status((uint64_t)id,&info)>0 && info.state==AGENT_CANCELLED && info.completed==0); CHECK(release((uint64_t)id)==0); return 0; }
    if(mode==3) { CHECK(wait_state((uint64_t)id,AGENT_FAILED,&info)==0 && info.state==AGENT_FAILED && info.error==-110); CHECK(release((uint64_t)id)==0); return 0; }
    CHECK(user_call(SYS_AGENT_APPROVE,(uint64_t)id,0,0)==SYS_EPERM);
    CHECK(user_call(SYS_AGENT_APPROVE,(uint64_t)id,AGENT_APPROVE_PLAN,0)==0);
    if(mode==1) { for(unsigned int n=0;n<200;++n) { CHECK(status((uint64_t)id,&info)>0); if(info.completed!=0) { break; } (void)user_call(SYS_SLEEP,1,0,0); } CHECK(info.completed==1); CHECK(user_call(SYS_AGENT_CANCEL,(uint64_t)id,0,0)==0); CHECK(user_call(SYS_STAT,(uintptr_t)"/home/agent-never",(uintptr_t)&file,0)==FS_ENOENT); CHECK(user_call(SYS_UNLINK,(uintptr_t)"/home/agent-cancel",0,0)==0); }
    else { CHECK(wait_state((uint64_t)id,AGENT_DONE,&info)==0 && info.state==AGENT_DONE && info.completed==2 && memcmp(info.result,"native worker",14)==0); CHECK(user_call(SYS_UNLINK,(uintptr_t)"/home/agent-check",0,0)==0); }
    struct agent_audit event; CHECK(user_call(SYS_AGENT_AUDIT,(uint64_t)id,(uintptr_t)&event,0)==1 && event.owner==pid);
    CHECK(release((uint64_t)id)==0 && status((uint64_t)id,&info)==SYS_ESRCH);
    CHECK(user_call(SYS_AGENT_STATUS,0,(uintptr_t)&info,0)==0 && user_call(SYS_AGENT_STATUS,0,(uintptr_t)&info,1)==0);
    return 0;
}
