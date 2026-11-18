#include "syscall.h"
#include "text.h"
#include "commands.h"
#include "fs/vfs.h"
int user_main(uint64_t pid, uint64_t mode);
static void output(const char *text) { size_t length = text_length(text), offset = 0; while (offset < length) { size_t amount = length - offset; if (amount > SYS_IO_MAX) { amount = SYS_IO_MAX; } if (user_write(1, text + offset, amount) != (long)amount) { return; } offset += amount; } }
int user_main(uint64_t pid, uint64_t mode) {
    (void)pid; commands_init(output);
    if (mode == 1) {
        static const char *const commands[] = {"uname", "pwd", "mkdir demo", "cd demo", "write note 'hello world'", "append note ' + AgenticOS'", "cat note", "mv note renamed", "ls", "stat renamed", "ps", "mem", "run /bin/hello", "run /bin/probe", "rm renamed", "cd ..", "rm demo"};
        for (size_t i = 0; i < sizeof(commands) / sizeof(commands[0]); ++i) { if (commands_run(commands[i]) < 0) { return (int)i + 1; } }
        if (commands_run("cat /missing") != FS_ENOENT || commands_run("echo 'bad") != FS_EINVAL || commands_run("unknown") != SYS_ENOSYS) { return 90; }
        output("native shell checks passed\n"); return 0;
    }
    output("AgenticOS native shell. Type help. Files reset on reboot.\n");
    while (!commands_leaving()) {
        output(commands_cwd()); output(" > "); char line[TEXT_LINE_MAX]; size_t length = 0; bool overflow = false;
        for (;;) { char byte; long result = user_read(0, &byte, 1); if (result < 0) { return 1; } if (result == 0) { (void)user_call(SYS_SLEEP, 1, 0, 0); continue; } if (byte == '\r') { continue; } if (byte == '\n') { output("\n"); break; } if (byte == '\b' || byte == 127) { if (length != 0) { --length; output("\b \b"); } continue; } if ((unsigned char)byte < 32 || (unsigned char)byte > 126) { continue; } if (length + 1 < sizeof(line) && !overflow) { line[length++] = byte; (void)user_write(1, &byte, 1); } else { overflow = true; } }
        line[length] = '\0'; if (overflow) { output("error: line too long (255 bytes maximum)\n"); } else { (void)commands_run(line); }
    }
    return 0;
}
