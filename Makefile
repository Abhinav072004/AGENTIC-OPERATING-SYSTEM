CC := clang-19
HOST_CC := cc
LD := ld.lld-19
NASM := nasm
PYTHON := python3
BUILD := build
LIMINE := .cache/limine
QEMU_DISPLAY ?= gtk
KERNEL_LOG_LEVEL ?= INFO
BOOT_TEST ?= none
.DELETE_ON_ERROR:

CFLAGS := --target=x86_64-unknown-none-elf -std=c17 -O2 -g \
    -ffreestanding -fno-builtin -fno-stack-protector -fno-pic -fno-pie \
    -fno-unwind-tables -fno-asynchronous-unwind-tables -fno-omit-frame-pointer \
    -m64 -mcmodel=kernel -mno-red-zone -mgeneral-regs-only \
    -Wall -Wextra -Werror -Wpedantic -Wconversion -Wshadow -Wstrict-prototypes \
    -Wmissing-prototypes -I$(LIMINE) -Ikernel -include $(BUILD)/config.h -MMD -MP
LDFLAGS := -nostdlib -static -z max-page-size=0x1000 -z noexecstack \
    --build-id=none -T kernel/arch/x86_64/linker.ld
SOURCES := kernel/kernel.c kernel/arch/x86_64/serial.c \
    kernel/drivers/display/framebuffer.c kernel/arch/x86_64/cpuid.c \
    kernel/arch/x86_64/diagnostics.c kernel/cpu/info.c \
    kernel/debug/format.c kernel/debug/log.c kernel/debug/panic.c \
    kernel/memory/pmm.c kernel/memory/heap.c kernel/memory/init.c \
    kernel/arch/x86_64/paging.c kernel/arch/x86_64/memory.c kernel/runtime/memory.c \
    kernel/arch/x86_64/descriptors.c kernel/arch/x86_64/interrupts.c \
    kernel/arch/x86_64/controllers.c kernel/interrupts/interrupts.c \
    kernel/process/process.c kernel/scheduler/core.c kernel/scheduler/stack.c \
    kernel/scheduler/task.c kernel/scheduler/selftest.c kernel/user/elf.c \
    kernel/user/copy.c kernel/user/space.c kernel/user/frame.c kernel/user/user.c kernel/user/selftest.c kernel/syscall/dispatch.c kernel/syscall/system.c kernel/user/shell.c kernel/fs/vfs.c kernel/fs/syscall.c kernel/fs/selftest.c kernel/drivers/pci.c kernel/drivers/ata.c kernel/drivers/input.c kernel/drivers/input_decode.c kernel/drivers/e1000.c kernel/drivers/drivers.c kernel/net/stack.c kernel/net/device.c kernel/net/syscall.c kernel/gui/display.c kernel/gui/buffer.c kernel/gui/syscall.c kernel/gui/desktop.c
ASM_SOURCES := kernel/arch/x86_64/entry.asm kernel/arch/x86_64/isr.asm kernel/arch/x86_64/task_probe.asm kernel/user/images.asm
ifneq ($(filter memory-ro memory-nx memory-guard,$(BOOT_TEST)),)
SOURCES += tests/guest/memory_probe.c
ASM_SOURCES += tests/guest/memory_probe_entry.asm
endif
ifneq ($(filter scheduler-guard scheduler-watchdog,$(BOOT_TEST)),)
SOURCES += tests/guest/scheduler_probe.c
endif
ifeq ($(BOOT_TEST),user-reject)
SOURCES += tests/guest/user_probe.c
endif
ifneq ($(filter exception-divide exception-ud exception-gp exception-pf exception-df exception-nmi exception-ro exception-nx,$(BOOT_TEST)),)
SOURCES += tests/guest/exception_probe.c
ASM_SOURCES += tests/guest/exception_probe_entry.asm
endif
OBJECTS := $(SOURCES:%.c=$(BUILD)/%.o) $(ASM_SOURCES:%.asm=$(BUILD)/%.o)

.PHONY: all deps iso run run-uefi debug test test-debug test-foundation test-memory test-interrupts test-scheduler test-user test-host check-elf clean distclean FORCE
all: iso

$(LIMINE)/.ready: boot/limine.lock scripts/fetch-limine.sh
	./scripts/fetch-limine.sh
	touch $@
deps: $(LIMINE)/.ready

