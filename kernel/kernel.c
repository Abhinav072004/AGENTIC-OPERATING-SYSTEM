#include <limine.h>
#include <stddef.h>
#include "arch/x86_64/io.h"
#include "arch/x86_64/serial.h"
#include "drivers/display/framebuffer.h"
#include "cpu/info.h"
#include "debug/log.h"
#include "debug/panic.h"
#include "memory/init.h"
#include "memory/probe.h"
#include "interrupts/interrupts.h"
#include "scheduler/task.h"
#include "user/user.h"
#include "fs/vfs.h"
#include "drivers/drivers.h"
#if CONFIG_SELFTEST == 21
_Noreturn void user_boot_failure_probe(void);
#endif
#if CONFIG_SELFTEST == 17 || CONFIG_SELFTEST == 18
_Noreturn void scheduler_failure_probe(unsigned int mode);
#endif
#if CONFIG_SELFTEST >= 8 && CONFIG_SELFTEST <= 15
_Noreturn void interrupt_exception_probe(unsigned int mode);
#endif

/* Volatile: Limine mutates these structures before transferring control. */
__attribute__((used, section(".limine_requests_start")))
static volatile LIMINE_REQUESTS_START_MARKER

__attribute__((used, section(".limine_requests")))
static volatile LIMINE_BASE_REVISION(3)

__attribute__((used, section(".limine_requests")))
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST,
    .revision = 0,
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_memmap_request memory_map_request = {
    .id = LIMINE_MEMMAP_REQUEST, .revision = 0,
};
__attribute__((used, section(".limine_requests")))
static volatile struct limine_hhdm_request hhdm_request = {
    .id = LIMINE_HHDM_REQUEST, .revision = 0,
};
__attribute__((used, section(".limine_requests")))
static volatile struct limine_kernel_address_request kernel_address_request = {
    .id = LIMINE_KERNEL_ADDRESS_REQUEST, .revision = 0,
};

__attribute__((used, section(".limine_requests_end")))
static volatile LIMINE_REQUESTS_END_MARKER

_Noreturn void kernel_main(void);
static struct memory_boot boot_memory;

static bool read_memory_boot(void) {
    const struct limine_memmap_response *map = memory_map_request.response;
    const struct limine_hhdm_response *hhdm = hhdm_request.response;
    const struct limine_kernel_address_response *image = kernel_address_request.response;
    if (map == NULL || map->entries == NULL || map->entry_count == 0 ||
        map->entry_count > PMM_REGION_LIMIT || hhdm == NULL || image == NULL) { return false; }
    boot_memory.count = (size_t)map->entry_count;
    boot_memory.hhdm = hhdm->offset;
    boot_memory.kernel_physical = image->physical_base;
    boot_memory.kernel_virtual = image->virtual_base;
    for (size_t i = 0; i < boot_memory.count; ++i) {
        const struct limine_memmap_entry *entry = map->entries[i];
        if (entry == NULL) { return false; }
        boot_memory.regions[i].base = entry->base;
        boot_memory.regions[i].length = entry->length;
        boot_memory.regions[i].usable = entry->type == LIMINE_MEMMAP_USABLE;
        uint64_t flags = MEMORY_SKIP_REGION;
        if (entry->type == LIMINE_MEMMAP_FRAMEBUFFER) {
            flags = VM_WRITE | VM_NX | VM_WRITE_THROUGH | VM_CACHE_DISABLE;
        } else if (entry->type == LIMINE_MEMMAP_USABLE || entry->type == LIMINE_MEMMAP_ACPI_RECLAIMABLE ||
                   entry->type == LIMINE_MEMMAP_ACPI_NVS || entry->type == LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE ||
                   entry->type == LIMINE_MEMMAP_KERNEL_AND_MODULES) {
            flags = VM_WRITE | VM_NX;
        }
        boot_memory.direct_flags[i] = flags;
    }
    return true;
}

static void serial_sink(const char *record, void *context) {
    (void)context;
    serial_write(record);
}

