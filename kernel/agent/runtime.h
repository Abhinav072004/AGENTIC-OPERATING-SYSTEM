#ifndef AGENTICOS_AGENT_RUNTIME_H
#define AGENTICOS_AGENT_RUNTIME_H
#include "broker.h"
#define AGENT_PLANNING 0u
#define AGENT_BACKEND_OFFLINE 0u
#define AGENT_BACKEND_MODEL 1u
struct agent_request { uint32_t backend, reserved; char intent[AGENT_INTENT_MAX]; };
struct user_process;
long agent_syscall(struct user_process *process, uint64_t number, uint64_t a, uint64_t b, uint64_t c);
void agent_poll(void);
void agent_process_reaped(uint64_t pid, int status);
void agent_runtime_boot(void);
#endif
