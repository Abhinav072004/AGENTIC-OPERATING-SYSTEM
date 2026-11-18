#include "broker.h"
#include "security/policy.h"
#include "runtime/memory.h"
#include "fs/vfs.h"
static bool controller(const struct agent_actor *actor) { return actor != NULL && actor->pid != 0 && (actor->capabilities & CAP_AGENT_CONTROL) != 0; }
static struct agent_job *lookup(struct agent_broker *broker, uint64_t id) { if (broker == NULL || !broker->ready) { return NULL; } for (size_t i = 0; i < AGENT_JOB_MAX; ++i) { if (broker->jobs[i].live && broker->jobs[i].info.id == id) { return &broker->jobs[i]; } } return NULL; }
static bool owner(const struct agent_job *job, const struct agent_actor *actor) { return controller(actor) && job->info.owner == actor->pid && job->info.uid == actor->uid; }
static bool room(const struct agent_broker *broker) { return broker->next_sequence != UINT64_MAX; }
static void record_event(struct agent_broker *broker, const struct agent_job *job, uint32_t event, int32_t status, const struct agent_action *action) {
    struct agent_audit *record = &broker->audit[(broker->next_sequence-1u)%AGENT_AUDIT_MAX];
    *record = (struct agent_audit){.sequence = broker->next_sequence++,.job = job->info.id,.owner = job->info.owner,.uid = job->info.uid,.event = event,.status = status};
    if (action != NULL) { record->tool = action->tool; memcpy(record->path,action->path,128); memcpy(record->other,action->other,128); }
    if (record->sequence > AGENT_AUDIT_MAX) { ++broker->dropped; }
}
bool agent_broker_init(struct agent_broker *broker, const struct agent_broker_ops *ops) {
    if (broker == NULL || ops == NULL || ops->execute == NULL) { return false; }
    memset(broker,0,sizeof(*broker)); broker->ops = *ops; broker->next_id = broker->next_sequence = 1; broker->ready = true; return true;
}
long agent_broker_submit(struct agent_broker *broker, const struct agent_actor *actor, const struct agent_plan *plan) {
    if (broker == NULL || !broker->ready) { return FS_EINVAL; } if (!controller(actor)) { return FS_EPERM; }
    long result = agent_validate_plan(plan); if (result < 0) { return result; }
    if (!room(broker) || broker->next_id > INT64_MAX) { return -75; }
    for (size_t i = 0; i < AGENT_JOB_MAX; ++i) {
        struct agent_job *job = &broker->jobs[i]; if (job->live) { continue; }
        memset(job,0,sizeof(*job)); job->live = true; job->plan = *plan;
        job->info = (struct agent_job_info){.id = broker->next_id++,.owner = actor->pid,.uid = actor->uid,.state = AGENT_REVIEW,.total = plan->count};
        for (uint32_t step = 0; step < plan->count; ++step) { if (plan->actions[step].tool == AGENT_DELETE) { job->info.destructive = 1; } }
        record_event(broker,job,AUDIT_SUBMITTED,0,NULL); return (long)job->info.id;
    }
    return -24;
}
long agent_broker_review(struct agent_broker *broker, const struct agent_actor *actor, uint64_t id, struct agent_plan *plan, struct agent_job_info *info) {
    struct agent_job *job = lookup(broker,id); if (job == NULL) { return -3; } if (!owner(job,actor)) { return FS_EPERM; }
    if (plan != NULL) { *plan = job->plan; } if (info != NULL) { *info = job->info; } return 0;
}
long agent_broker_approve(struct agent_broker *broker, const struct agent_actor *actor, uint64_t id, uint32_t consent) {
    struct agent_job *job = lookup(broker,id); if (job == NULL) { return -3; } if (!owner(job,actor)) { return FS_EPERM; }
    if (job->info.state != AGENT_REVIEW || (consent & ~(AGENT_APPROVE_PLAN|AGENT_APPROVE_DELETE)) != 0) { return FS_EINVAL; }
    uint32_t needed = AGENT_APPROVE_PLAN | (job->info.destructive != 0 ? AGENT_APPROVE_DELETE : 0u);
    uint64_t rights = agent_plan_permissions(&job->plan);
    if ((consent & needed) != needed || (actor->capabilities & rights) != rights) { return FS_EPERM; }
    if (!room(broker)) { return -75; } job->info.state = AGENT_APPROVED; record_event(broker,job,AUDIT_APPROVED,0,NULL); return 0;
}
long agent_broker_step(struct agent_broker *broker, const struct agent_actor *actor, uint64_t id) {
    struct agent_job *job = lookup(broker,id); if (job == NULL) { return -3; } if (!owner(job,actor)) { return FS_EPERM; }
    if (job->info.state != AGENT_APPROVED && job->info.state != AGENT_RUNNING) { return FS_EPERM; }
    if (!room(broker)) { return -75; }
    uint64_t rights = agent_plan_permissions(&job->plan);
    if ((actor->capabilities & rights) != rights) { job->info.state = AGENT_FAILED; job->info.error = (int32_t)FS_EPERM; record_event(broker,job,AUDIT_FAILED,(int32_t)FS_EPERM,NULL); return FS_EPERM; }
    const struct agent_action *action = &job->plan.actions[job->info.completed];
    memset(job->info.result,0,sizeof(job->info.result)); job->info.bytes = 0;
    long result = broker->ops.execute(broker->ops.context,actor,action,job->info.result,&job->info.bytes);
    if (result < 0) { job->info.state = AGENT_FAILED; job->info.error = (int32_t)result; record_event(broker,job,AUDIT_FAILED,(int32_t)result,action); return result; }
    ++job->info.completed; job->info.state = job->info.completed == job->plan.count ? AGENT_DONE : AGENT_RUNNING;
    record_event(broker,job,job->info.state == AGENT_DONE ? AUDIT_DONE : AUDIT_STEP,0,action);
    return job->info.state == AGENT_DONE ? 0 : 1;
}
long agent_broker_cancel(struct agent_broker *broker, const struct agent_actor *actor, uint64_t id) {
    struct agent_job *job = lookup(broker,id); if (job == NULL) { return -3; } if (!owner(job,actor)) { return FS_EPERM; }
    if (job->info.state == AGENT_DONE || job->info.state == AGENT_CANCELLED || job->info.state == AGENT_FAILED) { return FS_EINVAL; }
    if (!room(broker)) { return -75; } job->info.state = AGENT_CANCELLED; job->info.error = -125; record_event(broker,job,AUDIT_CANCELLED,-125,NULL); return 0;
}
long agent_broker_release(struct agent_broker *broker, const struct agent_actor *actor, uint64_t id) {
    struct agent_job *job = lookup(broker,id); if (job == NULL) { return -3; } if (!owner(job,actor)) { return FS_EPERM; }
    if (job->info.state != AGENT_DONE && job->info.state != AGENT_CANCELLED && job->info.state != AGENT_FAILED) { return -11; }
    if (!room(broker)) { return -75; } record_event(broker,job,AUDIT_RELEASED,0,NULL); memset(job,0,sizeof(*job)); return 0;
}
long agent_broker_audit(struct agent_broker *broker, const struct agent_actor *actor, uint64_t id, uint64_t after, struct agent_audit *record) {
    if (broker == NULL || !broker->ready || record == NULL) { return FS_EINVAL; } if (!controller(actor)) { return FS_EPERM; }
    uint64_t first = broker->next_sequence > AGENT_AUDIT_MAX ? broker->next_sequence-AGENT_AUDIT_MAX : 1;
    for (uint64_t sequence = first; sequence < broker->next_sequence; ++sequence) {
        const struct agent_audit *entry = &broker->audit[(sequence-1u)%AGENT_AUDIT_MAX];
        if (entry->sequence > after && entry->owner == actor->pid && entry->uid == actor->uid && (id == 0 || entry->job == id)) { *record = *entry; return 1; }
    }
    return 0;
}

