#include "syscall.h"
#include "display.h"
#include "user/copy.h"
#include "drivers/input.h"
#include "runtime/memory.h"
long gui_syscall(struct user_process *process, uint64_t number, uint64_t a, uint64_t b, uint64_t c) {
    (void)c;
    if (!process->display) { return SYS_EPERM; }
    if (number == SYS_GUI_PRESENT) { return display_present() ? 0 : SYS_EINVAL; }
    if (number == SYS_GUI_INFO) { struct gui_info info = display_info(); return copy_to_user(&process->space.vm, a, &info, sizeof(info)) ? 0 : SYS_EFAULT; }
    if (number == SYS_GUI_DRAW) { struct gui_draw draw; if (!copy_from_user(&process->space.vm, &draw, a, sizeof(draw))) { return SYS_EFAULT; } return display_draw(&draw) ? 0 : SYS_EINVAL; }
    if (number == SYS_GUI_INPUT) { struct input_event event; if (!user_buffer_valid(&process->space.vm, a, sizeof(event), true)) { return SYS_EFAULT; } if (!input_next(&event)) { return 0; } return copy_to_user(&process->space.vm, a, &event, sizeof(event)) ? 1 : SYS_EFAULT; }
    if (number == SYS_GUI_CAPTURE) { if (b > SYS_IO_MAX) { return SYS_E2BIG; } if (!user_buffer_valid(&process->space.vm, a, (size_t)b, true)) { return SYS_EFAULT; } size_t amount = process->output_bytes < b ? process->output_bytes : (size_t)b; if (!copy_to_user(&process->space.vm, a, process->output, amount)) { return SYS_EFAULT; } process->output_bytes -= amount; memmove(process->output, process->output + amount, process->output_bytes); return (long)amount; }
    return SYS_ENOSYS;
}
