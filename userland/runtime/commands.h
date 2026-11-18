#ifndef AGENTICOS_COMMANDS_H
#define AGENTICOS_COMMANDS_H
#include <stdbool.h>
void commands_init(void (*emit)(const char *text));
long commands_run(const char *source);
const char *commands_cwd(void);
bool commands_leaving(void);
#endif