/* Runtime reservation has no executable plan until a validated proposal arrives. */
long agent_broker_begin(struct agent_broker *broker, const struct agent_actor *actor) {
    if (broker == NULL || !broker->ready) { return FS_EINVAL; } if (!controller(actor)) { return FS_EPERM; }
    if (!room(broker) || broker->next_id > INT64_MAX) { return -75; }
    for (size_t i=0;i<AGENT_JOB_MAX;++i) { struct agent_job *job=&broker->jobs[i]; if (job->live) { continue; }
        memset(job,0,sizeof(*job)); job->live=true; job->info=(struct agent_job_info){.id=broker->next_id++,.owner=actor->pid,.uid=actor->uid};
        record_event(broker,job,8,0,NULL); return (long)job->info.id;
    } return -24;
}
long agent_broker_propose(struct agent_broker *broker, const struct agent_actor *actor, uint64_t id, const struct agent_plan *plan) {
    struct agent_job *job=lookup(broker,id); if (job==NULL) { return -3; } if (!owner(job,actor)) { return FS_EPERM; }
    if (job->info.state!=0) { return FS_EPERM; } long result=agent_validate_plan(plan); if(result<0) { return result; } if(!room(broker)) { return -75; }
    job->plan=*plan; job->info.total=plan->count; job->info.state=AGENT_REVIEW;
    for(uint32_t i=0;i<plan->count;++i) { if(plan->actions[i].tool==AGENT_DELETE) { job->info.destructive=1; } }
    record_event(broker,job,AUDIT_SUBMITTED,0,NULL); return 0;
}
long agent_broker_fail(struct agent_broker *broker, const struct agent_actor *actor, uint64_t id, int32_t error) {
    struct agent_job *job=lookup(broker,id); if(job==NULL) { return -3; } if(!owner(job,actor)) { return FS_EPERM; }
    if(job->info.state>=AGENT_DONE) { return 0; } if(!room(broker)) { return -75; }
    job->info.state=AGENT_FAILED; job->info.error=error; record_event(broker,job,AUDIT_FAILED,error,NULL); return 0;
}
