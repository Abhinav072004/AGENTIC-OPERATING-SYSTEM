#include "agent_panel.h"
#include "syscall.h"
#include "text.h"
#include "gui/abi.h"
#include "drivers/input.h"
#include "runtime/memory.h"
#include "fs/vfs.h"
struct agent_panel agent_panel;
static bool drawing_failed;
static void trace(const char *message) { (void)user_write(1,message,text_length(message)); }
static void message(long error) { text_copy(agent_panel.message,agent_error_label(error),sizeof(agent_panel.message)); }
static bool reviewed(void) { return agent_panel.has_plan && agent_panel.seen==((1u<<agent_panel.plan.count)-1u); }
void agent_panel_init(void) { memset(&agent_panel,0,sizeof(agent_panel)); text_copy(agent_panel.message,"Type an intent, then Plan. Review expires after 60 seconds.",sizeof(agent_panel.message)); }
bool agent_panel_tick(void) {
    if(agent_panel.id==0) { return false; } struct agent_job_info next; long result=agent_status(agent_panel.id,&next); if(result<0) { message(result); return false; }
    bool changed=memcmp(&next,&agent_panel.info,sizeof(next))!=0; agent_panel.info=next;
    if(next.state==AGENT_REVIEW && !agent_panel.has_plan) { result=agent_review(agent_panel.id,&agent_panel.plan); if(result<0) { message(result); } else { agent_panel.has_plan=true; agent_panel.action=0; agent_panel.seen=0; text_copy(agent_panel.message,"Visit every action, then Approve. Writes replace existing files.",sizeof(agent_panel.message)); trace("AGENT GUI review"); changed=true; } }
    if(changed && next.state>=AGENT_DONE) { message(next.error); if(next.state==AGENT_DONE) { agent_panel.action=next.total-1u; agent_panel.audit_mode=false; text_copy(agent_panel.message,"Completed. View the last action for its result preview.",sizeof(agent_panel.message)); } trace(next.state==AGENT_DONE ? "AGENT GUI done" : next.state==AGENT_CANCELLED ? "AGENT GUI cancelled" : "AGENT GUI failed"); }
    return changed;
}
static void start(void) {
    if(agent_panel.id!=0) { (void)agent_panel_tick(); if(agent_panel.info.state<AGENT_DONE) { message(-11); return; } long result=agent_release(agent_panel.id); if(result<0) { message(result); return; } agent_panel.id=0; }
    long result=agent_begin(agent_panel.intent,agent_panel.backend); if(result<0) { message(result); return; }
    agent_panel.id=(uint64_t)result; agent_panel.has_plan=false; agent_panel.deletion=false; agent_panel.audit_mode=false; agent_panel.action=agent_panel.seen=0; memset(&agent_panel.info,0,sizeof(agent_panel.info)); memset(&agent_panel.audit,0,sizeof(agent_panel.audit)); message(0); trace("AGENT GUI planning"); (void)agent_panel_tick();
}
void agent_panel_key(uint32_t key, uint32_t modifiers) {
    if((modifiers & INPUT_MOD_CTRL)!=0 && (key=='u' || key=='U')) { agent_panel.intent[0]=0; agent_panel.length=0; }
    else if(key=='\n') { start(); }
    else if(key=='\b') { if(agent_panel.length!=0) { agent_panel.intent[--agent_panel.length]=0; } }
    else if(key>=32 && key<=126 && agent_panel.length+1<sizeof(agent_panel.intent)) { agent_panel.intent[agent_panel.length++]=(char)key; agent_panel.intent[agent_panel.length]=0; }
}
void agent_panel_click(int x, int y) {
    if(y>=104 && y<128) { if(agent_panel.id!=0 && agent_panel.info.state<AGENT_DONE) { message(-11); return; } if(x>=12 && x<144) { agent_panel.backend=AGENT_BACKEND_OFFLINE; } else if(x>=152 && x<300) { agent_panel.backend=AGENT_BACKEND_MODEL; } return; }
    if(y>=138 && y<162) { if(x>=12 && x<84) { start(); } else if(agent_panel.has_plan && x>=96 && x<168) { agent_panel.audit_mode=false; if(agent_panel.action>0) { --agent_panel.action; } } else if(agent_panel.has_plan && x>=180 && x<252) { agent_panel.audit_mode=false; if(agent_panel.action+1<agent_panel.plan.count) { ++agent_panel.action; } } return; }
    if(y>=376 && y<400 && x>=12 && x<250 && agent_panel.info.destructive!=0 && agent_panel.info.state==AGENT_REVIEW) { agent_panel.deletion=!agent_panel.deletion; return; }
    if(y>=408 && y<432 && agent_panel.id!=0) {
        long result=0;
        if(x>=12 && x<112) { if(!reviewed() || (agent_panel.info.destructive!=0 && !agent_panel.deletion)) { text_copy(agent_panel.message,"Review every action and acknowledge deletion first.",sizeof(agent_panel.message)); return; } result=agent_approve(agent_panel.id,agent_panel.deletion); if(result==0) { trace("AGENT GUI approved"); } }
        else if(x>=124 && x<224) { result=agent_cancel(agent_panel.id); }
        else if(x>=236 && x<336) { result=agent_release(agent_panel.id); if(result==0) { agent_panel.id=0; agent_panel.has_plan=false; memset(&agent_panel.info,0,sizeof(agent_panel.info)); trace("AGENT GUI released"); } }
        else if(x>=348 && x<448) { agent_panel.audit_mode=true; result=user_call(SYS_AGENT_AUDIT,agent_panel.id,(uintptr_t)&agent_panel.audit,agent_panel.audit.sequence); if(result==0) { agent_panel.audit.sequence=0; text_copy(agent_panel.message,"End of retained audit; click Audit to restart.",sizeof(agent_panel.message)); return; } if(result==1) { trace("AGENT GUI audit"); result=0; } }
        message(result); (void)agent_panel_tick();
    }
}
void agent_panel_close(void) { if(agent_panel.id!=0) { (void)agent_panel_tick(); if(agent_panel.info.state<AGENT_DONE) { (void)agent_cancel(agent_panel.id); (void)agent_panel_tick(); } } }
static void rect(int x,int y,int width,int height,uint32_t color) { struct gui_draw draw={.operation=GUI_RECT,.x=x,.y=y,.width=width,.height=height,.color=color}; if(user_call(SYS_GUI_DRAW,(uintptr_t)&draw,0,0)!=0) { drawing_failed=true; } }
static void label(int x,int y,const char *text,uint32_t color) { size_t offset=0; do { struct gui_draw draw={.operation=GUI_TEXT,.x=x+(int)offset*6,.y=y,.color=color}; size_t n=0; while(n+1<sizeof(draw.text) && text[offset+n]!=0) { draw.text[n]=text[offset+n]; ++n; } if(user_call(SYS_GUI_DRAW,(uintptr_t)&draw,0,0)!=0) { drawing_failed=true; } offset+=n; } while(text[offset]!=0); }
static void button(int x,int y,int width,const char *text,bool active) { rect(x,y,width,24,active ? 0x285da3 : 0x2b3b52); label(x+8,y+8,text,0xffffff); }
static void lines(int x,int y,const char *text,unsigned int rows,bool escape) { size_t offset=0; char escaped[259]; if(escape) { size_t n=0; escaped[n++]='"'; for(size_t i=0;text[i]!=0 && n+2<sizeof(escaped);++i) { char c=text[i]; if(c=='\n' || c=='\t' || c=='\\' || c=='"') { escaped[n++]='\\'; c=c=='\n' ? 'n' : c=='\t' ? 't' : c; } escaped[n++]=c; } escaped[n++]='"'; escaped[n]=0; text=escaped; } for(unsigned int row=0;row<rows && text[offset]!=0;++row) { char line[85]; size_t count=0; while(count<84 && text[offset]!=0) { line[count++]=text[offset++]; } line[count]=0; label(x,y+(int)row*12,line,0xdce8f5); } }
bool agent_panel_draw(int x, int y) {
    drawing_failed=false; char number[24], status[84]="Job "; size_t n=text_length(status); n+=text_number(status+n,(int64_t)agent_panel.id); text_copy(status+n," / ",sizeof(status)-n); n=text_length(status); text_copy(status+n,agent_state_label(agent_panel.info.state),sizeof(status)-n); label(x+12,y+34,"Intent (Ctrl+U clears; Enter plans)",0x9dc9f4);
    rect(x+10,y+48,540,44,0x0b1422); size_t first=agent_panel.length>168 ? agent_panel.length-168 : 0; lines(x+14,y+56,agent_panel.intent+first,2,false);
    button(x+12,y+104,132,"Offline",agent_panel.backend==AGENT_BACKEND_OFFLINE); button(x+152,y+104,148,"Model bridge",agent_panel.backend==AGENT_BACKEND_MODEL);
    button(x+12,y+138,72,"Plan",false); button(x+96,y+138,72,"Prev",false); button(x+180,y+138,72,"Next",false); label(x+272,y+146,agent_panel.id==0 ? "No active request" : status,0x9dc9f4);
    text_copy(status,"Completed ",sizeof(status)); n=text_length(status); n+=text_number(status+n,agent_panel.info.completed); status[n++]='/'; n+=text_number(status+n,agent_panel.info.total); status[n]=0; label(x+12,y+178,status,0x9dc9f4);
    if(agent_panel.audit_mode && agent_panel.audit.sequence!=0) {
        label(x+12,y+202,"Audit event (metadata only)",0xffffff); (void)text_number(number,(int64_t)agent_panel.audit.sequence); label(x+12,y+220,number,0x9dc9f4); label(x+90,y+220,agent_tool_label(agent_panel.audit.tool),0xffffff); label(x+220,y+220,agent_audit_label(agent_panel.audit.event),0x9dc9f4); lines(x+12,y+244,agent_panel.audit.path,4,true); lines(x+12,y+298,agent_panel.audit.other,4,true); (void)text_number(number,agent_panel.audit.status); label(x+12,y+358,number,0x9dc9f4);
    } else if(agent_panel.has_plan) {
        const struct agent_action *action=&agent_panel.plan.actions[agent_panel.action]; agent_panel.seen|=1u<<agent_panel.action;
        (void)text_number(number,agent_panel.action+1u); label(x+12,y+202,"Action",0x9dc9f4); label(x+60,y+202,number,0xffffff); label(x+100,y+202,agent_tool_label(action->tool),0xffffff);
        label(x+12,y+220,"Path",0x9dc9f4); lines(x+54,y+220,action->path,4,true);
        label(x+12,y+270,"To",0x9dc9f4); lines(x+54,y+270,action->other,4,true);
        bool result=agent_panel.info.state==AGENT_DONE && agent_panel.action+1==agent_panel.info.total; label(x+12,y+320,result ? "Result" : "Text",0x9dc9f4); lines(x+54,y+320,result ? agent_panel.info.result : action->text,4,true);
    } else { label(x+12,y+220,"Plans stay here for review before any action runs.",0xb8cee7); }
    if(agent_panel.info.destructive!=0) { button(x+12,y+376,238,agent_panel.deletion ? "[x] Allow deletion" : "[ ] Allow deletion",agent_panel.deletion); }
    else { label(x+12,y+384,"Writes replace files; cancellation keeps completed work.",0xb8cee7); }
    button(x+12,y+408,100,"Approve",reviewed() && agent_panel.info.state==AGENT_REVIEW); button(x+124,y+408,100,"Cancel",false); button(x+236,y+408,100,"Release",false); button(x+348,y+408,100,"Audit",agent_panel.audit_mode);
    lines(x+12,y+448,agent_panel.message,2,false); return !drawing_failed;
}
#define CHECK(t) do { if(!(t)) { return __LINE__; } } while(0)
int agent_panel_acceptance(void) {
    agent_panel_init(); text_copy(agent_panel.intent,"mkdir /home/flow-check; write /home/flow-check/note 'reviewed workflow'; read /home/flow-check/note",sizeof(agent_panel.intent)); agent_panel.length=text_length(agent_panel.intent); start(); CHECK(agent_panel.id!=0);
    struct agent_job_info info; CHECK(agent_wait(agent_panel.id,true,&info)==0 && info.state==AGENT_REVIEW); (void)agent_panel_tick(); CHECK(agent_panel_draw(120,65)); CHECK(!reviewed()); agent_panel_click(30,416); CHECK(agent_status(agent_panel.id,&info)>0 && info.state==AGENT_REVIEW);
    for(unsigned int i=1;i<3;++i) { agent_panel_click(190,146); CHECK(agent_panel_draw(120,65)); } CHECK(reviewed()); agent_panel_click(30,416); CHECK(agent_wait(agent_panel.id,false,&info)==0 && info.state==AGENT_DONE && info.completed==3 && text_equal(info.result,"reviewed workflow")); (void)agent_panel_tick();
    text_copy(agent_panel.intent,"delete /home/flow-check/note; delete /home/flow-check",sizeof(agent_panel.intent)); start(); CHECK(agent_wait(agent_panel.id,true,&info)==0 && info.state==AGENT_REVIEW); (void)agent_panel_tick(); CHECK(agent_panel_draw(120,65)); agent_panel_click(190,146); CHECK(agent_panel_draw(120,65)); agent_panel_click(30,416); CHECK(agent_status(agent_panel.id,&info)>0 && info.state==AGENT_REVIEW); agent_panel_click(30,384); agent_panel_click(30,416); CHECK(agent_wait(agent_panel.id,false,&info)==0 && info.state==AGENT_DONE); CHECK(agent_release(agent_panel.id)==0); agent_panel_init(); return 0;
}