_Noreturn void kernel_main(void) {
    bool serial_ready = serial_init();
    log_init(serial_sink, NULL, CONFIG_LOG_LEVEL);
    serial_write("AGENTICOS: kernel entered\n");
    KASSERT(serial_ready);
    klog(LOG_INFO, "boot", "AgenticOS %s: initializing bootstrap CPU", AGENTICOS_VERSION);
    klog(LOG_DEBUG, "serial", "COM1 115200 8N1; transmit/receive loopback verified");
    if (!LIMINE_BASE_REVISION_SUPPORTED) {
        PANIC("unsupported Limine base revision");
    }
    const struct limine_framebuffer_response *response = framebuffer_request.response;
    if (response == NULL || response->framebuffer_count == 0 ||
        response->framebuffers == NULL ||
        !framebuffer_welcome(response->framebuffers[0])) {
        PANIC("missing or unsupported framebuffer");
    }
    klog(LOG_INFO, "boot", "Welcome to AgenticOS");
    struct cpu_info cpu;
    cpu_info_read(&cpu, arch_cpuid);
    KASSERT(cpu.long_mode);
    klog(LOG_INFO, "cpu", "vendor=%s family=%u model=%u stepping=%u",
         cpu.vendor, cpu.family, cpu.model, cpu.stepping);
    klog(LOG_INFO, "cpu", "brand=%s", cpu.brand[0] != '\0' ? cpu.brand : "unavailable");
    klog(LOG_DEBUG, "cpu", "long_mode=%u nx=%u sse2=%u apic=%u hypervisor=%u",
         (unsigned int)cpu.long_mode, (unsigned int)cpu.nx, (unsigned int)cpu.sse2,
         (unsigned int)cpu.apic, (unsigned int)cpu.hypervisor);
    interrupts_init();
    klog(LOG_WARNING, "boot", "Maskable interrupts disabled until timer verification");
    serial_write("AGENTICOS: PHASE1 PASS\n");

#if CONFIG_SELFTEST == 1
    klog(LOG_DEBUG, "selftest", "diagnostic DEBUG record");
    klog(LOG_INFO, "selftest", "diagnostic INFO record");
    klog(LOG_WARNING, "selftest", "diagnostic WARNING record");
    klog(LOG_ERROR, "selftest", "diagnostic ERROR record (intentional)");
#elif CONFIG_SELFTEST == 2
    PANIC("intentional panic probe");
#elif CONFIG_SELFTEST == 3
    KASSERT(2 + 2 == 5);
#endif
    klog(LOG_INFO, "boot", "Kernel foundation initialized");
    serial_write("AGENTICOS: PHASE2 PASS\n");
    KASSERT(cpu.nx);
    KASSERT(read_memory_boot());
    KASSERT(memory_bootstrap(&boot_memory));
    KASSERT(framebuffer_welcome(response->framebuffers[0]));
#if CONFIG_SELFTEST == 7
    memory_exhaustion_test();
#endif
    memory_self_test();
#if CONFIG_SELFTEST >= 4 && CONFIG_SELFTEST <= 6
    memory_fault_probe(CONFIG_SELFTEST);
#elif CONFIG_SELFTEST == 7
    serial_write("AGENTICOS: OOM PASS\n");
#endif
    klog(LOG_INFO, "boot", "Memory foundation initialized");
    serial_write("AGENTICOS: PHASE3 PASS\n");
    interrupts_self_test();
#if CONFIG_SELFTEST >= 8 && CONFIG_SELFTEST <= 15
    interrupt_exception_probe(CONFIG_SELFTEST);
#endif
    interrupts_timer_test();
    klog(LOG_INFO, "boot", "Interrupt foundation initialized");
    serial_write("AGENTICOS: PHASE4 PASS\n");
    scheduler_init();
#if CONFIG_SELFTEST == 17 || CONFIG_SELFTEST == 18
    scheduler_failure_probe(CONFIG_SELFTEST);
#endif
    scheduler_self_test();
    klog(LOG_INFO, "boot", "Task foundation initialized");
    serial_write("AGENTICOS: PHASE5 PASS\n");
#if CONFIG_SELFTEST == 21
    user_boot_failure_probe();
#endif
    user_self_test();
    klog(LOG_INFO, "boot", "User foundation initialized; halting");
    serial_write("AGENTICOS: PHASE6 PASS\n");
    fs_self_test();
    serial_write("AGENTICOS: PHASE7 PASS\n");
    drivers_init();
    drivers_self_test();
    cpu_halt();
}
