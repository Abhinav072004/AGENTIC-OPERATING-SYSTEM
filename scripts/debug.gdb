set pagination off
set architecture i386:x86-64
file build/kernel.elf
target remote 127.0.0.1:1234
# The higher-half mapping is not installed while firmware is executing.
# Use a hardware breakpoint, which does not patch unmapped memory.
hbreak kernel_main
continue
info registers rip rsp
x/5i $rip
