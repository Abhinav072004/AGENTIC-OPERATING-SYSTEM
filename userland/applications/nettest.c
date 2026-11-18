#include "syscall.h"
#include "text.h"
#include "net/stack.h"
int user_main(uint64_t pid, uint64_t mode);
#define CHECK(test) do { if (!(test)) { return __LINE__; } } while (0)
static void pause_poll(void) { (void)user_call(SYS_SLEEP, 1, 0, 0); }
static long connect_to(uint64_t fd, uint16_t port) {
    struct net_endpoint peer = {.ip = NET_GATEWAY, .port = port}; long result;
    for (unsigned int i = 0; i < 500; ++i) { result = user_call(SYS_NET_CONNECT, fd, (uintptr_t)&peer, 0); if (result != NET_EAGAIN) { return result; } pause_poll(); } return NET_ETIMEDOUT;
}
static long state_wait(uint64_t fd, uint32_t expected) {
    for (unsigned int i = 0; i < 1000; ++i) { struct net_status status; long result = user_call(SYS_NET_STATUS, fd, (uintptr_t)&status, 0); if (result < 0) { return result; } if (status.error != 0) { return status.error; } if (status.state == expected) { return 0; } pause_poll(); } return NET_ETIMEDOUT;
}
static long receive_text(uint64_t fd, const char *expected) {
    char buffer[128] = {0}; size_t position = 0, length = text_length(expected);
    for (unsigned int i = 0; i < 1000 && position < length; ++i) { long result = user_call(SYS_NET_RECEIVE, fd, (uintptr_t)(buffer + position), sizeof(buffer) - 1u - position); if (result == NET_EAGAIN) { pause_poll(); continue; } if (result <= 0) { return result == 0 ? NET_EINVAL : result; } position += (size_t)result; }
    return position == length && text_equal(buffer, expected) ? 0 : NET_EINVAL;
}
int user_main(uint64_t pid, uint64_t mode) {
    (void)pid; (void)mode;
    long result = NET_EAGAIN;
    for (unsigned int i = 0; i < 500 && result == NET_EAGAIN; ++i) { result = user_call(SYS_NET_PING, NET_GATEWAY, 7, 0); if (result == NET_EAGAIN) { pause_poll(); } } CHECK(result == 0);
    for (unsigned int i = 0; i < 500; ++i) { result = user_call(SYS_NET_PING, NET_GATEWAY, 7, 1); if (result != NET_EAGAIN) { break; } pause_poll(); } CHECK(result == 0);
    long udp = user_call(SYS_NET_SOCKET, NET_UDP, 0, 0); CHECK(udp >= 0 && connect_to((uint64_t)udp, 9000) == 0);
    CHECK(user_call(SYS_NET_SEND, (uint64_t)udp, (uintptr_t)"udp-request", 11) == 11);
    CHECK(user_call(SYS_NET_RECEIVE, (uint64_t)udp, UINT64_C(0xffffffff80000000), 12) == SYS_EFAULT);
    CHECK(receive_text((uint64_t)udp, "udp-response") == 0);
    long tcp = user_call(SYS_NET_SOCKET, NET_TCP, 0, 0); CHECK(tcp >= 0 && connect_to((uint64_t)tcp, 9001) == 0);
    CHECK(state_wait((uint64_t)tcp, NET_ESTABLISHED) == 0);
    CHECK(user_call(SYS_NET_SEND, (uint64_t)tcp, (uintptr_t)"tcp-request", 11) == 11);
    CHECK(receive_text((uint64_t)tcp, "tcp-response") == 0);
    CHECK(state_wait((uint64_t)tcp, NET_CLOSE_WAIT) == 0);
    char byte; CHECK(user_call(SYS_NET_RECEIVE, (uint64_t)tcp, (uintptr_t)&byte, 1) == 0);
    CHECK(user_call(SYS_NET_SHUTDOWN, (uint64_t)tcp, 0, 0) == 0 && state_wait((uint64_t)tcp, NET_CLOSED) == 0);
    CHECK(user_call(SYS_NET_CLOSE, (uint64_t)tcp, 0, 0) == 0);
    long listener = user_call(SYS_NET_SOCKET, NET_TCP, 0, 0); CHECK(listener >= 0);
    CHECK(user_call(SYS_NET_BIND, (uint64_t)listener, 8080, 0) == 0 && user_call(SYS_NET_LISTEN, (uint64_t)listener, 0, 0) == 0);
    CHECK(user_call(SYS_NET_SEND, (uint64_t)udp, (uintptr_t)"listen-ready", 12) == 12);
    long accepted = NET_EAGAIN;
    for (unsigned int i = 0; i < 500; ++i) { accepted = user_call(SYS_NET_ACCEPT, (uint64_t)listener, 0, 0); if (accepted != NET_EAGAIN) { break; } pause_poll(); } CHECK(accepted >= 0);
    CHECK(receive_text((uint64_t)accepted, "server-request") == 0);
    CHECK(user_call(SYS_NET_SEND, (uint64_t)accepted, (uintptr_t)"server-response", 15) == 15);
    CHECK(state_wait((uint64_t)accepted, NET_CLOSE_WAIT) == 0);
    CHECK(user_call(SYS_NET_SHUTDOWN, (uint64_t)accepted, 0, 0) == 0 && state_wait((uint64_t)accepted, NET_CLOSED) == 0);
    CHECK(user_call(SYS_NET_CLOSE, (uint64_t)accepted, 0, 0) == 0 && user_call(SYS_NET_CLOSE, (uint64_t)listener, 0, 0) == 0);
    long refused = user_call(SYS_NET_SOCKET, NET_TCP, 0, 0); CHECK(refused >= 0 && connect_to((uint64_t)refused, 9010) == 0);
    CHECK(state_wait((uint64_t)refused, NET_ESTABLISHED) == NET_ECONNREFUSED && user_call(SYS_NET_CLOSE, (uint64_t)refused, 0, 0) == 0);
    long timeout = user_call(SYS_NET_SOCKET, NET_TCP, 0, 0); CHECK(timeout >= 0 && connect_to((uint64_t)timeout, 9011) == 0);
    CHECK(state_wait((uint64_t)timeout, NET_ESTABLISHED) == NET_ETIMEDOUT && user_call(SYS_NET_CLOSE, (uint64_t)timeout, 0, 0) == 0);
    CHECK(user_call(SYS_NET_CLOSE, (uint64_t)udp, 0, 0) == 0 && user_call(SYS_NET_CLOSE, (uint64_t)udp, 0, 0) == NET_EBADF);
    CHECK(user_write(1, "native network checks passed", 28) == 28); return 0;
}
