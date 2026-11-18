#include "display.h"
#include "memory/init.h"
#include "debug/panic.h"
#include "runtime/memory.h"
#define BUFFER_BASE UINT64_C(0xffffd40000000000)
static uint64_t frames[2025];
static size_t count;
void gui_buffer_destroy(void) {
    display_set_buffer(NULL);
    while (count != 0) { --count; KASSERT(vmm_unmap(memory_address_space(),BUFFER_BASE+count*PAGE_SIZE,NULL)); KASSERT(pmm_free_frame(memory_frames(),frames[count])); }
}
bool gui_buffer_create(void) {
    if (count != 0) { return false; }
    struct gui_info info = display_info();
    size_t pages = ((size_t)info.width*info.height*4u+4095u)/4096u;
    if (pages == 0 || pages > 2025) { return false; }
    for (; count < pages; ++count) {
        if (!pmm_alloc_frame(memory_frames(),&frames[count])) { gui_buffer_destroy(); return false; }
        memset(memory_frame_pointer(frames[count]),0,PAGE_SIZE);
        if (!vmm_map(memory_address_space(),BUFFER_BASE+count*PAGE_SIZE,frames[count],VM_WRITE|VM_NX)) { KASSERT(pmm_free_frame(memory_frames(),frames[count])); gui_buffer_destroy(); return false; }
    }
    display_set_buffer((void *)(uintptr_t)BUFFER_BASE); return true;
}
