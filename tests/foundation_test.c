#include <assert.h>
#include <limits.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "debug/format.h"
#include "debug/log.h"
#include "debug/panic.h"
#include "arch/x86_64/diagnostics.h"
#include "cpu/info.h"

static void expect(const char *wanted, const char *format, ...) {
    char output[128];
    va_list args;
    va_start(args, format);
    int count = kvsnprintf(output, sizeof(output), format, args);
    va_end(args);
    assert(count == (int)strlen(wanted));
    assert(strcmp(output, wanted) == 0);
}

static void format_tests(void) {
    expect("", "");
    expect("literal %", "literal %%");
    expect("text Q", "%s %c", "text", 'Q');
    expect("(null)", "%s", (const char *)NULL);
    expect("0 -2147483648 2147483647", "%d %d %d", 0, INT_MIN, INT_MAX);
    expect("4294967295 ffffffff", "%u %x", UINT_MAX, UINT_MAX);
    expect("18446744073709551615 ffffffffffffffff", "%llu %llx", ULLONG_MAX, ULLONG_MAX);
    expect("-9223372036854775808", "%lld", LLONG_MIN);
    expect("-9223372036854775808", "%ld", LONG_MIN);
    expect("18446744073709551615", "%lu", ULONG_MAX);
    expect("37 25", "%zu %zx", (size_t)37, (size_t)37);
    int local = 0;
    char reference[64];
    (void)snprintf(reference, sizeof(reference), "%p", (void *)&local);
    expect(reference, "%p", (void *)&local);
    expect("0x0", "%p", (void *)NULL);
    char buffer[8];
    memset(buffer, 'G', sizeof(buffer));
    assert(ksnprintf(buffer + 1, 4, "%s", "abcdef") == 6);
    assert(buffer[0] == 'G' && memcmp(buffer + 1, "abc\0", 4) == 0 && buffer[5] == 'G');
    assert(ksnprintf(buffer, 1, "%u", 99u) == 2 && buffer[0] == '\0');
    assert(ksnprintf(NULL, 0, "%s", "count only") == 10);
    assert(ksnprintf(NULL, 1, "%s", "invalid destination") == -1);
    /* Call the va_list entry directly for intentionally unsupported syntax. */
    puts("PASS: formatter extrema, pointer, NULL string, truncation and guard bounds");
}

static char records[16][352];
static unsigned int record_count;
static volatile unsigned int interrupts_disabled;
static volatile unsigned int halt_count;
static volatile unsigned int evaluations;
static jmp_buf panic_jump;

static void capture(const char *record, void *context) {
    assert(context == &record_count);
    assert(record_count < 16 && strlen(record) < sizeof(records[0]));
    (void)strcpy(records[record_count++], record);
}

static void invalid_format(const char *format, ...) {
    char message[16];
    va_list args;
    va_start(args, format);
    assert(kvsnprintf(message, sizeof(message), format, args) == -1);
    va_end(args);
    assert(memchr(message, '\0', sizeof(message)) != NULL);
}

static void invalid_log(const char *format, ...) {
    va_list args;
    va_start(args, format);
    klogv(LOG_ERROR, "test", format, args);
    va_end(args);
}

static void logging_tests(void) {
    invalid_format("%q"); invalid_format("trailing %");
    invalid_format("%08x"); invalid_format("%ls"); invalid_format("%zd");
    record_count = 0;
    log_init(capture, &record_count, LOG_DEBUG);
    klog(LOG_DEBUG, "test", "debug %u", 7u);
    klog(LOG_INFO, "test", "info");
    klog(LOG_WARNING, "test", "warning");
    klog(LOG_ERROR, "test", "error");
    klog(LOG_PANIC, "test", "panic severity");
    assert(record_count == 5);
    assert(strcmp(records[0], "[1] [DEBUG] [test] debug 7\n") == 0);
    assert(strcmp(records[4], "[5] [PANIC] [test] panic severity\n") == 0);
    record_count = 0;
    log_init(capture, &record_count, LOG_PANIC);
    for (int level = LOG_DEBUG; level <= LOG_PANIC; ++level) {
        klog((enum log_level)level, "test", "filtered");
    }
    assert(record_count == 1 && strcmp(records[0], "[1] [PANIC] [test] filtered\n") == 0);
    log_init(NULL, NULL, LOG_DEBUG);
    klog(LOG_INFO, "test", "no sink");
    assert(record_count == 1);
    record_count = 0;
    log_init(capture, &record_count, LOG_DEBUG);
    klog((enum log_level)-1, "test", "invalid severity");
    assert(record_count == 0);
    klog(LOG_INFO, "bo\not", "line\n\r\033");
    assert(strcmp(records[0], "[1] [INFO] [bo?ot] line???\n") == 0);
    char long_message[1024];
    memset(long_message, 'x', sizeof(long_message) - 1);
    long_message[sizeof(long_message) - 1] = '\0';
    klog(LOG_ERROR, NULL, "%s", long_message);
    size_t length = strlen(records[1]);
    assert(strcmp(records[1] + length - 4, "...\n") == 0);
    invalid_log("%q");
    assert(strstr(records[2], "<invalid log format>") != NULL);
    puts("PASS: five log levels, filtering/reset, missing sink, controls and truncation");
}

