#include "policy.h"
#include "syscall/abi.h"
#include "fs/vfs.h"
bool security_syscall_allowed(uint64_t capabilities, uint64_t number, uint64_t first, uint64_t second) {
    uint64_t required = 0;
    if (number == SYS_READ) { required = first == 0 ? CAP_CONSOLE_INPUT : first >= 3 ? CAP_FS_READ : 0; }
    else if (number == SYS_WRITE && first >= 3) { required = CAP_FS_WRITE; }
    else if (number == SYS_OPEN) { required = ((second & FS_READ) != 0 ? CAP_FS_READ : 0) | ((second & (FS_WRITE|FS_CREATE|FS_TRUNCATE|FS_APPEND)) != 0 ? CAP_FS_WRITE : 0); }
    else if (number == SYS_STAT || number == SYS_LIST) { required = CAP_FS_READ; }
    else if (number == SYS_MKDIR || number == SYS_UNLINK || number == SYS_RENAME || number == SYS_CHMOD || number == SYS_CHOWN) { required = CAP_FS_WRITE; }
    else if (number == SYS_SPAWN) { required = CAP_SPAWN; }
    else if (number == SYS_SYSTEM || number == SYS_TASK) { required = CAP_INSPECT; }
    else if (number >= SYS_NET_SOCKET && number <= SYS_NET_SHUTDOWN) { required = CAP_NETWORK; }
    else if (number >= SYS_GUI_DRAW && number <= SYS_GUI_PRESENT) { required = CAP_DISPLAY; }
    return (capabilities & required) == required;
}
