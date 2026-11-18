#ifndef AGENTICOS_AGENT_PANEL_H
#define AGENTICOS_AGENT_PANEL_H
#include "agent_client.h"
struct agent_panel { char intent[AGENT_INTENT_MAX]; size_t length; uint64_t id; uint32_t backend, action, seen; struct agent_plan plan; struct agent_job_info info; struct agent_audit audit; bool has_plan, deletion, audit_mode; char message[96]; };
extern struct agent_panel agent_panel;
void agent_panel_init(void);
void agent_panel_key(uint32_t key, uint32_t modifiers);
void agent_panel_click(int x, int y);
bool agent_panel_tick(void);
bool agent_panel_draw(int x, int y);
void agent_panel_close(void);
int agent_panel_acceptance(void);
#endif
