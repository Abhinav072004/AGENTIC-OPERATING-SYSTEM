#include "runtime.h"
#include "transport.h"
#include "net/stack.h"
#include "user/user.h"
#include "user/copy.h"
#include "scheduler/task.h"
#include "security/policy.h"
#include "interrupts/interrupts.h"
#include "runtime/memory.h"
struct runtime_job { uint64_t id, owner, worker, deadline; struct agent_request request; struct agent_transport transport; };
static struct runtime_job jobs[AGENT_JOB_MAX];
static bool polling;
static uint64_t timeout_ticks=6000;
static struct agent_actor identity(const struct user_process *process) { return (struct agent_actor){.pid=process->pid,.uid=process->uid,.capabilities=process->capabilities}; }
static struct runtime_job *find_job(uint64_t id) { for(size_t i=0;i<AGENT_JOB_MAX;++i) { if(jobs[i].id==id && id!=0) { return &jobs[i]; } } return NULL; }
static bool terminal(uint32_t state) { return state>=AGENT_DONE; }
static bool information(const struct runtime_job *job, struct agent_actor *actor, struct agent_job_info *info) {
    struct user_process *owner=user_find(job->owner); if(owner==NULL) { return false; } *actor=identity(owner);
    return agent_broker_review(agent_broker_current(),actor,job->id,NULL,info)==0;
}
void agent_process_reaped(uint64_t pid, int status) {
    if(!agent_broker_current()->ready) { return; }
    for(size_t i=0;i<AGENT_JOB_MAX;++i) { struct runtime_job *job=&jobs[i]; if(job->id==0) { continue; }
        struct agent_actor actor; struct agent_job_info info;
        if(job->worker==pid) { agent_transport_close(&job->transport); job->worker=0; if(information(job,&actor,&info) && !terminal(info.state)) { (void)agent_broker_fail(agent_broker_current(),&actor,job->id,status<0 ? status : -5); } }
        if(job->owner==pid) {
            /* Children are reaped first by the process manager. No saved user
             * pointers or worker roots survive the owner's process lifetime. */
            if(information(job,&actor,&info)) { if(!terminal(info.state)) { (void)agent_broker_cancel(agent_broker_current(),&actor,job->id); } (void)agent_broker_release(agent_broker_current(),&actor,job->id); }
            agent_transport_close(&job->transport); memset(job,0,sizeof(*job));
        }
    }
}
void agent_poll(void) {
    if(polling || !agent_broker_current()->ready) { return; } polling=true;
    for(size_t i=0;i<AGENT_JOB_MAX;++i) { struct runtime_job *job=&jobs[i]; if(job->id==0) { continue; }
        struct agent_actor actor; struct agent_job_info info; bool owned=information(job,&actor,&info);
        if(owned && !terminal(info.state) && interrupts_ticks()>=job->deadline) { (void)agent_broker_fail(agent_broker_current(),&actor,job->id,-110); info.state=AGENT_FAILED; }
        struct user_process *worker=user_find(job->worker); struct task_control task;
        if(worker==NULL || worker->pid==task_pid() || !task_get_info(worker->tid,&task)) { continue; }
        if((!owned || terminal(info.state)) && task.state!=TASK_ZOMBIE) { agent_transport_close(&job->transport); (void)task_cancel(worker->tid); (void)task_get_info(worker->tid,&task); }
        if(task.state==TASK_ZOMBIE) { (void)user_process_reap(worker->pid,NULL); }
    }
    polling=false;
}
static long start(struct user_process *process, uint64_t address) {
    struct agent_request request;
    if(!copy_from_user(&process->space.vm,&request,address,sizeof(request))) { return SYS_EFAULT; }
    if(request.reserved!=0 || request.backend>AGENT_BACKEND_MODEL) { return SYS_EINVAL; }
    if(request.backend==AGENT_BACKEND_MODEL && (process->capabilities & CAP_NETWORK)==0) { return SYS_EPERM; }
    size_t length=0; while(length<sizeof(request.intent) && request.intent[length]!=0) { unsigned char c=(unsigned char)request.intent[length++]; if(c<32 || c>126) { return SYS_EINVAL; } }
    if(length==0 || length==sizeof(request.intent)) { return SYS_E2BIG; }
    for(size_t i=length+1;i<sizeof(request.intent);++i) { if(request.intent[i]!=0) { return SYS_EINVAL; } }
    struct runtime_job *slot=NULL; for(size_t i=0;i<AGENT_JOB_MAX;++i) { if(jobs[i].id==0) { slot=&jobs[i]; break; } } if(slot==NULL) { return -24; }
    struct agent_actor actor=identity(process); long id=agent_broker_begin(agent_broker_current(),&actor); if(id<0) { return id; }
    long pid=user_spawn_path("/bin/agent",(uint64_t)id,process->pid,false);
    if(pid<0) { (void)agent_broker_fail(agent_broker_current(),&actor,(uint64_t)id,(int32_t)pid); (void)agent_broker_release(agent_broker_current(),&actor,(uint64_t)id); return pid; }
    struct user_process *worker=user_find((uint64_t)pid); worker->capabilities=0; worker->cpu_budget=64; worker->syscall_budget=2048;
    *slot=(struct runtime_job){.id=(uint64_t)id,.owner=process->pid,.worker=(uint64_t)pid,.deadline=interrupts_ticks()+timeout_ticks,.request=request}; agent_transport_init(&slot->transport,(uint64_t)pid,request.intent); return id;
}
long agent_syscall(struct user_process *process, uint64_t number, uint64_t a, uint64_t b, uint64_t c) {
    if(number==SYS_AGENT_START) { return start(process,a); }
    struct runtime_job *job=find_job(a); struct agent_actor actor; struct agent_job_info info;
    if(number==SYS_AGENT_STATUS && a==0) { if(c>=AGENT_JOB_MAX) { return SYS_EINVAL; } job=&jobs[c]; if(job->id==0 || job->owner!=process->pid) { return 0; } a=job->id; }
    if(number==SYS_AGENT_AUDIT) { struct agent_audit entry; actor=identity(process); long result=agent_broker_audit(agent_broker_current(),&actor,a,c,&entry); if(result<=0) { return result; } return copy_to_user(&process->space.vm,b,&entry,sizeof(entry)) ? 1 : SYS_EFAULT; }
    if(job==NULL || job->id==0) { return SYS_ESRCH; }
    if(!information(job,&actor,&info)) { return SYS_EPERM; }
    if(number>=SYS_AGENT_CONTEXT && number<=SYS_AGENT_INFER) {
        if(job->worker!=process->pid) { return SYS_EPERM; }
        if(number==SYS_AGENT_CONTEXT) { return copy_to_user(&process->space.vm,b,&job->request,sizeof(job->request)) ? 0 : SYS_EFAULT; }
        if(number==SYS_AGENT_PROPOSE) { struct agent_plan plan; if(!copy_from_user(&process->space.vm,&plan,b,sizeof(plan))) { return SYS_EFAULT; } return agent_broker_propose(agent_broker_current(),&actor,a,&plan); }
        if(number==SYS_AGENT_INFER) {
            if(job->request.backend!=AGENT_BACKEND_MODEL || info.state!=AGENT_PLANNING || (actor.capabilities & CAP_NETWORK)==0) { return SYS_EPERM; }
            if(!user_buffer_valid(&process->space.vm,b,sizeof(struct agent_plan),true)) { return SYS_EFAULT; }
            struct agent_plan plan;
#if CONFIG_SELFTEST == 26
            uint64_t deadline=interrupts_ticks()+100;
#else
            uint64_t deadline=interrupts_ticks()+2000;
#endif
            long result;
            do {
                network_poll(); result=agent_transport_poll(&job->transport,&plan);
                if(result!=NET_EAGAIN) { break; }
                (void)task_sleep(1);
                if(!information(job,&actor,&info) || terminal(info.state) || (actor.capabilities & CAP_NETWORK)==0) { result=SYS_EPERM; break; }
            } while(interrupts_ticks()<deadline && interrupts_ticks()<job->deadline);
            if(result==NET_EAGAIN) { result=-110; } agent_transport_close(&job->transport);
            if(result<0) { return result; } return copy_to_user(&process->space.vm,b,&plan,sizeof(plan)) ? 0 : SYS_EFAULT;
        }
        if(number==SYS_AGENT_WAIT) {
            while(info.state==AGENT_REVIEW && interrupts_ticks()<job->deadline) { (void)task_sleep(10); agent_poll(); if(!information(job,&actor,&info)) { return SYS_EPERM; } }
            if(interrupts_ticks()>=job->deadline && !terminal(info.state)) { (void)agent_broker_fail(agent_broker_current(),&actor,a,-110); return -110; }
            return info.state==AGENT_APPROVED ? 0 : -125;
        }
        if(number==SYS_AGENT_STEP) { (void)task_sleep(10); agent_poll(); if(!information(job,&actor,&info)) { return SYS_EPERM; } return agent_broker_step(agent_broker_current(),&actor,a); }
        return SYS_ENOSYS;
    }
    if(job->owner!=process->pid) { return SYS_EPERM; }
    if(number==SYS_AGENT_REVIEW) {
        if(!user_buffer_valid(&process->space.vm,b,sizeof(struct agent_plan),true)) { return SYS_EFAULT; }
        struct agent_plan plan; long result=agent_broker_review(agent_broker_current(),&actor,a,&plan,NULL); if(result<0) { return result; }
        return copy_to_user(&process->space.vm,b,&plan,sizeof(plan)) ? 0 : SYS_EFAULT;
    }
    if(number==SYS_AGENT_STATUS) { return copy_to_user(&process->space.vm,b,&info,sizeof(info)) ? 1 : SYS_EFAULT; }
    if(number==SYS_AGENT_APPROVE) { if(b>UINT32_MAX) { return SYS_EINVAL; } return agent_broker_approve(agent_broker_current(),&actor,a,(uint32_t)b); }
    if(number==SYS_AGENT_CANCEL) { long result=agent_broker_cancel(agent_broker_current(),&actor,a); agent_poll(); return result; }
    if(number==SYS_AGENT_RELEASE) { if(job->worker!=0) { return SYS_EAGAIN; } long result=agent_broker_release(agent_broker_current(),&actor,a); if(result==0) { agent_transport_close(&job->transport); memset(job,0,sizeof(*job)); } return result; }
    return SYS_ENOSYS;
}
/* Acceptance boot adjusts only the deadline, never the worker's authority. */
void agent_runtime_test_timeout(uint64_t ticks);
void agent_runtime_test_timeout(uint64_t ticks) { timeout_ticks=ticks; }
