#include <limine.h>
#include <stddef.h>
#include "arch/x86_64/io.h"
#include "arch/x86_64/serial.h"
#include "drivers/display/framebuffer.h"

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

_Noreturn void kernel_main(void) {
    serial_init();
    serial_write("ASTRA: kernel entered\n");
    if (!LIMINE_BASE_REVISION_SUPPORTED) {
        serial_write("ASTRA: ERROR unsupported Limine base revision\n");
        cpu_halt();
    }
    const struct limine_framebuffer_response *response = framebuffer_request.response;
    if (response == NULL || response->framebuffer_count == 0 ||
        response->framebuffers == NULL ||
        !framebuffer_welcome(response->framebuffers[0])) {
        serial_write("ASTRA: ERROR missing or unsupported framebuffer\n");
        cpu_halt();
    }
    serial_write("Welcome to AstraOS\n");
    serial_write("ASTRA: PHASE1 PASS\n");
    cpu_halt();
}
