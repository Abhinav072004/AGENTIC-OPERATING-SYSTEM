/* Test-image-only observer reached through the production page-fault ISR. */
#include <stddef.h>
#include <stdint.h>
#include "memory/init.h"
#include "memory/probe.h"
#include "arch/x86_64/io.h"
#include "arch/x86_64/serial.h"
#include "debug/log.h"
#include "debug/panic.h"

static volatile uint64_t expected_address, expected_error, data_frame;
static volatile unsigned int selected_mode;
extern unsigned char __text_start[], __rodata_start[];
void memory_execute_address(uint64_t address);

_Noreturn void memory_fault_report(uint64_t address, uint64_t error, uint64_t instruction) {
    KASSERT(address == expected_address && error == expected_error);
    const char *kind;
    if (selected_mode == 5) { KASSERT(instruction == expected_address); kind = "nx"; }
    else {
        KASSERT(instruction >= (uintptr_t)__text_start && instruction < (uintptr_t)__rodata_start);
        kind = selected_mode == 4 ? "readonly" : "guard";
    }
    if (selected_mode == 4) {
        volatile uint64_t *original = memory_frame_pointer(data_frame);
        KASSERT(original != NULL && *original == UINT64_C(0x13579bdf2468ace0));
    }
    klog(LOG_INFO, "memory-probe", "%s fault: address=%p error=0x%llx ip=%p", kind,
         (void *)(uintptr_t)address, (unsigned long long)error, (void *)(uintptr_t)instruction);
    serial_write("AGENTICOS: PAGEFAULT PASS\n");
    cpu_halt();
}

_Noreturn void memory_fault_probe(unsigned int mode) {
    KASSERT(mode >= 4 && mode <= 6);
    selected_mode = mode;
    expected_address = mode == 6 ? MEMORY_HEAP_GUARD : MEMORY_SCRATCH;
    expected_error = mode == 4 ? UINT64_C(3) : mode == 5 ? UINT64_C(0x11) : 0;
    if (mode != 6) {
        uint64_t physical;
        KASSERT(pmm_alloc_frame(memory_frames(), &physical));
        data_frame = physical;
        volatile uint64_t *pointer = memory_frame_pointer(physical);
        KASSERT(pointer != NULL);
        *pointer = mode == 4 ? UINT64_C(0x13579bdf2468ace0) : UINT64_C(0xc3);
        KASSERT(vmm_map(memory_address_space(), MEMORY_SCRATCH, physical,
                       mode == 4 ? VM_NX : VM_WRITE | VM_NX));
    }
    if (mode == 4) { *(volatile uint64_t *)(uintptr_t)expected_address = 0; }
    else if (mode == 5) { memory_execute_address(expected_address); }
    else {
        volatile uint8_t value = *(volatile uint8_t *)(uintptr_t)expected_address;
        (void)value;
    }
    PANIC("memory protection probe returned without its expected page fault");
}
