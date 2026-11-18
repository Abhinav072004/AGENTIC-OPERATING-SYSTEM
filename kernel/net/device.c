#include "stack.h"
#include "drivers/e1000.h"
#include "interrupts/interrupts.h"
#include "debug/log.h"
#include "debug/panic.h"
#include "arch/x86_64/serial.h"
#include "user/user.h"
#include "scheduler/task.h"
static bool transmit(void *context, const void *frame, size_t length) { (void)context; return e1000_send(frame, length); }
void network_init(void) {
    struct net_ops ops = {.send = e1000_ready() ? transmit : NULL};
    net_stack_init(&ops, e1000_mac()); net_tick(interrupts_ticks());
    klog(LOG_INFO, "network", "IPv4=10.0.2.15/24 gateway=10.0.2.2; Ethernet device=%u; static configuration", (unsigned int)e1000_ready());
}
void network_poll(void) {
    unsigned char frame[1518];
    for (unsigned int i = 0; i < 8; ++i) { size_t length = e1000_receive(frame, sizeof(frame)); if (length == 0) { break; } net_input(frame, length); }
    net_tick(interrupts_ticks());
}
extern unsigned char user_nettest_start[], user_nettest_end[];
void network_self_test(void) {
#if CONFIG_SELFTEST == 24
    KASSERT(e1000_ready());
    uint64_t pid, tid; int status;
    serial_write("AGENTICOS: NETWORK TEST WAIT\n");
    KASSERT(user_process_start(user_nettest_start, (size_t)(user_nettest_end - user_nettest_start), "network", 0, false, &pid, &tid));
    scheduler_run(true, 2, 2500);
    KASSERT(user_process_reap(pid, &status));
    klog(LOG_INFO, "network-test", "native exit status=%d", status); KASSERT(status == 0);
    struct net_stats stats; net_get_stats(&stats);
    KASSERT(stats.retransmitted >= 5 && stats.received >= 10);
    klog(LOG_INFO, "network-test", "rx=%llu tx=%llu rejected=%llu retransmissions=%llu; native sockets reclaimed",
         (unsigned long long)stats.received, (unsigned long long)stats.sent, (unsigned long long)stats.rejected, (unsigned long long)stats.retransmitted);
    serial_write("AGENTICOS: NETWORK SOCKET PASS\n");
#endif
    serial_write("AGENTICOS: PHASE10 PASS\n");
}
