#include "syscall.h"
#include "agent/runtime.h"
#include "agent/inference.h"
#include "runtime/memory.h"
#include "fs/vfs.h"
int user_main(uint64_t pid, uint64_t mode);
#define CHECK(t) do { if(!(t)) { return __LINE__; } } while(0)
static long begin(const char *text, uint32_t backend) { struct agent_request request={.backend=backend}; size_t n=0; while(text[n]!=0 && n+1<sizeof(request.intent)) { request.intent[n]=text[n]; ++n; } return user_call(SYS_AGENT_START,(uintptr_t)&request,0,0); }
static long wait_for(uint64_t id, uint32_t state, struct agent_job_info *info) { for(unsigned int n=0;n<2500;++n) { if(user_call(SYS_AGENT_STATUS,id,(uintptr_t)info,0)<0) { return -3; } if(info->state==state || info->state>=AGENT_DONE) { return 0; } (void)user_call(SYS_SLEEP,1,0,0); } return -110; }
static long release(uint64_t id) { long result; do { (void)user_call(SYS_SLEEP,1,0,0); result=user_call(SYS_AGENT_RELEASE,id,0,0); } while(result==SYS_EAGAIN); return result; }
int user_main(uint64_t pid, uint64_t mode) {
    (void)pid; if(mode==2) { return -123; } struct agent_plan plan; struct agent_job_info info; struct fs_info file;
    if(mode==0) {
        CHECK(agent_infer_offline("please put 'offline inference' into /home/inference-check",&plan)==0 && plan.actions[0].tool==AGENT_WRITE);
        CHECK(agent_infer_offline("please erase /etc/passwd",&plan)<0 && plan.count==0);
        long id=begin("please put 'offline inference' into /home/inference-check",AGENT_BACKEND_OFFLINE); CHECK(id>0);
        CHECK(wait_for((uint64_t)id,AGENT_REVIEW,&info)==0 && info.state==AGENT_REVIEW);
        CHECK(user_call(SYS_STAT,(uintptr_t)"/home/inference-check",(uintptr_t)&file,0)==FS_ENOENT);
        CHECK(user_call(SYS_AGENT_APPROVE,(uint64_t)id,AGENT_APPROVE_PLAN,0)==0);
        CHECK(wait_for((uint64_t)id,AGENT_DONE,&info)==0 && info.state==AGENT_DONE);
        long fd=user_call(SYS_OPEN,(uintptr_t)"/home/inference-check",FS_READ,0); char text[32]={0}; CHECK(fd>=3 && user_read((uint64_t)fd,text,31)==17 && memcmp(text,"offline inference",18)==0); CHECK(user_call(SYS_CLOSE,(uint64_t)fd,0,0)==0 && user_call(SYS_UNLINK,(uintptr_t)"/home/inference-check",0,0)==0 && release((uint64_t)id)==0);
        id=begin("unsupported xyzzy /home/no-effect",AGENT_BACKEND_OFFLINE); CHECK(id>0 && wait_for((uint64_t)id,AGENT_FAILED,&info)==0 && info.state==AGENT_FAILED && info.error==-22 && info.completed==0 && release((uint64_t)id)==0);
        return 0;
    }
    static const char *const requests[]={"model good","model unsafe","model malformed","model error","wire magic","wire length","wire partial","wire timeout"};
    static const int32_t errors[]={0,-22,-22,-5,-71,-7,-5,-110};
    for(size_t i=0;i<sizeof(requests)/sizeof(requests[0]);++i) {
        long id=begin(requests[i],AGENT_BACKEND_MODEL); CHECK(id>0);
        if(i==0) {
            CHECK(wait_for((uint64_t)id,AGENT_REVIEW,&info)==0 && info.state==AGENT_REVIEW);
            CHECK(user_call(SYS_STAT,(uintptr_t)"/home/model-check",(uintptr_t)&file,0)==FS_ENOENT);
            CHECK(user_call(SYS_AGENT_REVIEW,(uint64_t)id,(uintptr_t)&plan,0)==0 && agent_validate_plan(&plan)==0 && plan.count==1 && plan.actions[0].tool==AGENT_WRITE && memcmp(plan.actions[0].text,"model fixture",14)==0);
            CHECK(user_call(SYS_AGENT_APPROVE,(uint64_t)id,AGENT_APPROVE_PLAN,0)==0 && wait_for((uint64_t)id,AGENT_DONE,&info)==0 && info.state==AGENT_DONE);
            CHECK(user_call(SYS_UNLINK,(uintptr_t)"/home/model-check",0,0)==0);
        } else {
            CHECK(wait_for((uint64_t)id,AGENT_FAILED,&info)==0 && info.state==AGENT_FAILED && info.error==errors[i] && info.completed==0);
            CHECK(user_call(SYS_STAT,(uintptr_t)"/home/model-check",(uintptr_t)&file,0)==FS_ENOENT);
        }
        CHECK(release((uint64_t)id)==0);
    }
    long id=begin("wire timeout",AGENT_BACKEND_MODEL); CHECK(id>0); (void)user_call(SYS_SLEEP,10,0,0); CHECK(user_call(SYS_AGENT_CANCEL,(uint64_t)id,0,0)==0 && release((uint64_t)id)==0);
    return 0;
}
