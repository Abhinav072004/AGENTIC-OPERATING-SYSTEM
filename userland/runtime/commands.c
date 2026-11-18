#include "syscall.h"
#include "text.h"
#include "fs/vfs.h"
#include "commands.h"
static char cwd[128] = "/home";
static bool leaving;
static void (*sink)(const char *text);
static void output(const char *text) { if (sink != NULL) { sink(text); } }
static void number(int64_t value) { char text[22]; (void)text_number(text, value); output(text); }
static long path_call(uint64_t call, const char *path, uint64_t b, uint64_t c) { return user_call(call, (uintptr_t)path, b, c); }
static long wait_child(uint64_t pid) { int status; long result; do { result = user_call(SYS_WAIT, pid, (uintptr_t)&status, 0); if (result == SYS_EAGAIN) { (void)user_call(SYS_SLEEP, 1, 0, 0); } } while (result == SYS_EAGAIN); if (result < 0) { return result; } char captured[SYS_IO_MAX+1]; long bytes; bool captured_any = false, newline = false; while ((bytes = user_call(SYS_GUI_CAPTURE, (uintptr_t)captured, SYS_IO_MAX, 0)) > 0) { captured[bytes] = 0; captured_any = true; newline = captured[bytes-1] == '\n'; output(captured); } if (captured_any && !newline) { output("\n"); } output("exit="); number(status); output("\n"); return status; }
static long execute(int count, char **args) {
    const char *command = args[0];
    if (text_equal(command, "help")) { output("help echo uname uptime mem ps pid id pwd cd ls mkdir touch cat write append rm mv stat chmod run wait kill exit\n"); return 0; }
    if (text_equal(command, "exit")) { leaving = true; return 0; }
    if (text_equal(command, "echo")) { for (int i = 1; i < count; ++i) { if (i > 1) { output(" "); } output(args[i]); } output("\n"); return 0; }
    if (text_equal(command, "pwd")) { output(cwd); output("\n"); return 0; }
    if (text_equal(command, "pid")) { number(user_call(SYS_GETPID, 0, 0, 0)); output("\n"); return 0; }
    if (text_equal(command, "id")) { struct self_info info; long result = user_call(SYS_SELF,(uintptr_t)&info,0,0); if (result < 0) { return result; } output("uid="); number(info.uid); output(" capabilities="); number((int64_t)info.capabilities); output("\n"); return 0; }
    if (text_equal(command, "chmod")) { if (count != 3) { return FS_EINVAL; } uint32_t mode = 0; for (size_t i = 0; args[1][i] != 0; ++i) { if (args[1][i] < '0' || args[1][i] > '7' || mode > 0177) { return FS_EINVAL; } mode = mode*8u+(uint32_t)(args[1][i]-'0'); } char target[128]; if (args[1][0] == 0 || !text_path(cwd,args[2],target)) { return FS_EINVAL; } return path_call(SYS_CHMOD,target,mode,0); }
    if (text_equal(command, "uname") || text_equal(command, "uptime") || text_equal(command, "mem")) {
        struct system_info info; long result = user_call(SYS_SYSTEM, (uintptr_t)&info, 0, 0); if (result < 0) { return result; }
        if (text_equal(command, "uname")) { output("AgenticOS "); output(info.version); output(" x86_64\n"); }
        else if (text_equal(command, "uptime")) { output("ticks="); number((int64_t)info.ticks); output(" hz=100\n"); }
        else { output("free_frames="); number((int64_t)info.free_frames); output(" managed_frames="); number((int64_t)info.managed_frames); output(" fs_blocks="); number((int64_t)info.fs_blocks); output("\n"); } return 0;
    }
    if (text_equal(command, "ps")) {
        output("PID TID STATE TICKS NAME\n");
        for (uint64_t i = 0; i < 16; ++i) { struct task_info info; long result = user_call(SYS_TASK, i, (uintptr_t)&info, 0); if (result < 0) { return result; } if (result == 0) { continue; } number((int64_t)info.pid); output(" "); number((int64_t)info.tid); output(" "); number(info.state); output(" "); number((int64_t)info.runtime_ticks); output(" "); output(info.name); output("\n"); } return 0;
    }
    if (text_equal(command, "wait") || text_equal(command, "kill")) { uint64_t pid; if (count != 2 || !text_unsigned(args[1], &pid)) { return FS_EINVAL; } return text_equal(command, "wait") ? wait_child(pid) : user_call(SYS_KILL, pid, 0, 0); }
    char path[128], other[128];
    if (text_equal(command, "ls")) {
        if (count > 2 || !text_path(cwd, count == 1 ? "." : args[1], path)) { return FS_EINVAL; }
        for (uint64_t i = 0; ; ++i) { struct fs_entry entry; long result = path_call(SYS_LIST, path, i, (uintptr_t)&entry); if (result <= 0) { return result; } output(entry.name); if (entry.info.directory != 0) { output("/"); } output("\n"); }
    }
    if (!text_equal(command, "cd") && !text_equal(command, "stat") && !text_equal(command, "mkdir") && !text_equal(command, "rm") && !text_equal(command, "mv") && !text_equal(command, "run") && !text_equal(command, "cat") && !text_equal(command, "write") && !text_equal(command, "append") && !text_equal(command, "touch")) { output("error: unknown command; type help\n"); return SYS_ENOSYS; }
    if (count < 2 || !text_path(cwd, args[1], path)) { return FS_EINVAL; }
    if (text_equal(command, "cd") || text_equal(command, "stat")) { if (count != 2) { return FS_EINVAL; } struct fs_info info; long result = path_call(SYS_STAT, path, (uintptr_t)&info, 0); if (result < 0) { return result; } if (text_equal(command, "cd")) { if (info.directory == 0) { return FS_ENOTDIR; } text_copy(cwd, path, sizeof(cwd)); } else { output(info.directory != 0 ? "directory bytes=" : "file bytes="); number((int64_t)info.size); output("\n"); } return 0; }
    if (text_equal(command, "mkdir")) { return count != 2 ? FS_EINVAL : path_call(SYS_MKDIR, path, 0, 0); }
    if (text_equal(command, "rm")) { return count != 2 ? FS_EINVAL : path_call(SYS_UNLINK, path, 0, 0); }
    if (text_equal(command, "mv")) { return count != 3 || !text_path(cwd, args[2], other) ? FS_EINVAL : path_call(SYS_RENAME, path, (uintptr_t)other, 0); }
    if (text_equal(command, "run")) { if (count != 2) { return FS_EINVAL; } long child = path_call(SYS_SPAWN, path, 0, 0); return child < 0 ? child : wait_child((uint64_t)child); }
    bool cat = text_equal(command, "cat"), write = text_equal(command, "write"), append = text_equal(command, "append"), touch = text_equal(command, "touch");
    if (!cat && !write && !append && !touch) { output("error: unknown command; type help\n"); return SYS_ENOSYS; }
    if (((cat || touch) && count != 2) || ((write || append) && count != 3)) { return FS_EINVAL; }
    unsigned int flags = cat ? FS_READ : FS_WRITE | FS_CREATE | (write ? FS_TRUNCATE : 0u) | (append ? FS_APPEND : 0u);
    long fd = path_call(SYS_OPEN, path, flags, 0); if (fd < 0) { return fd; }
    long result = 0;
    if (cat) { char buffer[SYS_IO_MAX + 1]; do { result = user_read((uint64_t)fd, buffer, SYS_IO_MAX); if (result > 0) { buffer[result] = '\0'; output(buffer); } } while (result > 0); output("\n"); }
    else if (write || append) { size_t length = text_length(args[2]), offset = 0; while (offset < length) { size_t amount = length - offset; if (amount > SYS_IO_MAX) { amount = SYS_IO_MAX; } result = user_write((uint64_t)fd, args[2] + offset, amount); if (result < 0) { break; } offset += (size_t)result; } }
    (void)user_call(SYS_CLOSE, (uint64_t)fd, 0, 0); return result < 0 ? result : 0;
}
static const char *error_text(long error) {
    if (error == FS_ENOENT) { return "path not found"; }
    if (error == FS_EEXIST) { return "destination already exists"; }
    if (error == FS_ENOTDIR) { return "path is not a directory"; }
    if (error == FS_EISDIR) { return "path is a directory"; }
    if (error == FS_ENOSPC) { return "RAM storage or process capacity exhausted"; }
    if (error == FS_ENOTEMPTY) { return "directory is not empty"; }
    if (error == FS_EINVAL) { return "invalid arguments or path; type help"; }
    if (error == SYS_EPERM || error == FS_EACCES) { return "permission denied"; }
    if (error == SYS_ESRCH) { return "process not found"; }
    if (error == SYS_ENOSYS) { return "unknown command; type help"; }
    return "operation failed";
}
long commands_run(const char *source) { char line[TEXT_LINE_MAX], *args[TEXT_ARGS_MAX]; text_copy(line, source, sizeof(line)); int count = text_split(line, args); if (count < 0) { output(count == -1 ? "error: unterminated quote\n" : count == -2 ? "error: too many arguments\n" : "error: incomplete escape\n"); return FS_EINVAL; } if (count == 0) { return 0; } long result = execute(count, args); if (result < 0) { output("error="); number(result); output(" "); output(error_text(result)); output("\n"); } return result; }

void commands_init(void (*emit)(const char *text)) { sink = emit; text_copy(cwd, "/home", sizeof(cwd)); leaving = false; }
const char *commands_cwd(void) { return cwd; }
bool commands_leaving(void) { return leaving; }
