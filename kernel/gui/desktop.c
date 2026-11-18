#include "display.h"
#include "security/policy.h"
#include "drivers/display/framebuffer.h"
#include "fs/vfs.h"
#include "user/user.h"
#include "scheduler/task.h"
#include "memory/init.h"
#include "debug/panic.h"
#include "debug/log.h"
#include "arch/x86_64/serial.h"
extern unsigned char user_desktop_start[], user_desktop_end[];
static void run_desktop(const struct limine_framebuffer *framebuffer, uint64_t mode) {
    uint64_t frames = memory_frames()->free_frames; size_t blocks = fs_used_blocks();
    KASSERT(display_init(framebuffer) && gui_buffer_create());
    long pid = user_spawn_path("/bin/desktop",mode,1,false); KASSERT(pid > 0);
    struct user_process *process = user_find((uint64_t)pid); process->display = true; process->capabilities |= CAP_DISPLAY;
    scheduler_run(true,2,mode == 0 ? UINT64_MAX : 1000); int status;
    KASSERT(user_process_reap((uint64_t)pid,&status));
    klog(LOG_INFO,"desktop-test","mode=%u status=%d",(unsigned int)mode,status); KASSERT(status == 0);
    if (mode != 0) { KASSERT(fs_used_blocks() == blocks); }
    gui_buffer_destroy(); KASSERT(memory_frames()->free_frames == frames && framebuffer_welcome(framebuffer));
}
void desktop_boot(const struct limine_framebuffer *framebuffer) {
    KASSERT(fs_seed_program("/bin/desktop",user_desktop_start,(size_t)(user_desktop_end-user_desktop_start)) == 0);
    static const char readme[] = "AgenticOS stores files in RAM. Use Terminal for commands.\n";
    KASSERT(fs_seed("/home/README",readme,sizeof(readme)-1) == 0);
    run_desktop(framebuffer,1);
    serial_write("AGENTICOS: DESKTOP USER PASS\n");
    serial_write("AGENTICOS: PHASE11 PASS\n");
}
void applications_boot(const struct limine_framebuffer *framebuffer) { run_desktop(framebuffer,2); serial_write("AGENTICOS: APPLICATION USER PASS\n"); serial_write("AGENTICOS: PHASE13 PASS\n"); }
void desktop_session(const struct limine_framebuffer *framebuffer) { run_desktop(framebuffer,0); serial_write("AGENTICOS: SESSION EXIT\n"); }

void agent_integration_boot(const struct limine_framebuffer *framebuffer) { run_desktop(framebuffer,3); serial_write("AGENTICOS: AGENT5 PASS\n"); }