FORCE:
$(BUILD)/config.h: FORCE tools/configure.py
	$(PYTHON) tools/configure.py --output $@ --log-level $(KERNEL_LOG_LEVEL) --selftest $(BOOT_TEST)

$(BUILD)/%.o: %.c Makefile $(BUILD)/config.h $(LIMINE)/.ready
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: %.asm Makefile
	@mkdir -p $(@D)
	$(NASM) -f elf64 -g -F dwarf $< -o $@

USER_CFLAGS := --target=x86_64-unknown-none-elf -std=c17 -O2 -g -ffreestanding -fno-builtin \
    -fno-stack-protector -fno-pic -fno-pie -fno-unwind-tables -fno-asynchronous-unwind-tables \
    -mno-red-zone -mgeneral-regs-only -Wall -Wextra -Werror -Wpedantic -Wconversion \
    -Wshadow -Wstrict-prototypes -Wmissing-prototypes -Ikernel -Iuserland/runtime -MMD -MP
.SECONDARY: $(BUILD)/user/hello.o $(BUILD)/user/probe.o $(BUILD)/user/fstest.o $(BUILD)/user/shell.o $(BUILD)/user/nettest.o $(BUILD)/user/desktop.o
$(BUILD)/user/%.o: userland/applications/%.c userland/runtime/syscall.h kernel/syscall/abi.h kernel/user/elf.h Makefile
	@mkdir -p $(@D)
	$(CC) $(USER_CFLAGS) -c $< -o $@
$(BUILD)/user/start.o: userland/runtime/start.asm Makefile
	@mkdir -p $(@D)
	$(NASM) -f elf64 -g -F dwarf $< -o $@
$(BUILD)/user/text.o: userland/runtime/text.c userland/runtime/text.h Makefile
	@mkdir -p $(@D)
	$(CC) $(USER_CFLAGS) -c $< -o $@
$(BUILD)/user/runtime-memory.o: kernel/runtime/memory.c kernel/runtime/memory.h Makefile
	@mkdir -p $(@D)
	$(CC) $(USER_CFLAGS) -c $< -o $@
$(BUILD)/user/runtime-%.o: userland/runtime/%.c Makefile
	@mkdir -p $(@D)
	$(CC) $(USER_CFLAGS) -c $< -o $@
$(BUILD)/user/shell.elf: $(BUILD)/user/runtime-commands.o
$(BUILD)/user/desktop.elf: $(BUILD)/user/runtime-commands.o $(BUILD)/user/runtime-window.o
$(BUILD)/user/desktop-image.elf: $(BUILD)/user/desktop.elf
	objcopy --strip-debug $< $@
$(BUILD)/user/%.elf: $(BUILD)/user/%.o $(BUILD)/user/start.o $(BUILD)/user/text.o $(BUILD)/user/runtime-memory.o userland/runtime/linker.ld
	$(LD) -nostdlib -static -z max-page-size=0x1000 -z noexecstack --build-id=none \
	    -T userland/runtime/linker.ld $(filter %.o,$^) -o $@
$(BUILD)/kernel/user/images.o: kernel/user/images.asm $(BUILD)/user/hello.elf $(BUILD)/user/probe.elf $(BUILD)/user/fstest.elf $(BUILD)/user/shell.elf $(BUILD)/user/nettest.elf $(BUILD)/user/desktop-image.elf Makefile
	@mkdir -p $(@D)
	$(NASM) -f elf64 -g -F dwarf -DUSER_BUILD='"$(abspath $(BUILD))/user"' $< -o $@

$(BUILD)/kernel.elf: $(OBJECTS) kernel/arch/x86_64/linker.ld
	$(LD) $(LDFLAGS) $(OBJECTS) -o $@

