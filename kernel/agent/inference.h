#ifndef AGENTICOS_AGENT_INFERENCE_H
#define AGENTICOS_AGENT_INFERENCE_H
#include "plan.h"
long agent_classify(const char *text, uint32_t *tool, int32_t *margin);
long agent_infer_offline(const char *text, struct agent_plan *plan);
void agent_inference_boot(void);
#endif
