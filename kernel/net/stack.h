#ifndef AGENTICOS_NET_STACK_H
#define AGENTICOS_NET_STACK_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define NET_IP(a,b,c,d) (((uint32_t)(a)<<24)|((uint32_t)(b)<<16)|((uint32_t)(c)<<8)|(uint32_t)(d))
#define NET_LOCAL_IP NET_IP(10,0,2,15)
#define NET_GATEWAY NET_IP(10,0,2,2)
#define NET_SOCKET_MAX 8u
#define NET_IO_MAX 512u
#define NET_UDP 1u
#define NET_TCP 2u
#define NET_CLOSED 0u
#define NET_DATAGRAM 1u
#define NET_LISTEN 2u
#define NET_SYN_SENT 3u
#define NET_SYN_RECEIVED 4u
#define NET_ESTABLISHED 5u
#define NET_CLOSE_WAIT 6u
#define NET_FIN_WAIT 7u
#define NET_LAST_ACK 8u
#define NET_TIME_WAIT 9u
#define NET_FAILED 10u
#define NET_EAGAIN (-11L)
#define NET_EINVAL (-22L)
#define NET_EBADF (-9L)
#define NET_EADDRINUSE (-98L)
#define NET_ENETDOWN (-100L)
#define NET_ECONNRESET (-104L)
#define NET_ETIMEDOUT (-110L)
#define NET_ECONNREFUSED (-111L)
struct net_endpoint { uint32_t ip; uint16_t port, reserved; };
struct net_status { uint32_t state; int32_t error; uint32_t remote_ip, queued; uint16_t local_port, remote_port; uint32_t reserved; };
struct net_stats { uint64_t received, sent, rejected, retransmitted; };
struct net_ops { void *context; bool (*send)(void *context, const void *frame, size_t length); };
void net_stack_init(const struct net_ops *ops, const uint8_t mac[6]);
void net_input(const void *frame, size_t length);
void net_tick(uint64_t ticks);
long net_socket(uint64_t owner, unsigned int type);
long net_bind(uint64_t owner, uint64_t fd, uint16_t port);
long net_connect(uint64_t owner, uint64_t fd, struct net_endpoint peer);
long net_send(uint64_t owner, uint64_t fd, const void *buffer, size_t length);
long net_receive(uint64_t owner, uint64_t fd, void *buffer, size_t capacity);
long net_shutdown(uint64_t owner, uint64_t fd);
long net_close(uint64_t owner, uint64_t fd);
void net_release(uint64_t owner);
long net_listen(uint64_t owner, uint64_t fd);
long net_accept(uint64_t owner, uint64_t fd);
long net_get_status(uint64_t owner, uint64_t fd, struct net_status *status);
long net_ping(uint64_t owner, uint32_t ip, uint16_t tag, bool query);
void net_get_stats(struct net_stats *stats);
void network_init(void);
void network_poll(void);
void network_self_test(void);
#endif
