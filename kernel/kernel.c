#include <limine.h>
#include <stddef.h>
#include "arch/x86_64/io.h"
#include "arch/x86_64/serial.h"
#include "drivers/display/framebuffer.h"
#include "cpu/info.h"
#include "debug/log.h"
#include "debug/panic.h"

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

__attribute__((used, section(".limine_requests_end")))
static volatile LIMINE_REQUESTS_END_MARKER

_Noreturn void kernel_main(void);

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
    klog(LOG_WARNING, "boot", "Interrupts disabled; exception handlers are not installed yet");
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
    klog(LOG_INFO, "boot", "Kernel foundation initialized; halting");
    serial_write("AGENTICOS: PHASE2 PASS\n");
    cpu_halt();
}
