#include "apps.h"
#include "syscall.h"
#include "text.h"
#include "fs/vfs.h"
struct editor_model editor;
struct monitor_model monitor;
bool apps_slate_theme;
char apps_status[64] = "Preferences are stored in RAM";
static uint32_t temporary_id;
long apps_atomic_save(const char *path, const void *bytes, size_t length) {
    char target[128]; if (!text_path("/",path,target)) { return FS_EINVAL; }
    size_t prefix = text_length(target); while (prefix != 0 && target[prefix-1] != '/') { --prefix; }
    char temporary[128]; long fd = FS_EEXIST;
    for (unsigned int attempt = 0; attempt < 16 && fd == FS_EEXIST; ++attempt) {
        if (temporary_id == UINT32_MAX) { return FS_ENOSPC; }
        char suffix[32] = ".save-", number[22]; (void)text_number(number,temporary_id++); text_copy(suffix+6,number,sizeof(suffix)-6);
        size_t suffix_length = text_length(suffix); if (prefix+suffix_length >= sizeof(temporary)) { return FS_ENAMETOOLONG; }
        for (size_t i = 0; i < prefix; ++i) { temporary[i] = target[i]; } text_copy(temporary+prefix,suffix,sizeof(temporary)-prefix);
        fd = user_call(SYS_OPEN,(uintptr_t)temporary,FS_CREATE|FS_WRITE|FS_EXCLUSIVE,0);
    }
    if (fd < 0) { return fd; }
    size_t offset = 0; long result = 0;
    while (offset < length) { size_t amount = length-offset; if (amount > SYS_IO_MAX) { amount = SYS_IO_MAX; } result = user_write((uint64_t)fd,(const unsigned char *)bytes+offset,amount); if (result <= 0) { if (result == 0) { result = FS_EIO; } break; } offset += (size_t)result; }
    (void)user_call(SYS_CLOSE,(uint64_t)fd,0,0);
    if (result >= 0) { result = user_call(SYS_RENAME,(uintptr_t)temporary,(uintptr_t)target,FS_REPLACE); }
    if (result < 0) { (void)user_call(SYS_UNLINK,(uintptr_t)temporary,0,0); }
    return result;
}
void editor_new(bool discard) { if (editor.dirty && !discard) { text_copy(editor.status,"Unsaved changes: Save or Discard first",sizeof(editor.status)); return; } editor.length = editor.cursor = 0; editor.text[0] = 0; editor.dirty = false; editor.path_mode = false; text_copy(editor.path,"/home/untitled.txt",sizeof(editor.path)); text_copy(editor.status,"New text file",sizeof(editor.status)); }
void editor_choose_path(void) { if (editor.dirty) { text_copy(editor.status,"Unsaved changes: Save or Discard first",sizeof(editor.status)); return; } editor.path_mode = true; editor.path[0] = 0; text_copy(editor.status,"Enter an absolute path, then press Enter",sizeof(editor.status)); (void)user_write(1,"APP editor path",15); }
long editor_open(const char *path) {
    if (editor.dirty) { text_copy(editor.status,"Unsaved changes: Save or Discard first",sizeof(editor.status)); return FS_EINVAL; }
    char canonical[128]; if (!text_path("/home",path,canonical)) { return FS_EINVAL; }
    long fd = user_call(SYS_OPEN,(uintptr_t)canonical,FS_READ,0);
    if (fd == FS_ENOENT) { text_copy(editor.path,canonical,sizeof(editor.path)); editor.length = editor.cursor = 0; editor.text[0] = 0; editor.path_mode = false; text_copy(editor.status,"New path. Type text, then Save",sizeof(editor.status)); return 0; }
    if (fd < 0) { text_copy(editor.status,"Cannot open this path",sizeof(editor.status)); return fd; }
    char content[EDITOR_CAPACITY]; size_t offset = 0; long result = 0;
    for (;;) { char buffer[SYS_IO_MAX]; result = user_read((uint64_t)fd,buffer,sizeof(buffer)); if (result <= 0) { break; } if ((size_t)result >= sizeof(content)-offset) { result = FS_ENOSPC; break; } for (size_t i = 0; i < (size_t)result; ++i) { unsigned char byte = (unsigned char)buffer[i]; if (byte != '\n' && byte != '\r' && byte != '\t' && (byte < 32 || byte > 126)) { result = FS_EINVAL; break; } content[offset++] = buffer[i]; } if (result < 0) { break; } }
    (void)user_call(SYS_CLOSE,(uint64_t)fd,0,0);
    if (result < 0) { text_copy(editor.status,"Only ASCII text up to 4095 bytes is supported",sizeof(editor.status)); return result; }
    content[offset] = 0; text_copy(editor.text,content,sizeof(editor.text)); editor.length = offset; editor.cursor = 0; text_copy(editor.path,canonical,sizeof(editor.path)); editor.dirty = editor.path_mode = false; text_copy(editor.status,"Opened. Ctrl+S saves",sizeof(editor.status)); (void)user_write(1,"APP editor opened",17); return 0;
}
long editor_save(void) { if (editor.path_mode) { text_copy(editor.status,"Finish entering the path first",sizeof(editor.status)); return FS_EINVAL; } long result = apps_atomic_save(editor.path,editor.text,editor.length); if (result == 0) { editor.dirty = false; text_copy(editor.status,"Saved in RAM",sizeof(editor.status)); (void)user_write(1,"APP editor saved",16); } else { text_copy(editor.status,"Save failed; previous file is intact",sizeof(editor.status)); } return result; }
void editor_key(uint32_t key, uint32_t modifiers) {
    if ((modifiers & KEY_CONTROL) != 0) { if (key == 's' || key == 'S') { (void)editor_save(); } else if (key == 'o' || key == 'O') { editor_choose_path(); } else if (key == 'n' || key == 'N') { editor_new(false); } return; }
    if (editor.path_mode) { size_t length = text_length(editor.path); if (key == '\n') { char path[128]; text_copy(path,editor.path,sizeof(path)); (void)editor_open(path); } else if (key == '\b') { if (length != 0) { editor.path[length-1] = 0; } } else if (key >= 32 && key <= 126 && length+1 < sizeof(editor.path)) { editor.path[length] = (char)key; editor.path[length+1] = 0; } return; }
    if (key == 258) { if (editor.cursor != 0) { --editor.cursor; } return; }
    if (key == 259) { if (editor.cursor < editor.length) { ++editor.cursor; } return; }
    if (key == 256 || key == 257) {
        size_t start = editor.cursor; while (start != 0 && editor.text[start-1] != '\n') { --start; } size_t column = editor.cursor-start;
        if (key == 256 && start != 0) { size_t end = start-1; start = end; while (start != 0 && editor.text[start-1] != '\n') { --start; } editor.cursor = start+(column < end-start ? column : end-start); }
        else if (key == 257) { size_t next = editor.cursor; while (next < editor.length && editor.text[next] != '\n') { ++next; } if (next < editor.length) { start = ++next; while (next < editor.length && editor.text[next] != '\n') { ++next; } editor.cursor = start+(column < next-start ? column : next-start); } }
        return;
    }
    if (key == '\b' && editor.cursor != 0) { for (size_t i = editor.cursor; i <= editor.length; ++i) { editor.text[i-1] = editor.text[i]; } --editor.cursor; --editor.length; editor.dirty = true; }
    else if (key == 260 && editor.cursor < editor.length) { for (size_t i = editor.cursor+1; i <= editor.length; ++i) { editor.text[i-1] = editor.text[i]; } --editor.length; editor.dirty = true; }
    else if ((key == '\n' || (key >= 32 && key <= 126)) && editor.length+1 < sizeof(editor.text)) { for (size_t i = editor.length+1; i > editor.cursor; --i) { editor.text[i] = editor.text[i-1]; } editor.text[editor.cursor++] = (char)key; ++editor.length; editor.dirty = true; }

}
void monitor_refresh(void) { (void)user_call(SYS_SYSTEM,(uintptr_t)&monitor.system,0,0); (void)user_call(SYS_SELF,(uintptr_t)&monitor.self,0,0); monitor.count = 0; for (uint64_t i = 0; i < 16; ++i) { struct task_info task; if (user_call(SYS_TASK,i,(uintptr_t)&task,0) == 1) { monitor.tasks[monitor.count++] = task; } } }
long settings_select(bool slate) { const char value = slate ? '1' : '0'; long result = apps_atomic_save("/home/.settings",&value,1); if (result == 0) { apps_slate_theme = slate; text_copy(apps_status,"Appearance saved in RAM",sizeof(apps_status)); } else { text_copy(apps_status,"Could not save appearance; settings unchanged",sizeof(apps_status)); } return result; }

void apps_init(void) { editor_new(true); monitor_refresh(); long fd = user_call(SYS_OPEN,(uintptr_t)"/home/.settings",FS_READ,0); if (fd >= 0) { char value; if (user_read((uint64_t)fd,&value,1) == 1) { apps_slate_theme = value == '1'; } (void)user_call(SYS_CLOSE,(uint64_t)fd,0,0); } }
