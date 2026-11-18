#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "agent/inference.h"
int main(void) {
    static const struct { const char *text; uint32_t tool; } heldout[]={
        {"please make a new folder",1},{"I would like a directory",1},{"put my words into a note",2},{"please store text",2},
        {"show me the contents of this note",3},{"please view a document",3},{"browse the files here",4},{"show me directory entries",4},
        {"please relocate my document",5},{"rename this file",5},{"please erase my note",6},{"discard a document",6},{"how much free memory",7},{"report resource usage",7}};
    unsigned int accepted=0; for(size_t i=0;i<sizeof(heldout)/sizeof(heldout[0]);++i) { uint32_t tool=0; int32_t margin=0; long result=agent_classify(heldout[i].text,&tool,&margin); if(result==0) { assert(tool==heldout[i].tool && margin>=180); ++accepted; } else { assert(margin<180); } } assert(accepted==11);
    struct agent_plan plan; assert(agent_infer_offline("Please put 'hello world' into /home/note",&plan)==0 && plan.count==1 && plan.actions[0].tool==AGENT_WRITE && strcmp(plan.actions[0].text,"hello world")==0);
    assert(agent_infer_offline("I would like a directory at /home/notes",&plan)==0 && plan.actions[0].tool==AGENT_MKDIR);
    assert(agent_infer_offline("show me the files in /home",&plan)==0 && plan.actions[0].tool==AGENT_LIST);
    assert(agent_infer_offline("how much free memory",&plan)==0 && plan.actions[0].tool==AGENT_SYSTEM);
    const char *bad[]={"put text into /home/a", "make a folder", "please erase /etc/passwd", "please put 'hi' into /home/a then delete /home/b", "unknown gizmo /home/a", "put 'x' into /home/../etc/x"};
    for(size_t i=0;i<sizeof(bad)/sizeof(bad[0]);++i) { memset(&plan,0x5a,sizeof(plan)); assert(agent_infer_offline(bad[i],&plan)<0 && plan.count==0); }
    unsigned int random=5; char input[512]; for(unsigned int test=0;test<20000;++test) { size_t length=test%511; for(size_t i=0;i<length;++i) { random=random*1664525u+1013904223u; input[i]="ab home/;'_\"t\\ "[random%15]; } input[length]=0; long result=agent_infer_offline(input,&plan); assert(result==0 ? agent_validate_plan(&plan)==0 : plan.count==0); }
    puts("PASS: actual integer inference on 14 held-out labels, bounded argument extraction, uncertainty/scope/compound rejection and 20,000 sanitized requests"); return 0;
}
