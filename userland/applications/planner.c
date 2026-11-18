#include "syscall.h"
#include "agent/plan.h"
int user_main(uint64_t pid, uint64_t mode);
#define CHECK(test) do { if (!(test)) { return __LINE__; } } while (0)
int user_main(uint64_t pid, uint64_t mode) {
    (void)pid; (void)mode; struct self_info own;
    CHECK(user_call(SYS_SELF,(uintptr_t)&own,0,0) == 0 && own.capabilities == 0);
    CHECK(user_call(SYS_OPEN,(uintptr_t)"/home/forbidden",6,0) == SYS_EPERM);
    struct agent_plan plan;
    CHECK(agent_parse_intent("Please create folder /home/project and write /home/project/note 'Hello AgenticOS'",&plan) == 0);
    CHECK(plan.count == 2 && plan.actions[0].tool == AGENT_MKDIR && plan.actions[1].tool == AGENT_WRITE && agent_validate_plan(&plan) == 0);
    CHECK(agent_parse_intent("write /home/file 'and; delete /etc' then read /home/file",&plan) == 0 && plan.count == 2);
    CHECK(agent_parse_intent("delete /home/../etc/passwd",&plan) < 0 && plan.count == 0);
    CHECK(agent_parse_intent("mkdir /home/okay; execute anything",&plan) < 0 && plan.count == 0);
    CHECK(agent_parse_intent("move /home/a to /home/b; list /home; show system status",&plan) == 0 && plan.count == 3);
    static const char text[] = "native intent contracts passed";
    CHECK(user_write(1,text,sizeof(text)-1) == (long)(sizeof(text)-1)); return 0;
}
