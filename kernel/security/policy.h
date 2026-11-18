#ifndef AGENTICOS_SECURITY_POLICY_H
#define AGENTICOS_SECURITY_POLICY_H
#include <stdbool.h>
#include <stdint.h>
#define CAP_FS_READ UINT64_C(1)
#define CAP_FS_WRITE UINT64_C(2)
#define CAP_SPAWN UINT64_C(4)
#define CAP_NETWORK UINT64_C(8)
#define CAP_INSPECT UINT64_C(16)
#define CAP_DISPLAY UINT64_C(32)
#define CAP_CONSOLE_INPUT UINT64_C(64)
#define CAP_AGENT_CONTROL UINT64_C(128)
#define CAP_SESSION (CAP_FS_READ|CAP_FS_WRITE|CAP_SPAWN|CAP_NETWORK|CAP_INSPECT|CAP_CONSOLE_INPUT|CAP_AGENT_CONTROL)
#define USER_QUOTA_STATUS (-122)
bool security_syscall_allowed(uint64_t capabilities, uint64_t number, uint64_t first, uint64_t second);
void security_self_test(void);
#endif
