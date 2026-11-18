#include "user/user.h"
#include "debug/panic.h"
_Noreturn void user_boot_failure_probe(void);
_Noreturn void user_boot_failure_probe(void) {
    static const unsigned char invalid[64] = {0};
    uint64_t pid, tid;
    KASSERT(user_process_start(invalid, sizeof(invalid), "required-invalid", 0, false, &pid, &tid));
    PANIC("invalid user image unexpectedly loaded");
}
