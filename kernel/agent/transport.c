#include "transport.h"
#include "net/stack.h"
#include "runtime/memory.h"
static uint32_t get32(const unsigned char *p) { return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24); }
static void put32(unsigned char *p, uint32_t value) { for(unsigned int i=0;i<4;++i) { p[i]=(unsigned char)(value>>(i*8)); } }
void agent_transport_init(struct agent_transport *transport, uint64_t owner, const char *intent) {
    memset(transport,0,sizeof(*transport)); transport->fd=-1; transport->owner=owner; size_t n=0; while(n+1<AGENT_INTENT_MAX && intent[n]!=0) { ++n; }
    memcpy(transport->request,"AGI1",4); put32(transport->request+4,(uint32_t)n); memcpy(transport->request+8,intent,n); transport->request_length=8+n; transport->length=12;
}
void agent_transport_close(struct agent_transport *transport) { if(transport->fd>=0) { (void)net_close(transport->owner,(uint64_t)transport->fd); transport->fd=-1; } }
long agent_transport_poll(struct agent_transport *transport, struct agent_plan *plan) {
    if(transport->fd<0) { transport->fd=net_socket(transport->owner,NET_TCP); if(transport->fd<0) { return transport->fd; } }
    uint64_t fd=(uint64_t)transport->fd;
    struct net_status status; long result=net_get_status(transport->owner,fd,&status); if(result<0) { return result; } if(status.error!=0) { return status.error; }
    if(!transport->connected) {
        if(status.state==NET_CLOSED) { struct net_endpoint peer={.ip=NET_GATEWAY,.port=CONFIG_AGENT_BRIDGE_PORT}; result=net_connect(transport->owner,fd,peer); return result<0 ? result : NET_EAGAIN; }
        if(status.state!=NET_ESTABLISHED) { return NET_EAGAIN; } transport->connected=true;
    }
    if(transport->sent<transport->request_length) { size_t n=transport->request_length-transport->sent; if(n>NET_IO_MAX) { n=NET_IO_MAX; } result=net_send(transport->owner,fd,transport->request+transport->sent,n); if(result<0) { return result; } transport->sent+=(size_t)result; return NET_EAGAIN; }
    size_t room=transport->length-transport->received; if(room>NET_IO_MAX) { room=NET_IO_MAX; }
    result=net_receive(transport->owner,fd,transport->reply+transport->received,room); if(result<0) { return result; } if(result==0) { return -5; } transport->received+=(size_t)result;
    if(transport->received==12 && transport->length==12) {
        if(memcmp(transport->reply,"AGP1",4)!=0) { return -71; }
        int32_t error=(int32_t)get32(transport->reply+4); uint32_t length=get32(transport->reply+8);
        if(error<0) { return length==0 ? error : -71; } if(error!=0) { return -71; }
        if(length!=sizeof(struct agent_plan)) { return -7; } transport->length=12+length;
    }
    if(transport->received<transport->length) { return NET_EAGAIN; }
    memcpy(plan,transport->reply+12,sizeof(*plan)); return agent_validate_plan(plan);
}
