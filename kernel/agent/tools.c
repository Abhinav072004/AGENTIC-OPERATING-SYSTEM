#include "broker.h"
#include "fs/vfs.h"
#include "runtime/memory.h"
#include "debug/format.h"
static uint64_t temporary_id;
static size_t size_of(const char *text) { size_t length = 0; while (text[length] != 0) { ++length; } return length; }
static long atomic_write(const struct agent_action *action) {
    struct fs_handles handles = {0}; char parent[128] = {0}, temporary[128];
    size_t length = size_of(action->path); while (length != 0 && action->path[length-1] != '/') { --length; }
    if (length == 0 || length >= sizeof(parent)) { return FS_EINVAL; }
    memcpy(parent,action->path,length); parent[length] = 0; long fd = FS_EEXIST;
    for (unsigned int attempt = 0; attempt < 16 && fd == FS_EEXIST; ++attempt) {
        if (temporary_id == UINT64_MAX) { return FS_ENOSPC; }
        int count = ksnprintf(temporary,sizeof(temporary),"%s.agent-%llu",parent,(unsigned long long)temporary_id++);
        if (count < 0 || (size_t)count >= sizeof(temporary)) { return FS_ENAMETOOLONG; }
        fd = fs_open(&handles,temporary,FS_CREATE|FS_WRITE|FS_EXCLUSIVE);
    }
    if (fd < 0) { return fd; }
    size_t bytes = size_of(action->text); long result = fs_write(&handles,(uint64_t)fd,action->text,bytes); fs_close_all(&handles);
    if (result == (long)bytes) { result = fs_replace(temporary,action->path); }
    else if (result >= 0) { result = FS_EIO; }
    if (result < 0) { (void)fs_unlink(temporary); } return result;
}
static long file_action(const struct agent_action *action, char output[128], uint32_t *bytes) {
    if (action->tool == AGENT_MKDIR) { return fs_mkdir(action->path); }
    if (action->tool == AGENT_WRITE) { long result = atomic_write(action); if (result == 0) { *bytes = (uint32_t)size_of(action->text); (void)ksnprintf(output,128,"Wrote %u bytes",*bytes); } return result; }
    if (action->tool == AGENT_MOVE) { return fs_rename(action->path,action->other); }
    if (action->tool == AGENT_DELETE) { return fs_unlink(action->path); }
    if (action->tool == AGENT_READ) {
        struct fs_handles handles = {0}; long fd = fs_open(&handles,action->path,FS_READ); if (fd < 0) { return fd; }
        long result = fs_read(&handles,(uint64_t)fd,output,127); fs_close_all(&handles); if (result < 0) { return result; }
        for (size_t i = 0; i < (size_t)result; ++i) { unsigned char c = (unsigned char)output[i]; if ((c < 32 && c != '\n' && c != '\t') || c > 126) { output[i] = '?'; } }
        output[result] = 0; *bytes = (uint32_t)result; return 0;
    }
    if (action->tool == AGENT_LIST) {
        size_t position = 0;
        for (uint64_t index = 0; index < FS_NODE_MAX; ++index) {
            struct fs_entry entry; long result = fs_list(action->path,index,&entry); if (result < 0) { return result; } if (result == 0) { break; }
            size_t length = size_of(entry.name); if (position+length+2 >= 128) { break; }
            memcpy(output+position,entry.name,length); position += length; if (entry.info.directory != 0) { output[position++] = '/'; } output[position++] = '\n'; output[position] = 0;
        }
        *bytes = (uint32_t)position; return 0;
    }
    return FS_EINVAL;
}
long agent_tools_execute(void *context, const struct agent_actor *actor, const struct agent_action *action, char output[128], uint32_t *bytes) {
    if (actor == NULL || action == NULL || output == NULL || bytes == NULL) { return FS_EINVAL; }
    struct agent_plan single = {.abi = AGENT_PLAN_ABI,.count = 1}; single.actions[0] = *action;
    long result = agent_validate_plan(&single); if (result < 0) { return result; }
    memset(output,0,128); *bytes = 0;
    if (action->tool == AGENT_SYSTEM) { const struct agent_tools *tools = context; return tools != NULL && tools->system != NULL ? tools->system(tools->context,output,bytes) : -38; }
    fs_set_uid(actor->uid); result = file_action(action,output,bytes); fs_set_uid(0); return result;
}
