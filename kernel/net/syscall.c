#include "syscall.h"
#include "stack.h"
#include "user/copy.h"
long network_syscall(struct user_process *process, uint64_t number, uint64_t a, uint64_t b, uint64_t c) {
    uint64_t owner = process->pid;
    if (number == SYS_NET_SOCKET) { return a > UINT32_MAX ? NET_EINVAL : net_socket(owner, (unsigned int)a); }
    if (number == SYS_NET_BIND) { return b > UINT16_MAX ? NET_EINVAL : net_bind(owner, a, (uint16_t)b); }
    if (number == SYS_NET_CLOSE) { return net_close(owner, a); }
    if (number == SYS_NET_SHUTDOWN) { return net_shutdown(owner, a); }
    if (number == SYS_NET_LISTEN) { return net_listen(owner, a); }
    if (number == SYS_NET_ACCEPT) { return net_accept(owner, a); }
    if (number == SYS_NET_PING) { return a > UINT32_MAX || b > UINT16_MAX || c > 1 ? NET_EINVAL : net_ping(owner, (uint32_t)a, (uint16_t)b, c != 0); }
    if (number == SYS_NET_CONNECT) { struct net_endpoint peer; if (!copy_from_user(&process->space.vm, &peer, b, sizeof(peer))) { return SYS_EFAULT; } return net_connect(owner, a, peer); }
    if (number == SYS_NET_STATUS) { struct net_status status; if (!user_buffer_valid(&process->space.vm, b, sizeof(status), true)) { return SYS_EFAULT; } long result = net_get_status(owner, a, &status); return result < 0 ? result : (copy_to_user(&process->space.vm, b, &status, sizeof(status)) ? 0 : SYS_EFAULT); }
    if (number == SYS_NET_SEND || number == SYS_NET_RECEIVE) {
        bool receive = number == SYS_NET_RECEIVE;
        if (c > NET_IO_MAX) { return SYS_E2BIG; }
        if (!user_buffer_valid(&process->space.vm, b, (size_t)c, receive)) { return SYS_EFAULT; }
        unsigned char buffer[NET_IO_MAX];
        if (!receive && !copy_from_user(&process->space.vm, buffer, b, (size_t)c)) { return SYS_EFAULT; }
        long result = receive ? net_receive(owner, a, buffer, (size_t)c) : net_send(owner, a, buffer, (size_t)c);
        if (receive && result > 0 && !copy_to_user(&process->space.vm, b, buffer, (size_t)result)) { return SYS_EFAULT; } return result;
    }
    return SYS_ENOSYS;
}
