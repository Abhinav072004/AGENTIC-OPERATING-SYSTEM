#ifndef AGENTICOS_USER_AGENT_CLIENT_H
#define AGENTICOS_USER_AGENT_CLIENT_H
#include "agent/runtime.h"
const char *agent_state_label(uint32_t state);
const char *agent_tool_label(uint32_t tool);
const char *agent_audit_label(uint32_t event);
const char *agent_error_label(long error);
long agent_begin(const char *intent, uint32_t backend);
long agent_status(uint64_t id, struct agent_job_info *info);
long agent_review(uint64_t id, struct agent_plan *plan);
long agent_approve(uint64_t id, bool deletion);
long agent_cancel(uint64_t id);
long agent_release(uint64_t id);
long agent_wait(uint64_t id, bool review, struct agent_job_info *info);
long agent_command(int count, char **args, void (*emit)(const char *text));
#endif
