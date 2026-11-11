#ifndef AGENTICOS_SERIAL_H
#define AGENTICOS_SERIAL_H

#include <stdbool.h>

/* Initialize and verify COM1 using its internal transmit/receive loopback. */
bool serial_init(void);
/* Nonblocking; consumes and discards errored input. No interrupt dependency. */
bool serial_try_read(char *character);
void serial_write(const char *text);

#endif
