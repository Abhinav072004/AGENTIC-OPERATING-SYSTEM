#include "inference.h"
#include <stdbool.h>
#include "runtime/memory.h"
#include "intent_model.h"
static bool letter(char c) { return (c>='a' && c<='z') || (c>='A' && c<='Z'); }
static char lower(char c) { return c>='A' && c<='Z' ? (char)(c+('a'-'A')) : c; }
static bool equal(const char *a, const char *b) { size_t i=0; while(a[i]!=0 && a[i]==b[i]) { ++i; } return a[i]==b[i]; }
long agent_classify(const char *text, uint32_t *tool, int32_t *margin) {
    if(text==NULL || tool==NULL || margin==NULL) { return -22; } int32_t scores[7]={0}; unsigned int known=0; size_t position=0;
    while(position<AGENT_INTENT_MAX && text[position]!=0) {
        if(text[position]=='/' || text[position]=='\'' || text[position]=='"') {
            char quote=text[position]=='/' ? 0 : text[position]; ++position;
            while(position<AGENT_INTENT_MAX && text[position]!=0 && (quote!=0 ? text[position]!=quote : text[position]!=' ')) { if(quote!=0 && text[position]=='\\' && position+1<AGENT_INTENT_MAX && text[position+1]!=0) { ++position; } ++position; }
            if(quote!=0 && position<AGENT_INTENT_MAX && text[position]==quote) { ++position; } continue;
        }
        if(!letter(text[position])) { ++position; continue; }
        char word[32]={0}; size_t length=0; while(position<AGENT_INTENT_MAX && letter(text[position])) { if(length+1<sizeof(word)) { word[length++]=lower(text[position]); } ++position; }
        for(size_t i=0;i<sizeof(intent_model)/sizeof(intent_model[0]);++i) { if(equal(word,intent_model[i].word)) { for(size_t label=0;label<7;++label) { scores[label]+=intent_model[i].score[label]; } ++known; break; } }
    }
    if(position==AGENT_INTENT_MAX || known==0) { return -22; }
    size_t best=0; for(size_t i=1;i<7;++i) { if(scores[i]>scores[best]) { best=i; } }
    int32_t second=INT32_MIN; for(size_t i=0;i<7;++i) { if(i!=best && scores[i]>second) { second=scores[i]; } }
    *margin=scores[best]-second; if(*margin<180) { return -22; } *tool=(uint32_t)best+1; return 0;
}
long agent_infer_offline(const char *text, struct agent_plan *plan) {
    if(plan==NULL) { return -22; } long explicit_result=agent_parse_intent(text,plan); if(explicit_result==0) { return 0; }
    uint32_t tool; int32_t margin; if(agent_classify(text,&tool,&margin)<0) { return explicit_result; }
    struct agent_plan candidate={.abi=AGENT_PLAN_ABI,.count=1}; struct agent_action *action=&candidate.actions[0]; action->tool=tool;
    size_t position=0; unsigned int paths=0, quotes=0;
    while(position<AGENT_INTENT_MAX && text[position]!=0) {
        char c=text[position];
        if(c==';' || c=='\n' || c=='\r') { return -22; }
        if(c=='\'' || c=='"') {
            if(tool!=AGENT_WRITE || ++quotes!=1) { return -22; } ++position; size_t length=0;
            while(position<AGENT_INTENT_MAX && text[position]!=0 && text[position]!=c) {
                char value=text[position++]; if(value=='\\') { if(position==AGENT_INTENT_MAX || text[position]==0) { return -22; } value=text[position++]; if(value=='n') { value='\n'; } else if(value=='t') { value='\t'; } else if(value!='\\' && value!='\'' && value!='"') { return -22; } }
                if(length+1>=AGENT_FIELD_MAX) { return -7; } action->text[length++]=value;
            }
            if(position==AGENT_INTENT_MAX || text[position]!=c) { return -22; } ++position; continue;
        }
        if(c=='/') {
            if(paths==2) { return -22; } char raw[128]={0}; size_t length=0;
            while(position<AGENT_INTENT_MAX && text[position]!=0 && text[position]!=' ' && text[position]!='\t') { if(length+1>=sizeof(raw)) { return -7; } raw[length++]=text[position++]; }
            if(agent_path(raw,paths==0 ? action->path : action->other)<0) { return -22; } ++paths; continue;
        }
        if(letter(c)) {
            char word[32]={0}; size_t length=0; while(position<AGENT_INTENT_MAX && letter(text[position])) { if(length+1<sizeof(word)) { word[length++]=lower(text[position]); } ++position; }
            if(equal(word,"and") || equal(word,"then")) { return -22; } continue;
        }
        if((unsigned char)c<32 || (unsigned char)c>126) { return -22; } ++position;
    }
    if(position==AGENT_INTENT_MAX || (tool==AGENT_SYSTEM ? paths!=0 : paths!=(tool==AGENT_MOVE ? 2u : 1u)) || (tool==AGENT_WRITE ? quotes!=1 : quotes!=0)) { return -22; }
    long result=agent_validate_plan(&candidate); if(result==0) { *plan=candidate; } return result;
}
