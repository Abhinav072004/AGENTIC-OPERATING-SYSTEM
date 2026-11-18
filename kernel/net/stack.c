#include "stack.h"
#include "runtime/memory.h"
#define RX_CAPACITY 4096u
struct socket_state {
    uint64_t owner, deadline;
    uint32_t state, remote_ip, snd_next, snd_unacked, rcv_next;
    uint16_t local_port, remote_port, window;
    int32_t error;
    unsigned char received[RX_CAPACITY], pending[NET_IO_MAX];
    size_t received_bytes, pending_bytes;
    uint8_t pending_flags, retries;
    int listener;
    bool live, published, fin_received, datagram_pending;
};
struct neighbor { uint32_t ip; uint8_t mac[6]; uint64_t expiry; };
static struct socket_state sockets[NET_SOCKET_MAX];
static struct neighbor neighbors[4];
static struct net_ops io;
static uint8_t local_mac[6];
static uint64_t now, arp_retry;
static uint32_t arp_pending;
static uint16_t next_port, identifier;
static uint32_t next_sequence;
static struct net_stats statistics;
static struct { uint64_t owner, deadline; uint32_t ip; uint16_t tag; long result; bool live; } ping;
static uint16_t get16(const unsigned char *p) { return (uint16_t)(((uint16_t)p[0] << 8) | p[1]); }
static uint32_t get32(const unsigned char *p) { return ((uint32_t)get16(p) << 16) | get16(p + 2); }
static void put16(unsigned char *p, uint16_t value) { p[0] = (uint8_t)(value >> 8); p[1] = (uint8_t)value; }
static void put32(unsigned char *p, uint32_t value) { put16(p, (uint16_t)(value >> 16)); put16(p + 2, (uint16_t)value); }
static uint32_t sum_bytes(const unsigned char *bytes, size_t length, uint32_t sum) {
    while (length >= 2) { sum += get16(bytes); bytes += 2; length -= 2; }
    if (length != 0) { sum += (uint32_t)bytes[0] << 8; } return sum;
}
static uint16_t finish_sum(uint32_t sum) { while ((sum >> 16) != 0) { sum = (sum & 0xffffu) + (sum >> 16); } return (uint16_t)~sum; }
static uint16_t checksum(const unsigned char *bytes, size_t length) { return finish_sum(sum_bytes(bytes, length, 0)); }
static uint16_t transport_checksum(uint32_t source, uint32_t destination, uint8_t protocol, const unsigned char *bytes, size_t length) {
    uint32_t sum = (source >> 16) + (source & 0xffffu) + (destination >> 16) + (destination & 0xffffu) + protocol + (uint32_t)length;
    return finish_sum(sum_bytes(bytes, length, sum));
}
static bool unicast_ip(uint32_t ip) { return ip != 0 && ip != UINT32_MAX && (ip >> 24) != 127 && (ip >> 24) < 224; }
static bool send_frame(const void *frame, size_t length) { if (io.send == NULL || !io.send(io.context, frame, length)) { return false; } ++statistics.sent; return true; }
static void learn(uint32_t ip, const unsigned char *mac) {
    if (!unicast_ip(ip) || (mac[0] & 1u) != 0) { return; }
    size_t slot = 0;
    for (size_t i = 0; i < 4; ++i) { if (neighbors[i].ip == ip) { slot = i; break; } if (neighbors[i].expiry < neighbors[slot].expiry) { slot = i; } }
    neighbors[slot].ip = ip; memcpy(neighbors[slot].mac, mac, 6); neighbors[slot].expiry = now + 3000;
}
static void arp(uint32_t target, const unsigned char *destination, bool reply) {
    unsigned char frame[60] = {0};
    if (destination == NULL) { memset(frame, 0xff, 6); } else { memcpy(frame, destination, 6); }
    memcpy(frame + 6, local_mac, 6); put16(frame + 12, 0x0806); put16(frame + 14, 1); put16(frame + 16, 0x0800);
    frame[18] = 6; frame[19] = 4; put16(frame + 20, reply ? 2 : 1); memcpy(frame + 22, local_mac, 6); put32(frame + 28, NET_LOCAL_IP);
    if (destination != NULL) { memcpy(frame + 32, destination, 6); } put32(frame + 38, target); (void)send_frame(frame, sizeof(frame));
}
static bool resolve(uint32_t ip, unsigned char mac[6]) {
    uint32_t next = (ip & UINT32_C(0xffffff00)) == (NET_LOCAL_IP & UINT32_C(0xffffff00)) ? ip : NET_GATEWAY;
    for (size_t i = 0; i < 4; ++i) { if (neighbors[i].ip == next && neighbors[i].expiry > now) { memcpy(mac, neighbors[i].mac, 6); return true; } }
    if (arp_pending != next || now >= arp_retry) { arp(next, NULL, false); arp_pending = next; arp_retry = now + 100; }
    return false;
}
static bool send_ip(uint32_t destination, uint8_t protocol, const void *payload, size_t length) {
    unsigned char frame[1518] = {0};
    if (length > 1480 || !resolve(destination, frame)) { return false; }
    memcpy(frame + 6, local_mac, 6); put16(frame + 12, 0x0800);
    unsigned char *ip = frame + 14; ip[0] = 0x45; put16(ip + 2, (uint16_t)(20 + length)); put16(ip + 4, ++identifier);
    put16(ip + 6, 0x4000); ip[8] = 64; ip[9] = protocol; put32(ip + 12, NET_LOCAL_IP); put32(ip + 16, destination); put16(ip + 10, checksum(ip, 20));
    memcpy(ip + 20, payload, length); size_t size = 34 + length; return send_frame(frame, size < 60 ? 60 : size);
}
static struct socket_state *find_socket(uint64_t owner, uint64_t fd) { return fd < NET_SOCKET_MAX && sockets[fd].live && sockets[fd].published && sockets[fd].owner == owner ? &sockets[fd] : NULL; }
static int free_slot(void) { for (unsigned int i = 0; i < NET_SOCKET_MAX; ++i) { if (!sockets[i].live) { return (int)i; } } return -1; }
static bool port_used(uint16_t port, bool udp) {
    for (size_t i = 0; i < NET_SOCKET_MAX; ++i) { if (sockets[i].live && sockets[i].local_port == port && ((sockets[i].state == NET_DATAGRAM) == udp) && sockets[i].listener < 0) { return true; } } return false;
}
static bool assign_port(struct socket_state *socket) {
    if (socket->local_port != 0) { return true; }
    for (unsigned int i = 0; i < 16384; ++i) { uint16_t port = next_port++; if (next_port < 49152) { next_port = 49152; } if (!port_used(port, socket->state == NET_DATAGRAM)) { socket->local_port = port; return true; } } return false;
}
static bool tcp_segment(struct socket_state *socket, uint8_t flags, uint32_t sequence, const void *data, size_t length) {
    unsigned char segment[20 + NET_IO_MAX] = {0};
    put16(segment, socket->local_port); put16(segment + 2, socket->remote_port); put32(segment + 4, sequence); put32(segment + 8, socket->rcv_next);
    segment[12] = 0x50; segment[13] = flags; put16(segment + 14, (uint16_t)(RX_CAPACITY - socket->received_bytes));
    if (length != 0) { memcpy(segment + 20, data, length); }
    put16(segment + 16, transport_checksum(NET_LOCAL_IP, socket->remote_ip, 6, segment, 20 + length));
    return send_ip(socket->remote_ip, 6, segment, 20 + length);
}
static bool reliable(struct socket_state *socket, uint8_t flags, const void *bytes, size_t length) {
    if (!tcp_segment(socket, flags, socket->snd_next, bytes, length)) { return false; }
    socket->snd_unacked = socket->snd_next;
    socket->snd_next += (uint32_t)length + (((flags & 3u) != 0) ? 1u : 0u);
    socket->pending_flags = flags; socket->pending_bytes = length; if (length != 0) { memcpy(socket->pending, bytes, length); }
    socket->deadline = now + 100; socket->retries = 0; return true;
}
void net_stack_init(const struct net_ops *ops, const uint8_t mac[6]) {
    memset(sockets, 0, sizeof(sockets)); memset(neighbors, 0, sizeof(neighbors)); memset(&statistics, 0, sizeof(statistics)); memset(&ping, 0, sizeof(ping));
    io = *ops; memcpy(local_mac, mac, 6); now = arp_retry = 0; arp_pending = 0; next_port = 49152; identifier = 0; next_sequence = 0x13572468;
}
long net_socket(uint64_t owner, unsigned int type) {
    if (io.send == NULL) { return NET_ENETDOWN; }
    if (type != NET_UDP && type != NET_TCP) { return NET_EINVAL; }
    int slot = free_slot(); if (slot < 0) { return -24; }
    sockets[slot] = (struct socket_state){.owner = owner, .state = type == NET_UDP ? NET_DATAGRAM : NET_CLOSED, .live = true, .published = true, .listener = -1, .window = 512}; return slot;
}
long net_bind(uint64_t owner, uint64_t fd, uint16_t port) {
    struct socket_state *socket = find_socket(owner, fd); if (socket == NULL) { return NET_EBADF; }
    if (port == 0 || socket->local_port != 0 || (socket->state != NET_DATAGRAM && socket->state != NET_CLOSED)) { return NET_EINVAL; }
    if (port_used(port, socket->state == NET_DATAGRAM)) { return NET_EADDRINUSE; } socket->local_port = port; return 0;
}
long net_connect(uint64_t owner, uint64_t fd, struct net_endpoint peer) {
    struct socket_state *socket = find_socket(owner, fd); if (socket == NULL) { return NET_EBADF; }
    if (!unicast_ip(peer.ip) || peer.port == 0 || peer.reserved != 0) { return NET_EINVAL; }
    if (socket->state == NET_SYN_SENT) { return NET_EAGAIN; }
    if (socket->state != NET_CLOSED && socket->state != NET_DATAGRAM) { return NET_EINVAL; }
    if (!assign_port(socket)) { return NET_EADDRINUSE; }
    unsigned char mac[6]; if (!resolve(peer.ip, mac)) { return NET_EAGAIN; }
    socket->remote_ip = peer.ip; socket->remote_port = peer.port;
    if (socket->state == NET_DATAGRAM) { return 0; }
    socket->snd_next = socket->snd_unacked = next_sequence; next_sequence += 0x10001;
    if (!reliable(socket, 2, NULL, 0)) { return NET_EAGAIN; } socket->state = NET_SYN_SENT; return 0;
}
long net_send(uint64_t owner, uint64_t fd, const void *buffer, size_t length) {
    struct socket_state *socket = find_socket(owner, fd); if (socket == NULL) { return NET_EBADF; }
    if (length > NET_IO_MAX) { return NET_EINVAL; } if (socket->error != 0) { return socket->error; }
    if (socket->state == NET_DATAGRAM) {
        if (socket->remote_port == 0) { return NET_EINVAL; } unsigned char datagram[8 + NET_IO_MAX] = {0};
        put16(datagram, socket->local_port); put16(datagram + 2, socket->remote_port); put16(datagram + 4, (uint16_t)(8 + length)); if (length != 0) { memcpy(datagram + 8, buffer, length); }
        uint16_t check = transport_checksum(NET_LOCAL_IP, socket->remote_ip, 17, datagram, 8 + length); put16(datagram + 6, check == 0 ? UINT16_MAX : check);
        return send_ip(socket->remote_ip, 17, datagram, 8 + length) ? (long)length : NET_EAGAIN;
    }
    if (socket->state != NET_ESTABLISHED && socket->state != NET_CLOSE_WAIT) { return NET_EAGAIN; }
    if (length == 0) { return 0; }
    if (socket->snd_unacked != socket->snd_next || length > socket->window) { return NET_EAGAIN; }
    return reliable(socket, 0x18, buffer, length) ? (long)length : NET_EAGAIN;
}
long net_receive(uint64_t owner, uint64_t fd, void *buffer, size_t capacity) {
    struct socket_state *socket = find_socket(owner, fd); if (socket == NULL) { return NET_EBADF; }
    if (capacity == 0) { return 0; } if (socket->error != 0) { return socket->error; }
    if (socket->received_bytes == 0 && !socket->datagram_pending) { return socket->fin_received ? 0 : NET_EAGAIN; }
    size_t count = socket->received_bytes < capacity ? socket->received_bytes : capacity;
    memcpy(buffer, socket->received, count);
    if (socket->state == NET_DATAGRAM) { socket->received_bytes = 0; socket->datagram_pending = false; }
    else { socket->received_bytes -= count; memmove(socket->received, socket->received + count, socket->received_bytes); (void)tcp_segment(socket, 0x10, socket->snd_next, NULL, 0); }
    return (long)count;
}
long net_listen(uint64_t owner, uint64_t fd) {
    struct socket_state *socket = find_socket(owner, fd); if (socket == NULL) { return NET_EBADF; }
    if (socket->state != NET_CLOSED || socket->local_port == 0) { return NET_EINVAL; } socket->state = NET_LISTEN; return 0;
}
long net_accept(uint64_t owner, uint64_t fd) {
    struct socket_state *socket = find_socket(owner, fd); if (socket == NULL) { return NET_EBADF; }
    if (socket->state != NET_LISTEN) { return NET_EINVAL; }
    for (unsigned int i = 0; i < NET_SOCKET_MAX; ++i) { if (sockets[i].live && !sockets[i].published && sockets[i].listener == (int)fd && sockets[i].state == NET_ESTABLISHED) { sockets[i].published = true; return (long)i; } } return NET_EAGAIN;
}
long net_shutdown(uint64_t owner, uint64_t fd) {
    struct socket_state *socket = find_socket(owner, fd); if (socket == NULL) { return NET_EBADF; }
    if (socket->state == NET_ESTABLISHED || socket->state == NET_CLOSE_WAIT) {
        if (socket->snd_next != socket->snd_unacked) { return NET_EAGAIN; }
        if (!reliable(socket, 0x11, NULL, 0)) { return NET_EAGAIN; }
        socket->state = socket->state == NET_CLOSE_WAIT ? NET_LAST_ACK : NET_FIN_WAIT; return 0;
    }
    if (socket->state == NET_LISTEN) { for (size_t i = 0; i < NET_SOCKET_MAX; ++i) { if (sockets[i].live && !sockets[i].published && sockets[i].listener == (int)fd) { memset(&sockets[i], 0, sizeof(sockets[i])); } } }
    if (socket->state == NET_LISTEN) { socket->state = NET_CLOSED; }
    return 0;
}
long net_close(uint64_t owner, uint64_t fd) {
    struct socket_state *socket = find_socket(owner, fd); if (socket == NULL) { return NET_EBADF; }
    if (socket->state == NET_LISTEN) { (void)net_shutdown(owner, fd); memset(socket, 0, sizeof(*socket)); return 0; }
    if (socket->remote_port != 0 && socket->state != NET_DATAGRAM && socket->state != NET_CLOSED && socket->state != NET_FAILED && socket->state != NET_TIME_WAIT) { (void)tcp_segment(socket, 0x14, socket->snd_next, NULL, 0); }
    memset(socket, 0, sizeof(*socket)); return 0;
}
void net_release(uint64_t owner) { for (size_t i = 0; i < NET_SOCKET_MAX; ++i) { if (sockets[i].live && sockets[i].owner == owner) { memset(&sockets[i], 0, sizeof(sockets[i])); } } if (ping.live && ping.owner == owner) { memset(&ping, 0, sizeof(ping)); } }
long net_get_status(uint64_t owner, uint64_t fd, struct net_status *status) {
    struct socket_state *socket = find_socket(owner, fd); if (socket == NULL) { return NET_EBADF; }
    *status = (struct net_status){.state = socket->state, .error = socket->error, .remote_ip = socket->remote_ip, .queued = (uint32_t)socket->received_bytes, .local_port = socket->local_port, .remote_port = socket->remote_port}; return 0;
}
static void receive_tcp(uint32_t source, const unsigned char *segment, size_t length) {
    if (length < 20 || transport_checksum(source, NET_LOCAL_IP, 6, segment, length) != 0) { ++statistics.rejected; return; }
    size_t header = (size_t)(segment[12] >> 4) * 4u;
    if (header < 20 || header > length) { ++statistics.rejected; return; }
    uint16_t source_port = get16(segment), destination_port = get16(segment + 2); uint8_t flags = segment[13];
    uint32_t sequence = get32(segment + 4), ack = get32(segment + 8); const unsigned char *payload = segment + header; size_t payload_bytes = length - header;
    struct socket_state *socket = NULL, *listener = NULL;
    for (size_t i = 0; i < NET_SOCKET_MAX; ++i) {
        struct socket_state *candidate = &sockets[i];
        if (!candidate->live || candidate->local_port != destination_port) { continue; }
        if (candidate->state == NET_LISTEN) { listener = candidate; }
        else if (candidate->state != NET_DATAGRAM && candidate->remote_ip == source && candidate->remote_port == source_port) { socket = candidate; break; }
    }
    if (socket == NULL) {
        if (listener == NULL || flags != 2 || payload_bytes != 0) { return; }
        int slot = free_slot(); if (slot < 0) { return; }
        socket = &sockets[slot]; *socket = (struct socket_state){.owner = listener->owner, .live = true, .listener = (int)(listener - sockets), .state = NET_SYN_RECEIVED, .local_port = destination_port, .remote_port = source_port, .remote_ip = source, .rcv_next = sequence + 1, .snd_next = next_sequence, .snd_unacked = next_sequence, .window = get16(segment + 14)};
        next_sequence += 0x10001;
        if (!reliable(socket, 0x12, NULL, 0)) { memset(socket, 0, sizeof(*socket)); } return;
    }
    if (socket->state == NET_SYN_SENT) {
        if ((flags & 0x14u) == 0x14u && ack == socket->snd_next) { socket->state = NET_FAILED; socket->error = (int32_t)NET_ECONNREFUSED; return; }
        if ((flags & 0x12u) != 0x12u || (flags & 5u) != 0 || ack != socket->snd_next || payload_bytes != 0) { return; }
        socket->rcv_next = sequence + 1; socket->snd_unacked = ack; socket->state = NET_ESTABLISHED; socket->window = get16(segment + 14); (void)tcp_segment(socket, 0x10, socket->snd_next, NULL, 0); return;
    }
    if ((flags & 4u) != 0) { if (sequence == socket->rcv_next) { socket->state = NET_FAILED; socket->error = (int32_t)NET_ECONNRESET; } return; }
    if (sequence != socket->rcv_next) { (void)tcp_segment(socket, 0x10, socket->snd_next, NULL, 0); return; }
    if ((flags & 0x10u) == 0 || (int32_t)(ack - socket->snd_next) > 0 || (int32_t)(ack - socket->snd_unacked) < 0) { return; }
    socket->window = get16(segment + 14);
    if (ack == socket->snd_next) { socket->snd_unacked = ack; socket->pending_bytes = 0; }
    if (socket->state == NET_SYN_RECEIVED && ack == socket->snd_next) { socket->state = NET_ESTABLISHED; }
    if (socket->state == NET_LAST_ACK && ack == socket->snd_next) { socket->state = NET_CLOSED; return; }
    if ((flags & 2u) != 0 || socket->fin_received || (socket->state != NET_ESTABLISHED && socket->state != NET_FIN_WAIT && socket->state != NET_CLOSE_WAIT)) { return; }
    if (payload_bytes > RX_CAPACITY - socket->received_bytes) { (void)tcp_segment(socket, 0x10, socket->snd_next, NULL, 0); return; }
    if (payload_bytes != 0) { memcpy(socket->received + socket->received_bytes, payload, payload_bytes); socket->received_bytes += payload_bytes; socket->rcv_next += (uint32_t)payload_bytes; }
    if ((flags & 1u) != 0) { ++socket->rcv_next; socket->fin_received = true; socket->state = socket->state == NET_FIN_WAIT ? NET_TIME_WAIT : NET_CLOSE_WAIT; socket->deadline = now + 200; }
    if (payload_bytes != 0 || (flags & 1u) != 0) { (void)tcp_segment(socket, 0x10, socket->snd_next, NULL, 0); }
}
void net_input(const void *frame_pointer, size_t length) {
    const unsigned char *frame = frame_pointer; ++statistics.received;
    if (frame == NULL || length < 14 || length > 1518 || (memcmp(frame, local_mac, 6) != 0 && memcmp(frame, "\xff\xff\xff\xff\xff\xff", 6) != 0)) { ++statistics.rejected; return; }
    uint16_t type = get16(frame + 12);
    if (type == 0x0806) {
        if (length < 42 || get16(frame + 14) != 1 || get16(frame + 16) != 0x800 || frame[18] != 6 || frame[19] != 4 || (get16(frame + 20) != 1 && get16(frame + 20) != 2) || get32(frame + 38) != NET_LOCAL_IP || memcmp(frame + 6, frame + 22, 6) != 0) { ++statistics.rejected; return; }
        uint32_t source = get32(frame + 28); learn(source, frame + 22); if (get16(frame + 20) == 1) { arp(source, frame + 22, true); } return;
    }
    if (type != 0x0800) { ++statistics.rejected; return; }
    if (length < 34) { ++statistics.rejected; return; }
    const unsigned char *ip = frame + 14; size_t header = (size_t)(ip[0] & 15u) * 4u, total = get16(ip + 2);
    if ((ip[0] >> 4) != 4 || header < 20 || header > length - 14 || total < header || total > length - 14 || total > 1500 || (get16(ip + 6) & 0xbfffu) != 0 || ip[8] == 0 || get32(ip + 16) != NET_LOCAL_IP || !unicast_ip(get32(ip + 12)) || checksum(ip, header) != 0) { ++statistics.rejected; return; }
    uint32_t source = get32(ip + 12); uint32_t neighbor_ip = (source & UINT32_C(0xffffff00)) == (NET_LOCAL_IP & UINT32_C(0xffffff00)) ? source : NET_GATEWAY; learn(neighbor_ip, frame + 6);
    const unsigned char *payload = ip + header; size_t count = total - header;
    if (ip[9] == 1) {
        if (count < 8 || checksum(payload, count) != 0) { ++statistics.rejected; return; }
        if (payload[0] == 8 && payload[1] == 0) { unsigned char reply[1480]; memcpy(reply, payload, count); reply[0] = 0; put16(reply + 2, 0); put16(reply + 2, checksum(reply, count)); (void)send_ip(source, 1, reply, count); }
        else if (payload[0] == 0 && payload[1] == 0 && ping.live && ping.ip == source && get16(payload + 4) == 0xa570 && get16(payload + 6) == ping.tag) { ping.result = 0; }
        return;
    }
    if (ip[9] == 6) { receive_tcp(source, payload, count); return; }
    if (ip[9] == 17) {
        if (count < 8 || get16(payload + 4) != count || (get16(payload + 6) != 0 && transport_checksum(source, NET_LOCAL_IP, 17, payload, count) != 0)) { ++statistics.rejected; return; }
        for (size_t i = 0; i < NET_SOCKET_MAX; ++i) {
            struct socket_state *socket = &sockets[i];
            if (!socket->live || socket->state != NET_DATAGRAM || socket->local_port != get16(payload + 2) || (socket->remote_port != 0 && (socket->remote_ip != source || socket->remote_port != get16(payload)))) { continue; }
            if (socket->datagram_pending || count - 8 > RX_CAPACITY) { return; } memcpy(socket->received, payload + 8, count - 8); socket->received_bytes = count - 8; socket->datagram_pending = true; return;
        } return;
    }
    ++statistics.rejected;
}
void net_tick(uint64_t ticks) {
    now = ticks;
    if (ping.live && ping.result == NET_EAGAIN && now >= ping.deadline) { ping.result = NET_ETIMEDOUT; }
    for (size_t i = 0; i < NET_SOCKET_MAX; ++i) {
        struct socket_state *socket = &sockets[i];
        if (!socket->live) { continue; }
        if (!socket->published && socket->state == NET_FAILED) { memset(socket, 0, sizeof(*socket)); continue; }
        if (socket->state == NET_TIME_WAIT && now >= socket->deadline) { socket->state = NET_CLOSED; }
        if (socket->state == NET_FIN_WAIT && socket->snd_next == socket->snd_unacked && now >= socket->deadline + 500) { socket->state = NET_FAILED; socket->error = (int32_t)NET_ETIMEDOUT; }
        if (socket->snd_next == socket->snd_unacked || socket->state == NET_FAILED || now < socket->deadline) { continue; }
        if (socket->retries == 3) { socket->state = NET_FAILED; socket->error = (int32_t)NET_ETIMEDOUT; if (!socket->published) { memset(socket, 0, sizeof(*socket)); } continue; }
        (void)tcp_segment(socket, socket->pending_flags, socket->snd_unacked, socket->pending, socket->pending_bytes); ++socket->retries; ++statistics.retransmitted; socket->deadline = now + 100;
    }
}
long net_ping(uint64_t owner, uint32_t ip, uint16_t tag, bool query) {
    if (query) { if (!ping.live || ping.owner != owner || ping.ip != ip || ping.tag != tag) { return NET_EINVAL; } return ping.result; }
    if (!unicast_ip(ip)) { return NET_EINVAL; }
    if (ping.live && ping.result == NET_EAGAIN) { return NET_EAGAIN; }
    unsigned char payload[16] = {8}; put16(payload + 4, 0xa570); put16(payload + 6, tag); memcpy(payload + 8, "AgenticOS", 8); put16(payload + 2, checksum(payload, sizeof(payload)));
    if (!send_ip(ip, 1, payload, sizeof(payload))) { return NET_EAGAIN; }
    ping.owner = owner; ping.ip = ip; ping.tag = tag; ping.result = NET_EAGAIN; ping.deadline = now + 300; ping.live = true; return 0;
}
void net_get_stats(struct net_stats *stats) { *stats = statistics; }
