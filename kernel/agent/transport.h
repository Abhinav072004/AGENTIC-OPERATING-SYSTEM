#ifndef AGENTICOS_AGENT_TRANSPORT_H
#define AGENTICOS_AGENT_TRANSPORT_H
#include "plan.h"
#include <stdbool.h>
_Static_assert(sizeof(struct agent_plan)==3144,"model bridge wire ABI changed");
struct agent_transport { long fd; uint64_t owner; size_t sent, received, length, request_length; bool connected; unsigned char request[AGENT_INTENT_MAX+8], reply[sizeof(struct agent_plan)+12]; };
void agent_transport_init(struct agent_transport *transport, uint64_t owner, const char *intent);
long agent_transport_poll(struct agent_transport *transport, struct agent_plan *plan);
void agent_transport_close(struct agent_transport *transport);
#endif
