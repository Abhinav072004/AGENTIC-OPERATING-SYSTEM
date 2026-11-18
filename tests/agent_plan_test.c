#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "agent/plan.h"
#include "security/policy.h"
int main(void) {
    struct agent_plan plan;
    assert(agent_parse_intent("Please create folder /home/project and write /home/project/note 'hello world'; move /home/project/note to /home/project/README; read /home/project/README; list /home; system",&plan) == 0);
    assert(plan.count == 6 && plan.actions[1].tool == AGENT_WRITE && strcmp(plan.actions[1].text,"hello world") == 0);
    assert(agent_plan_permissions(&plan) == (CAP_FS_READ|CAP_FS_WRITE|CAP_INSPECT));
    assert(agent_parse_intent("write /home/a 'and; delete /etc'",&plan) == 0 && plan.count == 1 && strcmp(plan.actions[0].text,"and; delete /etc") == 0);
    assert(agent_parse_intent("write /home/a ''",&plan) == 0 && plan.actions[0].text[0] == 0);
    assert(agent_parse_intent("write /home/a \"hello\\nworld\"",&plan) == 0 && strcmp(plan.actions[0].text,"hello\nworld") == 0);
    const char *bad[] = {"", "execute /home/a", "delete /etc/passwd", "delete /home", "mkdir /home/../root", "mkdir /home/a;", "mkdir /home/a; unknown /home/b", "write /home/a 'unfinished", "write /home/a ; delete /home/b"};
    for (size_t i = 0; i < sizeof(bad)/sizeof(bad[0]); ++i) { memset(&plan,0xa5,sizeof(plan)); assert(agent_parse_intent(bad[i],&plan) < 0 && plan.count == 0); }
    assert(agent_parse_intent("read /home/file",&plan) == 0); plan.actions[0].path[20] = 'x'; assert(agent_validate_plan(&plan) < 0);
    char oversized[AGENT_INTENT_MAX+1]; memset(oversized,'a',sizeof(oversized)); oversized[AGENT_INTENT_MAX]=0;
    assert(agent_parse_intent(oversized,&plan) == -7 && plan.count == 0);
    assert(agent_parse_intent("read /home/a; read /home/a; read /home/a; read /home/a; read /home/a; read /home/a; read /home/a; read /home/a; read /home/a",&plan) == -7 && plan.count == 0);
    char canonical[128]; assert(agent_path("/home//a/./b",canonical) == 0 && strcmp(canonical,"/home/a/b") == 0); assert(agent_path("/home2/a",canonical) < 0);
    uint32_t random = 17; char input[AGENT_INTENT_MAX]; const char alphabet[] = "abc /home;-_'\"\\\t";
    for (unsigned int trial = 0; trial < 20000; ++trial) { size_t length = trial%sizeof(input); for (size_t i = 0; i < length; ++i) { random = random*1664525u+1013904223u; input[i] = alphabet[random%(sizeof(alphabet)-1u)]; } input[length] = 0; long result = agent_parse_intent(input,&plan); assert(result == 0 ? agent_validate_plan(&plan) == 0 : plan.count == 0); }
    puts("PASS: typed intent contracts, quoted data, workspace containment, canonical zero-tailed plans, atomic parse failure and 20,000 bounded inputs"); return 0;
}
