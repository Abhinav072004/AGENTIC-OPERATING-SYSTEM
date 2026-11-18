#ifndef AGENTICOS_AGENT_PLAN_H
#define AGENTICOS_AGENT_PLAN_H
#include <stddef.h>
#include <stdint.h>
#define AGENT_PLAN_ABI 1u
#define AGENT_MAX_ACTIONS 8u
#define AGENT_FIELD_MAX 128u
#define AGENT_INTENT_MAX 512u
#define AGENT_MKDIR 1u
#define AGENT_WRITE 2u
#define AGENT_READ 3u
#define AGENT_LIST 4u
#define AGENT_MOVE 5u
#define AGENT_DELETE 6u
#define AGENT_SYSTEM 7u
struct agent_action { uint32_t tool, flags; char path[128], other[128], text[128]; };
struct agent_plan { uint32_t abi, count; struct agent_action actions[AGENT_MAX_ACTIONS]; };
_Static_assert(sizeof(struct agent_plan) <= 4096, "bounded copied plan");
long agent_path(const char *input, char output[AGENT_FIELD_MAX]);
long agent_validate_plan(const struct agent_plan *plan);
long agent_parse_intent(const char *intent, struct agent_plan *plan);
uint64_t agent_plan_permissions(const struct agent_plan *plan);
const char *agent_tool_name(uint32_t tool);
void agent_foundation_boot(void);
#endif
