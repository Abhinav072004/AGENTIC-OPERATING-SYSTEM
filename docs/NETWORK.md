# Native network stack

AgenticOS uses its e1000 driver, Ethernet II, ARP, IPv4, ICMP echo, UDP and a
bounded TCP implementation. Configuration is static: `10.0.2.15/24`, gateway
`10.0.2.2`. Optional QEMU user networking can supply that gateway. The default
acceptance image attaches no NIC and remains functional offline.

ARP maintains four expiring neighbors and limits repeated unresolved requests.
IPv4 verifies lengths, version, checksum, destination, source and TTL; fragmented
packets are rejected. ICMP verifies checksums and supports echo requests/replies.
UDP verifies optional IPv4 checksums, connected-peer filtering and datagram bounds.
Zero-length datagrams are distinguished from an empty receive queue.

TCP supports active open and bind/listen/accept, SYN/ACK, ordered byte-stream
receive, acknowledgments, peer window limits, retransmission, FIN/EOF, RST and
bounded timeouts. One segment (up to 512 bytes) is outstanding per socket. Lost
segments retry after 100 ticks, at most three times. Duplicate/out-of-order data
elicits the current ACK and is not delivered twice. Receive windows are bounded
to 4096 bytes. No out-of-order reassembly, congestion control, selective ACK,
window scaling, fragmentation, IPv6, DHCP, DNS or TLS is implemented. This is a
small tested stack, not a general Internet-ready TCP/IP implementation.

Eight sockets are globally available and owned by process IDs. Every socket
operation checks its owner. Syscalls in `kernel/syscall/abi.h` provide socket,
bind, connect, send, receive, status, listen, accept, shutdown, close and ping.
Endpoints carry a network-order numeric IPv4 value (e.g. `NET_IP(10,0,2,2)`),
a host-order port, and a zero reserved field. User buffers are checked and copied;
512 bytes is the per-call transfer limit. Empty queues return EAGAIN; TCP EOF
returns zero. Shutdown initiates graceful TCP FIN; close releases the descriptor
and aborts an unfinished connection. Process cleanup releases all owned sockets.
The stack is polled in bounded batches at syscall entry; callers sleep/retry.

`./scripts/env.sh make test-network` boots native socket tests on BIOS and UEFI.
An independent Python Ethernet peer verifies checksums and the protocol exchange
through real e1000 DMA. It deliberately drops the first SYN and data packet,
sends out-of-order data, initiates a server connection, completes both FIN paths,
refuses a port and ignores another until timeout. Traffic stays on loopback.
Evidence is `build/tests/network-{bios,uefi}`, with packet events, retry counts,
exact QEMU commands, ISO hashes, logs, registers and framebuffer captures.

For a selected ISO, `tools/qemu.py --network user` attaches an emulated e1000
using QEMU's user-mode backend. No inbound host forwarding is enabled. Network
access in AgenticOS is still optional; no credentials are embedded in the image.
