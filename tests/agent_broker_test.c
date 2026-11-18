#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "agent/broker.h"
#include "fs/vfs.h"
#include "security/policy.h"
static struct agent_broker broker;
static struct agent_actor owner = {.pid=10,.uid=1000,.capabilities=CAP_AGENT_CONTROL|CAP_FS_READ|CAP_FS_WRITE|CAP_INSPECT};
static long submit(const char *intent) { struct agent_plan plan; assert(agent_parse_intent(intent,&plan) == 0); return agent_broker_submit(&broker,&owner,&plan); }
static void run(uint64_t id) { assert(agent_broker_approve(&broker,&owner,id,AGENT_APPROVE_PLAN|AGENT_APPROVE_DELETE)==0); while(agent_broker_step(&broker,&owner,id)>0) {} struct agent_job_info info; assert(agent_broker_review(&broker,&owner,id,NULL,&info)==0 && info.state==AGENT_DONE); assert(agent_broker_release(&broker,&owner,id)==0); }
static void check_text(const char *expected) { struct fs_handles files={0}; char buffer[128]={0}; long fd=fs_open(&files,"/home/note",FS_READ); assert(fd>=3 && fs_read(&files,(uint64_t)fd,buffer,127)==(long)strlen(expected) && strcmp(buffer,expected)==0); fs_close_all(&files); }
int main(void) {
    fs_init(); assert(fs_mkdir("/home")==0 && fs_chown("/home",1000)==0 && fs_chmod("/home",0700)==0);
    struct agent_broker_ops ops={.execute=agent_tools_execute}; assert(agent_broker_init(&broker,&ops));
    struct agent_plan plan,copy; assert(agent_parse_intent("write /home/note 'old'; read /home/note",&plan)==0);
    long id=agent_broker_submit(&broker,&owner,&plan); assert(id==1); plan.actions[0].text[0]='X';
    struct fs_info file; assert(fs_stat("/home/note",&file)==FS_ENOENT && agent_broker_step(&broker,&owner,(uint64_t)id)==FS_EPERM);
    struct agent_actor stranger=owner; stranger.pid=11; assert(agent_broker_review(&broker,&stranger,(uint64_t)id,&copy,NULL)==FS_EPERM); stranger=owner; stranger.uid=1001; assert(agent_broker_cancel(&broker,&stranger,(uint64_t)id)==FS_EPERM);
    assert(agent_broker_review(&broker,&owner,(uint64_t)id,&copy,NULL)==0 && strcmp(copy.actions[0].text,"old")==0); copy.actions[0].text[0]='Y'; run((uint64_t)id); check_text("old");
    id=submit("delete /home/note"); assert(id==2 && agent_broker_approve(&broker,&owner,(uint64_t)id,AGENT_APPROVE_PLAN)==FS_EPERM); assert(agent_broker_cancel(&broker,&owner,(uint64_t)id)==0 && agent_broker_step(&broker,&owner,(uint64_t)id)==FS_EPERM); assert(agent_broker_release(&broker,&owner,(uint64_t)id)==0); check_text("old");
    id=submit("write /home/note 'changed'; mkdir /home/never"); assert(agent_broker_approve(&broker,&owner,(uint64_t)id,AGENT_APPROVE_PLAN)==0 && agent_broker_step(&broker,&owner,(uint64_t)id)==1); assert(agent_broker_cancel(&broker,&owner,(uint64_t)id)==0 && agent_broker_step(&broker,&owner,(uint64_t)id)<0 && fs_stat("/home/never",&file)==FS_ENOENT); assert(agent_broker_release(&broker,&owner,(uint64_t)id)==0); check_text("changed");
    id=submit("write /home/note 'revoked'"); assert(agent_broker_approve(&broker,&owner,(uint64_t)id,AGENT_APPROVE_PLAN)==0); struct agent_actor revoked=owner; revoked.capabilities=CAP_AGENT_CONTROL; assert(agent_broker_step(&broker,&revoked,(uint64_t)id)==FS_EPERM); assert(agent_broker_release(&broker,&owner,(uint64_t)id)==0); check_text("changed");
    id=submit("read /home/note"); stranger=owner; stranger.uid=1001; stranger.pid=12; long other=agent_broker_submit(&broker,&stranger,&copy); assert(other>0 && submit("list /home")==-24); assert(agent_broker_approve(&broker,&stranger,(uint64_t)other,AGENT_APPROVE_PLAN)==0 && agent_broker_step(&broker,&stranger,(uint64_t)other)==FS_EACCES); assert(agent_broker_release(&broker,&stranger,(uint64_t)other)==0); run((uint64_t)id);
    /* Fill every RAM block: an atomic save must leave the original intact. */
    static char data[FS_FILE_MAX]; memset(data,'q',sizeof(data));
    for(unsigned int i=0;i<8;++i) { char path[32]; snprintf(path,sizeof(path),"/fill%u",i); size_t length=i==7 ? FS_FILE_MAX-4096 : FS_FILE_MAX; assert(fs_seed(path,data,length)==0); }
    assert(fs_used_blocks()==FS_BLOCK_COUNT); id=submit("write /home/note 'lost'"); assert(agent_broker_approve(&broker,&owner,(uint64_t)id,AGENT_APPROVE_PLAN)==0 && agent_broker_step(&broker,&owner,(uint64_t)id)==FS_ENOSPC); check_text("changed"); assert(agent_broker_release(&broker,&owner,(uint64_t)id)==0);
    for(unsigned int i=0;i<8;++i) { char path[32]; snprintf(path,sizeof(path),"/fill%u",i); assert(fs_unlink(path)==0); }
    for(unsigned int i=0;i<40;++i) { id=submit("list /home"); assert(id>0); run((uint64_t)id); }
    assert(broker.dropped>0); struct agent_audit entry; uint64_t cursor=0; unsigned int records=0; while(agent_broker_audit(&broker,&owner,0,cursor,&entry)==1) { assert(entry.sequence>cursor && entry.owner==owner.pid && entry.uid==owner.uid); cursor=entry.sequence; ++records; } assert(records==AGENT_AUDIT_MAX);
    stranger=owner; stranger.pid=99; assert(agent_broker_audit(&broker,&stranger,0,0,&entry)==0); assert(agent_broker_review(&broker,&owner,1,NULL,NULL)==-3);
    broker.next_id=UINT64_MAX; assert(submit("list /home")==-75); broker.next_id=200; broker.next_sequence=UINT64_MAX; assert(submit("list /home")==-75);
    assert(fs_unlink("/home/note")==0 && fs_used_blocks()==0);
    puts("PASS: immutable review, explicit consent, PID/UID/capability checks, revocation, partial cancellation, atomic quota failure, bounded audit privacy/wrap and stale ID rejection"); return 0;
}
