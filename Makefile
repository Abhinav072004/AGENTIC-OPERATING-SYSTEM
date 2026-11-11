CC := clang-19
HOST_CC := cc
LD := ld.lld-19
NASM := nasm
PYTHON := python3
BUILD := build
LIMINE := .cache/limine
.DELETE_ON_ERROR:

CFLAGS := --target=x86_64-unknown-none-elf -std=c17 -O2 -g \
    -ffreestanding -fno-builtin -fno-stack-protector -fno-pic -fno-pie \
    -fno-unwind-tables -fno-asynchronous-unwind-tables -fno-omit-frame-pointer \
    -m64 -mcmodel=kernel -mno-red-zone -mgeneral-regs-only \
    -Wall -Wextra -Werror -Wpedantic -Wconversion -Wshadow -Wstrict-prototypes \
    -Wmissing-prototypes -I$(LIMINE) -Ikernel -MMD -MP
LDFLAGS := -nostdlib -static -z max-page-size=0x1000 -z noexecstack \
    --build-id=none -T kernel/arch/x86_64/linker.ld
SOURCES := kernel/kernel.c kernel/arch/x86_64/serial.c \
    kernel/drivers/display/framebuffer.c
OBJECTS := $(SOURCES:%.c=$(BUILD)/%.o) $(BUILD)/kernel/arch/x86_64/entry.o

.PHONY: all deps iso run run-uefi debug test test-debug test-host check-elf clean distclean
all: iso

$(LIMINE)/.ready: boot/limine.lock scripts/fetch-limine.sh
	./scripts/fetch-limine.sh
	touch $@
deps: $(LIMINE)/.ready

$(BUILD)/%.o: %.c Makefile $(LIMINE)/.ready
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: %.asm Makefile
	@mkdir -p $(@D)
	$(NASM) -f elf64 -g -F dwarf $< -o $@

$(BUILD)/kernel.elf: $(OBJECTS) kernel/arch/x86_64/linker.ld
	$(LD) $(LDFLAGS) $(OBJECTS) -o $@

$(BUILD)/astraos.iso: $(BUILD)/kernel.elf boot/limine.conf Makefile $(LIMINE)/.ready
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
iso: $(BUILD)/astraos.iso

run: iso
	$(PYTHON) tools/qemu.py --firmware bios
run-uefi: iso
	$(PYTHON) tools/qemu.py --firmware uefi
debug: iso
	$(PYTHON) tools/qemu.py --firmware bios --debug

check-elf: $(BUILD)/kernel.elf
	$(PYTHON) tests/check_elf.py $(BUILD)/kernel.elf
test-host: $(LIMINE)/.ready
	@mkdir -p $(BUILD)/tests
	$(HOST_CC) -std=c17 -g -O1 -Wall -Wextra -Werror -Wpedantic \
	    -fsanitize=address,undefined -fno-omit-frame-pointer -I$(LIMINE) -Ikernel \
	    tests/framebuffer_test.c kernel/drivers/display/framebuffer.c \
	    -o $(BUILD)/tests/framebuffer-test
	$(BUILD)/tests/framebuffer-test
test: iso check-elf test-host
	$(PYTHON) tests/boot_test.py --firmware bios
	$(PYTHON) tests/boot_test.py --firmware uefi
test-debug: iso
	$(PYTHON) tests/debug_test.py

clean:
	rm -rf $(BUILD)
distclean: clean
	rm -rf .cache/limine .cache/limine.tar.gz

-include $(OBJECTS:.o=.d)
