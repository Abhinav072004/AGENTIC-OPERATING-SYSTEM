#include "syscall.h"
#include "user/copy.h"
static long path_copy(struct user_process *process, uint64_t address, char *path) {
    for (size_t i = 0; i < FS_PATH_MAX; ++i) {
        if (address > UINT64_MAX - i || !copy_from_user(&process->space.vm, &path[i], address + i, 1)) { return SYS_EFAULT; }
        if (path[i] == '\0') { return 0; }
    }
    return FS_ENAMETOOLONG;
}
static long dispatch(struct user_process *process, uint64_t number, uint64_t a, uint64_t b, uint64_t c) {
    if (number == SYS_CLOSE) { return fs_close(&process->files, a); }
    if (number == SYS_SEEK) { return fs_seek(&process->files, a, b); }
    if (number == SYS_READ || number == SYS_WRITE) {
        bool write = number == SYS_WRITE;
        if (c > SYS_IO_MAX) { return SYS_E2BIG; }
        if (!user_buffer_valid(&process->space.vm, b, (size_t)c, !write)) { return SYS_EFAULT; }
        unsigned char buffer[SYS_IO_MAX];
        if (write && !copy_from_user(&process->space.vm, buffer, b, (size_t)c)) { return SYS_EFAULT; }
        long result = write ? fs_write(&process->files, a, buffer, (size_t)c) : fs_read(&process->files, a, buffer, (size_t)c);
        if (!write && result > 0 && !copy_to_user(&process->space.vm, b, buffer, (size_t)result)) { return SYS_EFAULT; }
        return result;
    }
    if (number < SYS_OPEN || (number > SYS_LIST && number != SYS_CHMOD && number != SYS_CHOWN)) { return SYS_ENOSYS; }
    char path[FS_PATH_MAX], other[FS_PATH_MAX];
    long result = path_copy(process, a, path);
    if (result < 0) { return result; }
    if (number == SYS_CHMOD) { return b > UINT32_MAX ? FS_EINVAL : fs_chmod(path, (uint32_t)b); }
    if (number == SYS_CHOWN) { return b > UINT32_MAX ? FS_EINVAL : fs_chown(path, (uint32_t)b); }
    if (number == SYS_OPEN) { return b > UINT32_MAX ? FS_EINVAL : fs_open(&process->files, path, (unsigned int)b); }
    if (number == SYS_MKDIR) { return fs_mkdir(path); }
    if (number == SYS_UNLINK) { return fs_unlink(path); }
    if (number == SYS_RENAME) { result = path_copy(process, b, other); return result < 0 ? result : fs_rename(path, other); }
    if (number == SYS_STAT) {
        struct fs_info info;
        if (!user_buffer_valid(&process->space.vm, b, sizeof(info), true)) { return SYS_EFAULT; }
        result = fs_stat(path, &info);
        if (result == 0 && !copy_to_user(&process->space.vm, b, &info, sizeof(info))) { return SYS_EFAULT; }
        return result;
    }
    if (number == SYS_LIST) {
        struct fs_entry entry;
        if (!user_buffer_valid(&process->space.vm, c, sizeof(entry), true)) { return SYS_EFAULT; }
        result = fs_list(path, b, &entry);
        if (result > 0 && !copy_to_user(&process->space.vm, c, &entry, sizeof(entry))) { return SYS_EFAULT; }
        return result;
    }
    return SYS_ENOSYS;
}

long fs_syscall(struct user_process *process, uint64_t number, uint64_t a, uint64_t b, uint64_t c) {
    fs_set_uid(process->uid); long result = dispatch(process,number,a,b,c); fs_set_uid(0); return result;
}
