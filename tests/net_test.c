#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "net/stack.h"
static unsigned char last[1518]; static size_t last_length;
static const uint8_t mac[6] = {0x52,0x54,0,0x12,0x34,0x56};
static bool send_frame(void *context, const void *frame, size_t length) { (void)context; assert(length <= sizeof(last)); memcpy(last, frame, length); last_length = length; return true; }
static void put16(unsigned char *p, unsigned int value) { p[0] = (unsigned char)(value >> 8); p[1] = (unsigned char)value; }
static unsigned int sum(const unsigned char *p, size_t length) { unsigned int total = 0; for (size_t i = 0; i < length; i += 2) { total += (unsigned int)p[i] << 8; if (i + 1 < length) { total += p[i + 1]; } } while (total >> 16) { total = (total & 65535u) + (total >> 16); } return (~total) & 65535u; }
static size_t packet(unsigned char frame[1519], unsigned int payload) {
    memset(frame, 0, 1519); memcpy(frame, mac, 6); frame[6] = 2; put16(frame + 12, 0x800); frame[14] = 0x45; put16(frame + 16, 28 + payload); frame[22] = 64; frame[23] = 17;
    frame[26] = 10; frame[28] = 2; frame[29] = 2; frame[30] = 10; frame[32] = 2; frame[33] = 15;
    put16(frame + 34, 9000); put16(frame + 36, 7000); put16(frame + 38, 8 + payload); memcpy(frame + 42, "hello", payload);
    put16(frame + 24, sum(frame + 14, 20)); return 42 + payload;
}
int main(void) {
    struct net_ops ops = {.send = send_frame}; net_stack_init(&ops, mac);
    long fd = net_socket(7, NET_UDP); assert(fd == 0 && net_bind(7, 0, 7000) == 0);
    assert(net_bind(8, 0, 7000) == NET_EBADF); long other = net_socket(8, NET_UDP); assert(other == 1 && net_bind(8, 1, 7000) == NET_EADDRINUSE);
    unsigned char frame[1519], buffer[32]; size_t length = packet(frame, 5);
    for (size_t i = 0; i < length; ++i) { net_input(frame, i); }
    assert(net_receive(7, 0, buffer, sizeof(buffer)) == NET_EAGAIN);
    frame[20] = 0x20; put16(frame + 24, 0); put16(frame + 24, sum(frame + 14, 20)); net_input(frame, length);
    assert(net_receive(7, 0, buffer, sizeof(buffer)) == NET_EAGAIN);
    length = packet(frame, 5); frame[40] = 1; net_input(frame, length); assert(net_receive(7, 0, buffer, sizeof(buffer)) == NET_EAGAIN);
    length = packet(frame, 5); net_input(frame, length); assert(net_receive(8, 0, buffer, sizeof(buffer)) == NET_EBADF);
    assert(net_receive(7, 0, buffer, sizeof(buffer)) == 5 && memcmp(buffer, "hello", 5) == 0);
    length = packet(frame, 0); net_input(frame, length); assert(net_receive(7, 0, buffer, sizeof(buffer)) == 0 && net_receive(7, 0, buffer, sizeof(buffer)) == NET_EAGAIN);
    struct net_endpoint peer = {.ip = NET_GATEWAY, .port = 9000}; assert(net_connect(7, 0, peer) == 0 && net_send(7, 0, "abc", 3) == 3);
    assert(last_length == 60 && last[23] == 17 && last[42] == 'a' && sum(last + 14, 20) == 0);
    long tcp = net_socket(7, NET_TCP); assert(tcp == 2 && net_connect(7, 2, peer) == 0);
    for (unsigned int i = 1; i <= 4; ++i) { net_tick(100u * i); }
    struct net_status status; assert(net_get_status(7, 2, &status) == 0 && status.state == NET_FAILED && status.error == NET_ETIMEDOUT);
    net_release(7); assert(net_get_status(7, 0, &status) == NET_EBADF && net_close(8, 1) == 0);
    uint32_t random = 0x76543210;
    for (unsigned int trial = 0; trial < 30000; ++trial) {
        size_t count = trial % sizeof(frame);
        for (size_t i = 0; i < count; ++i) { random = random * 1664525u + 1013904223u; frame[i] = (unsigned char)(random >> 24); }
        if (count >= 14) { memcpy(frame, mac, 6); put16(frame + 12, (trial & 1u) ? 0x800 : 0x806); }
        net_input(frame, count);
    }
    net_input(NULL, 1); net_input(frame, 1519);
    struct net_stats stats; net_get_stats(&stats); assert(stats.rejected > 29000 && stats.retransmitted == 3);
    puts("PASS: packet truncation/checksums/fragments, UDP zero-length/data, socket ownership/ports, TCP timeout/retries and 30,000 malformed Ethernet frames"); return 0;
}
