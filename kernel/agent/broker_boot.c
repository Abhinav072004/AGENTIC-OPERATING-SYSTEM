#include "broker.h"
#include "fs/vfs.h"
#include "security/policy.h"
#include "memory/init.h"
#include "debug/format.h"
#include "debug/panic.h"
#include "arch/x86_64/serial.h"
#include "runtime/memory.h"
static struct agent_broker broker;
static long system_summary(void *context, char output[128], uint32_t *bytes) {
    (void)context; int result = ksnprintf(output,128,"Free frames: %llu; RAM filesystem blocks: %u",(unsigned long long)memory_frames()->free_frames,(unsigned int)fs_used_blocks());
    *bytes = result > 0 ? (uint32_t)result : 0; return result < 0 ? -5 : 0;
}
static struct agent_tools tools = {.system = system_summary};
struct agent_broker *agent_broker_current(void) { return &broker; }
void agent_broker_boot(void) {
    struct agent_broker_ops ops = {.context = &tools,.execute = agent_tools_execute}; KASSERT(agent_broker_init(&broker,&ops));
    const struct agent_actor actor = {.pid = 1,.uid = 1000,.capabilities = CAP_AGENT_CONTROL|CAP_FS_READ|CAP_FS_WRITE|CAP_INSPECT};
    size_t blocks = fs_used_blocks(); struct agent_plan plan; struct fs_info file;
    KASSERT(agent_parse_intent("mkdir /home/broker-check; write /home/broker-check/note 'approved literal'; read /home/broker-check/note",&plan) == 0);
    long id = agent_broker_submit(&broker,&actor,&plan); KASSERT(id > 0);
    KASSERT(agent_broker_step(&broker,&actor,(uint64_t)id) == FS_EPERM && fs_stat("/home/broker-check",&file) == FS_ENOENT);
    plan.actions[0].tool = AGENT_DELETE; /* The submitted copy stays immutable. */
    KASSERT(agent_broker_approve(&broker,&actor,(uint64_t)id,AGENT_APPROVE_PLAN) == 0);
    KASSERT(agent_broker_step(&broker,&actor,(uint64_t)id) == 1 && agent_broker_step(&broker,&actor,(uint64_t)id) == 1 && agent_broker_step(&broker,&actor,(uint64_t)id) == 0);
    struct agent_job_info info; KASSERT(agent_broker_review(&broker,&actor,(uint64_t)id,NULL,&info) == 0 && info.completed == 3 && memcmp(info.result,"approved literal",17) == 0);
    KASSERT(agent_broker_release(&broker,&actor,(uint64_t)id) == 0);
    KASSERT(agent_parse_intent("delete /home/broker-check/note; delete /home/broker-check",&plan) == 0);
    id = agent_broker_submit(&broker,&actor,&plan); KASSERT(id > 0);
    KASSERT(agent_broker_approve(&broker,&actor,(uint64_t)id,AGENT_APPROVE_PLAN) == FS_EPERM);
    KASSERT(agent_broker_approve(&broker,&actor,(uint64_t)id,AGENT_APPROVE_PLAN|AGENT_APPROVE_DELETE) == 0);
    KASSERT(agent_broker_step(&broker,&actor,(uint64_t)id) == 1 && agent_broker_step(&broker,&actor,(uint64_t)id) == 0);
    KASSERT(fs_used_blocks() == blocks && fs_stat("/home/broker-check",&file) == FS_ENOENT);
    struct agent_audit record; KASSERT(agent_broker_audit(&broker,&actor,0,0,&record) == 1 && record.event == AUDIT_SUBMITTED);
    KASSERT(agent_broker_init(&broker,&ops)); /* Acceptance fixtures are not runtime jobs. */
    serial_write("AGENTICOS: AGENT2 PASS\n");
}