void arch_disable_interrupts(void) { ++interrupts_disabled; }
_Noreturn void arch_panic_stop(void) {
    assert(interrupts_disabled != 0);
    ++halt_count;
    longjmp(panic_jump, 1);
}

static void panic_tests(void) {
    record_count = 0;
    log_init(capture, &record_count, LOG_PANIC);
    KASSERT(++evaluations == 1);
    assert(evaluations == 1 && interrupts_disabled == 0 && record_count == 0);
    if (setjmp(panic_jump) == 0) {
        KASSERT(++evaluations == 3);
        assert(!"failed assertion returned");
    }
    assert(evaluations == 2 && interrupts_disabled == 1 && halt_count == 1);
    assert(record_count == 1);
    assert(strstr(records[0], "[PANIC] [panic] tests/foundation_test.c:") != NULL);
    assert(strstr(records[0], "panic_tests: assertion failed: ++evaluations == 3") != NULL);
    if (setjmp(panic_jump) == 0) {
        PANIC("explicit panic %u", 42u);
    }
    assert(interrupts_disabled == 2 && halt_count == 2 && record_count == 2);
    assert(strstr(records[1], "explicit panic 42") != NULL);
    puts("PASS: assertions evaluate once; fatal path reports source, disables IRQs and stops");
}

static unsigned int cpu_case;
static uint32_t pack(const char *bytes) {
    return (uint32_t)(uint8_t)bytes[0] | ((uint32_t)(uint8_t)bytes[1] << 8) |
           ((uint32_t)(uint8_t)bytes[2] << 16) | ((uint32_t)(uint8_t)bytes[3] << 24);
}

static struct cpuid_registers fake_cpuid(uint32_t leaf, uint32_t subleaf) {
    struct cpuid_registers result = {0};
    assert(subleaf == 0);
    if (leaf == 0) {
        result.eax = cpu_case == 0 ? 0 : 1;
        result.ebx = pack("Genu"); result.edx = pack("ineI"); result.ecx = pack("ntel");
    } else if (leaf == 1) {
        assert(cpu_case != 0);
        uint32_t family = cpu_case == 1 ? 6u : cpu_case == 2 ? 15u : 5u;
        result.eax = (8u << 20) | (3u << 16) | (family << 8) | (10u << 4) | 2u;
        result.edx = (1u << 9) | (1u << 26);
        result.ecx = 1u << 31;
    } else if (leaf == UINT32_C(0x80000000)) {
        result.eax = cpu_case == 0 ? UINT32_C(0x80000000) : UINT32_C(0x80000004);
    } else if (leaf == UINT32_C(0x80000001)) {
        assert(cpu_case != 0);
        result.edx = (1u << 20) | (1u << 29);
    } else if (leaf >= UINT32_C(0x80000002) && leaf <= UINT32_C(0x80000004)) {
        assert(cpu_case != 0);
        static const char brand[49] = "AgenticOS synthetic CPU";
        const char *part = brand + (leaf - UINT32_C(0x80000002)) * 16;
        result.eax = pack(part); result.ebx = pack(part + 4);
        result.ecx = pack(part + 8); result.edx = pack(part + 12);
    } else { assert(!"unsupported CPUID leaf read"); }
    return result;
}

static void cpu_tests(void) {
    struct cpu_info info;
    memset(&info, 0xa5, sizeof(info));
    cpu_case = 0;
    cpu_info_read(&info, fake_cpuid);
    assert(strcmp(info.vendor, "GenuineIntel") == 0 && info.brand[0] == '\0');
    assert(info.family == 0 && info.model == 0 && info.stepping == 0);
    assert(!info.long_mode && !info.nx && !info.apic && !info.sse2 && !info.hypervisor);
    for (cpu_case = 1; cpu_case <= 3; ++cpu_case) {
        cpu_info_read(&info, fake_cpuid);
        assert(strcmp(info.brand, "AgenticOS synthetic CPU") == 0);
        assert(info.stepping == 2);
        assert(info.family == (cpu_case == 1 ? 6u : cpu_case == 2 ? 23u : 5u));
        assert(info.model == (cpu_case <= 2 ? 58u : 10u));
        assert(info.long_mode && info.nx && info.apic && info.sse2 && info.hypervisor);
    }
    puts("PASS: CPUID vendor/brand, supported-leaf guards and family/model extension rules");
}

int main(void) {
    format_tests(); logging_tests(); panic_tests(); cpu_tests();
    return 0;
}
