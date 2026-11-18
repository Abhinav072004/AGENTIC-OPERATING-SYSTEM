#include "display.h"
#include "drivers/display/framebuffer.h"
#include "fs/vfs.h"
#include "user/user.h"
#include "scheduler/task.h"
#include "memory/init.h"
#include "debug/panic.h"
#include "arch/x86_64/serial.h"
extern unsigned char user_desktop_start[], user_desktop_end[];
void desktop_boot(const struct limine_framebuffer *framebuffer) {
    KASSERT(display_init(framebuffer));
    KASSERT(fs_seed("/bin/desktop", user_desktop_start, (size_t)(user_desktop_end - user_desktop_start)) == 0);
    static const char readme[] = "AgenticOS stores files in RAM. Use Terminal for commands.\n";
    KASSERT(fs_seed("/home/README", readme, sizeof(readme)-1) == 0);
    uint64_t frames = memory_frames()->free_frames;
    KASSERT(gui_buffer_create());
    long pid = user_spawn_path("/bin/desktop", CONFIG_SELFTEST == 25 ? 0 : 1, 1, false); KASSERT(pid > 0);
    user_find((uint64_t)pid)->display = true;
    scheduler_run(true, 2, CONFIG_SELFTEST == 25 ? UINT64_MAX : 1000);
    int status; KASSERT(user_process_reap((uint64_t)pid, &status) && status == 0);
    gui_buffer_destroy();
    KASSERT(memory_frames()->free_frames == frames);
    KASSERT(framebuffer_welcome(framebuffer));
    serial_write("AGENTICOS: DESKTOP USER PASS\n");
    serial_write("AGENTICOS: PHASE11 PASS\n");
}
