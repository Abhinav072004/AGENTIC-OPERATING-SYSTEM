#include "plan.h"
#include "runtime/memory.h"
#include "security/policy.h"
#include "fs/vfs.h"
static size_t length(const char *text, size_t limit) { size_t i = 0; while (i < limit && text[i] != 0) { ++i; } return i; }
static bool equal(const char *a, const char *b) { size_t i = 0; while (a[i] != 0 && a[i] == b[i]) { ++i; } return a[i] == b[i]; }
static bool word(const char *a, const char *b) { size_t i = 0; while (a[i] != 0 && b[i] != 0) { char c = a[i]; if (c >= 'A' && c <= 'Z') { c = (char)(c-'A'+'a'); } if (c != b[i]) { return false; } ++i; } return a[i] == b[i]; }
static bool field_valid(const char *field, bool text) {
    size_t end = length(field,AGENT_FIELD_MAX); if (end == AGENT_FIELD_MAX) { return false; }
    for (size_t i = 0; i < AGENT_FIELD_MAX; ++i) { unsigned char c = (unsigned char)field[i]; if (i > end && c != 0) { return false; } if (i < end && (c < 32 || c > 126) && !(text && (c == '\n' || c == '\t'))) { return false; } } return true;
}
long agent_path(const char *input, char output[AGENT_FIELD_MAX]) {
    if (input == NULL || output == NULL || input[0] != '/') { return FS_EINVAL; }
    size_t size = length(input,AGENT_FIELD_MAX); if (size == AGENT_FIELD_MAX) { return FS_ENAMETOOLONG; }
    char canonical[AGENT_FIELD_MAX] = {0}; size_t read = 0, write = 1; canonical[0] = '/';
    while (read < size) {
        while (input[read] == '/') { ++read; } size_t start = read;
        while (input[read] != 0 && input[read] != '/') { unsigned char c = (unsigned char)input[read]; if (c < 32 || c > 126) { return FS_EINVAL; } ++read; }
        size_t count = read-start; if (count == 0 || (count == 1 && input[start] == '.')) { continue; }
        if (count == 2 && input[start] == '.' && input[start+1] == '.') { return FS_EACCES; }
        if (count > FS_NAME_MAX || write+count+(write>1 ? 1u : 0u) >= sizeof(canonical)) { return FS_ENAMETOOLONG; }
        if (write > 1) { canonical[write++] = '/'; } memcpy(canonical+write,input+start,count); write += count;
    }
    if (!equal(canonical,"/home") && (write < 6 || memcmp(canonical,"/home/",6) != 0)) { return FS_EACCES; }
    memcpy(output,canonical,sizeof(canonical)); return 0;
}
const char *agent_tool_name(uint32_t tool) {
    static const char *const names[] = {"invalid","mkdir","write","read","list","move","delete","system"};
    return tool >= AGENT_MKDIR && tool <= AGENT_SYSTEM ? names[tool] : names[0];
}
long agent_validate_plan(const struct agent_plan *plan) {
    if (plan == NULL || plan->abi != AGENT_PLAN_ABI || plan->count == 0 || plan->count > AGENT_MAX_ACTIONS) { return FS_EINVAL; }
    for (size_t i = 0; i < AGENT_MAX_ACTIONS; ++i) {
        const struct agent_action *action = &plan->actions[i];
        if (i >= plan->count) { const unsigned char *bytes = (const void *)action; for (size_t j = 0; j < sizeof(*action); ++j) { if (bytes[j] != 0) { return FS_EINVAL; } } continue; }
        if (action->tool < AGENT_MKDIR || action->tool > AGENT_SYSTEM || action->flags != 0 || !field_valid(action->path,false) || !field_valid(action->other,false) || !field_valid(action->text,true)) { return FS_EINVAL; }
        if (action->tool == AGENT_SYSTEM) { if (action->path[0] != 0 || action->other[0] != 0 || action->text[0] != 0) { return FS_EINVAL; } continue; }
        char canonical[AGENT_FIELD_MAX]; long result = agent_path(action->path,canonical);
        if (result < 0) { return result; } if (!equal(canonical,action->path)) { return FS_EINVAL; }
        bool read = action->tool == AGENT_READ || action->tool == AGENT_LIST;
        if (!read && equal(action->path,"/home")) { return FS_EACCES; }
        if (action->tool == AGENT_MOVE) { result = agent_path(action->other,canonical); if (result < 0) { return result; } if (!equal(canonical,action->other) || equal(action->other,"/home")) { return FS_EINVAL; } }
        else if (action->other[0] != 0) { return FS_EINVAL; }
        if (action->tool != AGENT_WRITE && action->text[0] != 0) { return FS_EINVAL; }
    }
    return 0;
}
uint64_t agent_plan_permissions(const struct agent_plan *plan) {
    uint64_t permissions = 0;
    if (agent_validate_plan(plan) != 0) { return UINT64_MAX; }
    for (uint32_t i = 0; i < plan->count; ++i) { uint32_t tool = plan->actions[i].tool; permissions |= tool == AGENT_SYSTEM ? CAP_INSPECT : tool == AGENT_READ || tool == AGENT_LIST ? CAP_FS_READ : CAP_FS_WRITE; }
    return permissions;
}
struct reader { const char *input; size_t position; bool quoted, separator; };
static long token(struct reader *reader, char output[128]) {
    memset(output,0,128); reader->quoted = reader->separator = false;
    const char *input = reader->input; size_t position = reader->position, write = 0;
    while (input[position] == ' ' || input[position] == '\t' || input[position] == '\n' || input[position] == '\r') { ++position; }
    if (input[position] == 0) { reader->position = position; return 0; }
    if (input[position] == ';') { output[0] = ';'; reader->position = position+1; reader->separator = true; return 1; }
    char quote = 0;
    while (input[position] != 0) {
        char c = input[position];
        if (quote == 0 && (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == ';')) { break; }
        ++position;
        if (c == '\\' && quote != '\'') { if (input[position] == 0) { return FS_EINVAL; } c = input[position++]; if (c == 'n') { c = '\n'; } else if (c == 't') { c = '\t'; } }
        else if ((c == '\'' || c == '"') && (quote == 0 || quote == c)) { quote = quote == 0 ? c : 0; reader->quoted = true; continue; }
        if (write+1 >= 128) { return FS_ENAMETOOLONG; } output[write++] = c;
    }
    if (quote != 0) { return FS_EINVAL; } reader->position = position; return 1;
}
static long argument(struct reader *reader, char output[128]) { long result = token(reader,output); return result == 1 && !reader->separator ? 0 : result < 0 ? result : FS_EINVAL; }
long agent_parse_intent(const char *intent, struct agent_plan *plan) {
    if (plan == NULL) { return FS_EINVAL; } memset(plan,0,sizeof(*plan));
    if (intent == NULL) { return FS_EINVAL; } size_t size = length(intent,AGENT_INTENT_MAX);
    if (size == 0 || size == AGENT_INTENT_MAX) { return size == 0 ? FS_EINVAL : -7; }
    for (size_t i = 0; i < size; ++i) { unsigned char c = (unsigned char)intent[i]; if ((c < 32 && c != '\n' && c != '\r' && c != '\t') || c > 126) { return FS_EINVAL; } }
    struct agent_plan candidate = {.abi = AGENT_PLAN_ABI}; struct reader reader = {.input = intent};
    for (;;) {
        if (candidate.count == AGENT_MAX_ACTIONS) { return -7; }
        char verb[128], value[128]; long result = argument(&reader,verb); if (result < 0) { return result; }
        if (word(verb,"please")) { result = argument(&reader,verb); if (result < 0) { return result; } }
        struct agent_action *action = &candidate.actions[candidate.count];
        if (word(verb,"mkdir") || word(verb,"create") || word(verb,"make")) { action->tool = AGENT_MKDIR; }
        else if (word(verb,"write")) { action->tool = AGENT_WRITE; }
        else if (word(verb,"read") || word(verb,"show")) { action->tool = AGENT_READ; }
        else if (word(verb,"list") || word(verb,"ls")) { action->tool = AGENT_LIST; }
        else if (word(verb,"move") || word(verb,"rename")) { action->tool = AGENT_MOVE; }
        else if (word(verb,"delete") || word(verb,"remove")) { action->tool = AGENT_DELETE; }
        else if (word(verb,"system") || word(verb,"status")) { action->tool = AGENT_SYSTEM; }
        else { return FS_EINVAL; }
        if (action->tool != AGENT_SYSTEM) {
            result = argument(&reader,value); if (result < 0) { return result; }
            if (word(value,"folder") || word(value,"directory") || word(value,"file")) { result = argument(&reader,value); if (result < 0) { return result; } }
            if (word(verb,"show") && word(value,"system")) { action->tool = AGENT_SYSTEM; }
            else { result = agent_path(value,action->path); if (result < 0) { return result; } }
        }
        if (action->tool == AGENT_WRITE || action->tool == AGENT_MOVE) {
            result = argument(&reader,value); if (result < 0) { return result; }
            if ((action->tool == AGENT_WRITE && word(value,"with")) || (action->tool == AGENT_MOVE && word(value,"to"))) { result = argument(&reader,value); if (result < 0) { return result; } }
            if (action->tool == AGENT_WRITE) { memcpy(action->text,value,128); }
            else { result = agent_path(value,action->other); if (result < 0) { return result; } }
        }
        ++candidate.count; result = token(&reader,value);
        if (action->tool == AGENT_SYSTEM && result == 1 && word(value,"status")) { result = token(&reader,value); }
        if (result < 0) { return result; }
        if (result == 0) { result = agent_validate_plan(&candidate); if (result != 0) { return result; } memcpy(plan,&candidate,sizeof(*plan)); return 0; }
        if (!reader.separator && (reader.quoted || (!word(value,"and") && !word(value,"then")))) { return FS_EINVAL; }
    }
}
