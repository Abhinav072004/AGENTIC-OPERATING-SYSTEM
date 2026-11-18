#include "syscall.h"
#include "fs/vfs.h"
#include "security/policy.h"
int user_main(uint64_t pid, uint64_t mode);
#define CHECK(test) do { if (!(test)) { return __LINE__; } } while (0)
static long path(uint64_t call, const char *name, uint64_t b) { return user_call(call,(uintptr_t)name,b,0); }
int user_main(uint64_t pid, uint64_t mode) {
    (void)pid;
    if (mode == 0) {
        char byte; struct fs_info info; struct self_info own;
        CHECK(user_call(SYS_SELF,(uintptr_t)&own,0,0) == 0 && own.uid == 1000 && own.capabilities == 0);
        CHECK(path(SYS_OPEN,"/etc/motd",FS_READ) == SYS_EPERM);
        CHECK(path(SYS_UNLINK,"/tmp/other",0) == SYS_EPERM);
        CHECK(path(SYS_SPAWN,"/bin/hello",0) == SYS_EPERM);
        CHECK(user_call(SYS_GUI_INFO,(uintptr_t)&info,0,0) == SYS_EPERM);
        CHECK(user_call(SYS_NET_SOCKET,1,0,0) == SYS_EPERM);
        CHECK(user_call(SYS_SYSTEM,(uintptr_t)&info,0,0) == SYS_EPERM);
        CHECK(user_read(0,&byte,1) == SYS_EPERM);
        CHECK(user_call(SYS_GETPID,0,0,0) == (long)pid);
    } else if (mode == 1) {
        CHECK(path(SYS_OPEN,"/home/secret",FS_READ) == FS_EACCES);
        CHECK(path(SYS_OPEN,"/bin/hello",FS_WRITE) == FS_EACCES);
        CHECK(path(SYS_UNLINK,"/tmp/other",0) == FS_EACCES);
        CHECK(path(SYS_CHMOD,"/tmp/other",0777) == FS_EPERM);
        CHECK(path(SYS_CHOWN,"/tmp/other",1001) == FS_EPERM);
        long fd = path(SYS_OPEN,"/tmp/guest",FS_CREATE|FS_READ|FS_WRITE); CHECK(fd >= 3);
        CHECK(user_write((uint64_t)fd,"sandbox",7) == 7);
        struct fs_info info; CHECK(path(SYS_STAT,"/tmp/guest",(uintptr_t)&info) == 0 && info.uid == 1001 && info.mode == 0600);
        CHECK(path(SYS_UNLINK,"/tmp/guest",0) == 0); /* Deliberately leave the descriptor open. */
    } else if (mode == 2) {
        long fd = path(SYS_OPEN,"/tmp/orphan",FS_CREATE|FS_WRITE); CHECK(fd >= 3 && user_write((uint64_t)fd,"orphan",6) == 6);
        CHECK(path(SYS_UNLINK,"/tmp/orphan",0) == 0);
        CHECK(path(SYS_SPAWN,"/bin/security",4) > 0); /* Exit must cancel and reap this child. */
    } else if (mode == 3) {
        for (;;) { (void)user_call(SYS_GETPID,0,0,0); }
    } else if (mode == 4) {
        for (;;) { __asm__ volatile ("pause" : : : "memory"); }
    } else if (mode == 5) {
        static const uint64_t calls[] = {SYS_OPEN,SYS_STAT,SYS_LIST,SYS_RENAME,SYS_SYSTEM,SYS_TASK,SYS_SPAWN,SYS_NET_CONNECT,SYS_NET_STATUS,SYS_NET_RECEIVE,SYS_GUI_DRAW,SYS_GUI_INPUT,SYS_GUI_INFO,SYS_GUI_CAPTURE,SYS_SELF};
        for (size_t i = 0; i < sizeof(calls)/sizeof(calls[0]); ++i) { CHECK(user_call(calls[i],UINT64_MAX,UINT64_MAX,UINT64_MAX) < 0); }
        CHECK(path(SYS_OPEN,"/etc/motd",FS_READ) >= 3); /* Valid work still succeeds after rejection. */
    }
    return 0;
}
