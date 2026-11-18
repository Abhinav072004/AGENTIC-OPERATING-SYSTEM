#ifndef AGENTICOS_AGENT_BROKER_H
#define AGENTICOS_AGENT_BROKER_H
#include "plan.h"
#include <stdbool.h>
#define AGENT_JOB_MAX 2u
#define AGENT_AUDIT_MAX 64u
#define AGENT_REVIEW 1u
#define AGENT_APPROVED 2u
#define AGENT_RUNNING 3u
#define AGENT_DONE 4u
#define AGENT_CANCELLED 5u
#define AGENT_FAILED 6u
#define AGENT_APPROVE_PLAN 1u
#define AGENT_APPROVE_DELETE 2u
#define AUDIT_SUBMITTED 1u
#define AUDIT_APPROVED 2u
#define AUDIT_STEP 3u
#define AUDIT_DONE 4u
#define AUDIT_CANCELLED 5u
#define AUDIT_FAILED 6u
#define AUDIT_RELEASED 7u
struct agent_actor { uint64_t pid, capabilities; uint32_t uid; };
struct agent_job_info { uint64_t id, owner; uint32_t uid, state, completed, total; int32_t error; uint32_t bytes, destructive; char result[128]; };
struct agent_audit { uint64_t sequence, job, owner; uint32_t uid, event, tool; int32_t status; char path[128], other[128]; };
struct agent_job { struct agent_job_info info; struct agent_plan plan; bool live; };
struct agent_broker_ops { void *context; long (*execute)(void *context, const struct agent_actor *actor, const struct agent_action *action, char output[128], uint32_t *bytes); };
/* Trusted kernel callers supply actor identity; user interfaces derive it from
 * the current PCB, never from copied plan data. Calls are serialized and do not
 * sleep. User worker authentication is a separate runtime boundary. */
struct agent_broker { struct agent_job jobs[AGENT_JOB_MAX]; struct agent_audit audit[AGENT_AUDIT_MAX]; struct agent_broker_ops ops; uint64_t next_id, next_sequence, dropped; bool ready; };
bool agent_broker_init(struct agent_broker *broker, const struct agent_broker_ops *ops);
long agent_broker_begin(struct agent_broker *broker, const struct agent_actor *actor);
long agent_broker_propose(struct agent_broker *broker, const struct agent_actor *actor, uint64_t id, const struct agent_plan *plan);
long agent_broker_fail(struct agent_broker *broker, const struct agent_actor *actor, uint64_t id, int32_t error);
long agent_broker_submit(struct agent_broker *broker, const struct agent_actor *actor, const struct agent_plan *plan);
long agent_broker_review(struct agent_broker *broker, const struct agent_actor *actor, uint64_t id, struct agent_plan *plan, struct agent_job_info *info);
long agent_broker_approve(struct agent_broker *broker, const struct agent_actor *actor, uint64_t id, uint32_t consent);
long agent_broker_step(struct agent_broker *broker, const struct agent_actor *actor, uint64_t id);
long agent_broker_cancel(struct agent_broker *broker, const struct agent_actor *actor, uint64_t id);
long agent_broker_release(struct agent_broker *broker, const struct agent_actor *actor, uint64_t id);
long agent_broker_audit(struct agent_broker *broker, const struct agent_actor *actor, uint64_t id, uint64_t after, struct agent_audit *record);
struct agent_tools { void *context; long (*system)(void *context, char output[128], uint32_t *bytes); };
long agent_tools_execute(void *context, const struct agent_actor *actor, const struct agent_action *action, char output[128], uint32_t *bytes);
struct agent_broker *agent_broker_current(void);
void agent_broker_boot(void);
#endif