$(BUILD)/agenticos.iso: $(BUILD)/kernel.elf boot/limine.conf Makefile $(LIMINE)/.ready
	rm -rf $(BUILD)/iso-root
	mkdir -p $(BUILD)/iso-root/boot/limine $(BUILD)/iso-root/EFI/BOOT
	cp $(BUILD)/kernel.elf $(BUILD)/iso-root/boot/kernel.elf
	cp boot/limine.conf $(BUILD)/iso-root/boot/limine/limine.conf
	cp $(LIMINE)/limine-bios.sys $(LIMINE)/limine-bios-cd.bin \
	    $(LIMINE)/limine-uefi-cd.bin $(BUILD)/iso-root/boot/limine/
	cp $(LIMINE)/BOOTX64.EFI $(BUILD)/iso-root/EFI/BOOT/
	cp LICENSE $(BUILD)/iso-root/LICENSE
	cp $(LIMINE)/LICENSE $(BUILD)/iso-root/LIMINE-LICENSE.txt
	cp $(LIMINE)/LICENSE $(BUILD)/LIMINE-LICENSE.txt
	xorriso -as mkisofs -R -r -J -b boot/limine/limine-bios-cd.bin \
	    -no-emul-boot -boot-load-size 4 -boot-info-table \
	    --efi-boot boot/limine/limine-uefi-cd.bin -efi-boot-part \
	    --efi-boot-image --protective-msdos-label $(BUILD)/iso-root -o $@
	$(LIMINE)/limine bios-install $@
iso: $(BUILD)/agenticos.iso

run: iso
	$(PYTHON) tools/qemu.py --firmware bios
run-uefi: iso
	$(PYTHON) tools/qemu.py --firmware uefi
debug: iso
	$(PYTHON) tools/qemu.py --firmware bios --debug

check-elf: $(BUILD)/kernel.elf
	$(PYTHON) tests/check_elf.py $(BUILD)/kernel.elf
test-host: $(LIMINE)/.ready $(BUILD)/user/probe.elf
	@mkdir -p $(BUILD)/tests
	$(HOST_CC) -std=c17 -g -O1 -Wall -Wextra -Werror -Wpedantic \
	    -fsanitize=address,undefined -fno-omit-frame-pointer -I$(LIMINE) -Ikernel \
	    tests/framebuffer_test.c kernel/drivers/display/framebuffer.c \
	    -o $(BUILD)/tests/framebuffer-test
	$(BUILD)/tests/framebuffer-test
	$(HOST_CC) -std=c17 -g -O1 -Wall -Wextra -Werror -Wpedantic \
	    -fsanitize=address,undefined -fno-omit-frame-pointer -Ikernel \
	    tests/foundation_test.c kernel/debug/format.c kernel/debug/log.c \
	    kernel/debug/panic.c kernel/cpu/info.c -o $(BUILD)/tests/foundation-test
	$(BUILD)/tests/foundation-test
	$(PYTHON) tests/config_test.py
	$(HOST_CC) -std=c17 -g -O1 -Wall -Wextra -Werror -Wpedantic \
	    -fsanitize=address,undefined -fno-omit-frame-pointer -Ikernel \
	    tests/memory_test.c kernel/memory/pmm.c kernel/memory/heap.c \
	    kernel/arch/x86_64/paging.c -o $(BUILD)/tests/memory-test
	$(BUILD)/tests/memory-test
	$(HOST_CC) -std=c17 -g -O1 -Wall -Wextra -Werror -Wpedantic \
	    -fsanitize=address,undefined -fno-omit-frame-pointer -ffreestanding -fno-builtin \
	    -Dmemcpy=agenticos_memcpy -Dmemmove=agenticos_memmove \
	    -Dmemset=agenticos_memset -Dmemcmp=agenticos_memcmp -Ikernel \
	    -c kernel/runtime/memory.c -o $(BUILD)/tests/runtime-memory.o
	$(HOST_CC) -std=c17 -g -O1 -Wall -Wextra -Werror -Wpedantic \
	    -fsanitize=address,undefined -fno-omit-frame-pointer -Ikernel \
	    tests/runtime_memory_test.c $(BUILD)/tests/runtime-memory.o -o $(BUILD)/tests/runtime-memory-test
	$(BUILD)/tests/runtime-memory-test
	$(HOST_CC) -std=c17 -g -O1 -Wall -Wextra -Werror -Wpedantic \
	    -fsanitize=address,undefined -fno-omit-frame-pointer -Ikernel \
	    tests/interrupt_test.c kernel/arch/x86_64/descriptors.c \
	    kernel/arch/x86_64/controllers.c -o $(BUILD)/tests/interrupt-test
	$(BUILD)/tests/interrupt-test
	$(HOST_CC) -std=c17 -g -O1 -Wall -Wextra -Werror -Wpedantic \
	    -fsanitize=address,undefined -fno-omit-frame-pointer -Ikernel \
	    tests/scheduler_test.c kernel/process/process.c kernel/scheduler/core.c \
	    kernel/scheduler/stack.c -o $(BUILD)/tests/scheduler-test
	$(BUILD)/tests/scheduler-test
	$(HOST_CC) -std=c17 -g -O1 -Wall -Wextra -Werror -Wpedantic \
	    -fsanitize=address,undefined -fno-omit-frame-pointer -Ikernel \
	    tests/user_test.c kernel/user/elf.c kernel/user/copy.c kernel/user/space.c kernel/user/frame.c \
	    kernel/arch/x86_64/paging.c kernel/memory/pmm.c -o $(BUILD)/tests/user-test
	$(BUILD)/tests/user-test $(BUILD)/user/probe.elf
	$(HOST_CC) -std=c17 -g -O1 -Wall -Wextra -Werror -Wpedantic -fsanitize=address,undefined -fno-omit-frame-pointer -Ikernel tests/fs_test.c kernel/fs/vfs.c -o $(BUILD)/tests/fs-test
	$(BUILD)/tests/fs-test
	$(HOST_CC) -std=c17 -g -O1 -Wall -Wextra -Werror -Wpedantic -fsanitize=address,undefined -fno-omit-frame-pointer -Ikernel tests/input_test.c kernel/drivers/input_decode.c -o $(BUILD)/tests/input-test
	$(BUILD)/tests/input-test
	$(HOST_CC) -std=c17 -g -O1 -Wall -Wextra -Werror -Wpedantic -fsanitize=address,undefined -fno-omit-frame-pointer -Iuserland/runtime tests/text_test.c userland/runtime/text.c -o $(BUILD)/tests/text-test
	$(BUILD)/tests/text-test
	$(HOST_CC) -std=c17 -g -O1 -Wall -Wextra -Werror -Wpedantic -fsanitize=address,undefined -fno-omit-frame-pointer -Ikernel tests/net_test.c kernel/net/stack.c -o $(BUILD)/tests/net-test
	$(BUILD)/tests/net-test
	$(HOST_CC) -std=c17 -g -O1 -Wall -Wextra -Werror -Wpedantic -fsanitize=address,undefined -fno-omit-frame-pointer -Iuserland/runtime tests/window_test.c userland/runtime/window.c userland/runtime/text.c -o $(BUILD)/tests/window-test
	$(BUILD)/tests/window-test
	$(HOST_CC) -std=c17 -g -O1 -Wall -Wextra -Werror -Wpedantic -fsanitize=address,undefined -fno-omit-frame-pointer -Ikernel -I$(LIMINE) tests/display_test.c kernel/gui/display.c -o $(BUILD)/tests/display-test
	$(BUILD)/tests/display-test
