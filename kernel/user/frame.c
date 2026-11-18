#include "user.h"
bool user_frame_prepare(struct interrupt_frame *frame) {
    if (frame == NULL || frame->cs != X86_USER_CODE || frame->ss != X86_USER_DATA || frame->rip < 4096 ||
        frame->rip >= USER_LIMIT || frame->rsp < 4096 || frame->rsp >= USER_LIMIT) { return false; }
    /* Only arithmetic flags/DF survive; no IOPL, NT, VM, TF, AC or user CLI. */
    frame->rflags = (frame->rflags & UINT64_C(0xcd5)) | UINT64_C(0x202);
    return true;
}