test: iso check-elf test-host
	$(PYTHON) tests/boot_test.py --firmware bios
	$(PYTHON) tests/boot_test.py --firmware uefi
test-debug: iso
	$(PYTHON) tests/debug_test.py
test-foundation:
	$(PYTHON) tests/foundation_matrix.py
test-memory:
	$(PYTHON) tests/memory_matrix.py
	$(PYTHON) tests/memory_runner_test.py
test-interrupts:
	$(PYTHON) tests/interrupt_matrix.py
test-scheduler:
	$(PYTHON) tests/scheduler_matrix.py
test-user:
	$(PYTHON) tests/user_matrix.py

clean:
	rm -rf $(BUILD)
distclean: clean
	rm -rf .cache/limine .cache/limine.tar.gz

-include $(OBJECTS:.o=.d)

test-drivers:
	$(PYTHON) tests/driver_test.py

test-shell:
	$(PYTHON) tests/shell_test.py

.PHONY: shell test-shell test-drivers
shell:
	$(MAKE) -j2 BUILD=build/interactive BOOT_TEST=shell iso
	$(PYTHON) tools/qemu.py --iso build/interactive/agenticos.iso

.PHONY: test-network
test-network:
	$(PYTHON) tests/network_test.py

.PHONY: desktop test-desktop
desktop:
	$(MAKE) -j2 BUILD=build/desktop BOOT_TEST=desktop iso
	$(PYTHON) tools/qemu.py --iso build/desktop/agenticos.iso --display $(QEMU_DISPLAY)
test-desktop:
	$(PYTHON) tests/desktop_test.py
-include $(wildcard $(BUILD)/user/*.d)
