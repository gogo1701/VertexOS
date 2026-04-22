ASM=nasm
CC=gcc
LD=ld
OBJCOPY=objcopy
QEMU=/usr/bin/qemu-system-i386
QEMU_UEFI=qemu-system-x86_64
QEMU_NET=-netdev user,id=n1 -device rtl8139,netdev=n1
KVM ?= auto
KVM_AVAILABLE := $(shell [ -r /dev/kvm ] && echo 1 || echo 0)

ifeq ($(KVM),1)
QEMU_ACCEL=-enable-kvm
else ifeq ($(KVM),auto)
ifeq ($(KVM_AVAILABLE),1)
QEMU_ACCEL=-enable-kvm
endif
endif

MKISOFS := $(shell if command -v genisoimage >/dev/null 2>&1; then echo genisoimage; elif command -v mkisofs >/dev/null 2>&1; then echo mkisofs; fi)
GRUB_MKRESCUE := $(shell if command -v grub-mkrescue >/dev/null 2>&1; then echo grub-mkrescue; fi)
XORRISO := $(shell if command -v xorriso >/dev/null 2>&1; then echo xorriso; fi)

CFLAGS=-m32 -ffreestanding -fno-stack-protector -fno-pie -nostdlib -Wall -Wextra -O2 \
       -Iinclude \
       -Iinclude/core \
       -Iinclude/drivers \
       -Iinclude/fs \
       -Iinclude/mem \
       -Iinclude/task \
       -Iinclude/video \
       -Iinclude/exec
CFLAGS_USER=-m32 -ffreestanding -fno-stack-protector -fno-pie -nostdlib -Wall -Wextra -O2
LDFLAGS=-m elf_i386 -nostdlib -T boot/kernel.ld
LDFLAGS_USER=-m elf_i386 -nostdlib -T user/user.ld
MAX_KERNEL_BYTES=98304
DISK_IMAGE_BYTES=33554432
ELTORITO_FLOPPY_BYTES=1474560

BUILD=build
SRC_DIR=src
BOOT_DIR=boot
USER_DIR=user

# Source files grouped by subsystem
CORE_SRCS  = $(addprefix $(SRC_DIR)/core/,    kernel.c cli.c commands.c editor.c panic.c)
DRIVER_SRCS= $(addprefix $(SRC_DIR)/drivers/, ata.c blockdev.c interrupts.c keyboard.c mouse.c pic.c pit.c power.c rtc.c serial.c pci.c rtl8139.c net.c)
FS_SRCS    = $(addprefix $(SRC_DIR)/fs/,      simplefs.c vfs.c)
MEM_SRCS   = $(addprefix $(SRC_DIR)/mem/,     bootinfo.c heap.c paging.c pmm.c)
TASK_SRCS  = $(addprefix $(SRC_DIR)/task/,    scheduler.c syscall.c)
VIDEO_SRCS = $(addprefix $(SRC_DIR)/video/,   display.c framebuffer.c video.c)
EXEC_SRCS  = $(addprefix $(SRC_DIR)/exec/,    exec.c userland.c)

C_SOURCES=$(CORE_SRCS) $(DRIVER_SRCS) $(FS_SRCS) $(MEM_SRCS) $(TASK_SRCS) $(VIDEO_SRCS) $(EXEC_SRCS)
C_OBJECTS=$(notdir $(C_SOURCES:.c=.o))
C_OBJECTS_BUILD=$(addprefix $(BUILD)/,$(C_OBJECTS))

all: $(BUILD)/os-image.bin

$(BUILD):
	mkdir -p $(BUILD)
	mkdir -p $(BUILD)/user

$(BUILD)/boot.bin: $(BOOT_DIR)/boot.asm | $(BUILD)
	$(ASM) -f bin $(BOOT_DIR)/boot.asm -o $(BUILD)/boot.bin

$(BUILD)/kernel_entry.o: $(BOOT_DIR)/kernel_entry.asm | $(BUILD)
	$(ASM) -f elf32 $(BOOT_DIR)/kernel_entry.asm -o $(BUILD)/kernel_entry.o

$(BUILD)/irq_stubs.o: $(BOOT_DIR)/interrupts.asm | $(BUILD)
	$(ASM) -f elf32 $(BOOT_DIR)/interrupts.asm -o $(BUILD)/irq_stubs.o

$(BUILD)/user/hello.o: $(USER_DIR)/hello.c $(USER_DIR)/user.ld | $(BUILD)
	$(CC) $(CFLAGS_USER) -c $(USER_DIR)/hello.c -o $(BUILD)/user/hello.o

$(BUILD)/user/hello.elf: $(BUILD)/user/hello.o $(USER_DIR)/user.ld | $(BUILD)
	$(LD) $(LDFLAGS_USER) -o $(BUILD)/user/hello.elf $(BUILD)/user/hello.o

$(BUILD)/user_hello_elf.o: $(BUILD)/user/hello.elf | $(BUILD)
	$(OBJCOPY) -I binary -O elf32-i386 -B i386 $(BUILD)/user/hello.elf $(BUILD)/user_hello_elf.o

$(BUILD)/%.o: $(SRC_DIR)/core/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: $(SRC_DIR)/drivers/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: $(SRC_DIR)/fs/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: $(SRC_DIR)/mem/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: $(SRC_DIR)/task/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: $(SRC_DIR)/video/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: $(SRC_DIR)/exec/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/kernel.elf: $(BUILD)/kernel_entry.o $(BUILD)/irq_stubs.o $(C_OBJECTS_BUILD) $(BUILD)/user_hello_elf.o $(BOOT_DIR)/kernel.ld
	$(LD) $(LDFLAGS) -o $(BUILD)/kernel.elf $(BUILD)/kernel_entry.o $(BUILD)/irq_stubs.o $(C_OBJECTS_BUILD) $(BUILD)/user_hello_elf.o

$(BUILD)/kernel.bin: $(BUILD)/kernel.elf
	$(OBJCOPY) -O binary $(BUILD)/kernel.elf $(BUILD)/kernel.bin
	@test $$(wc -c < $(BUILD)/kernel.bin) -le $(MAX_KERNEL_BYTES) || \
		(echo "kernel.bin is too large; increase KERNEL_SECTORS in $(BOOT_DIR)/boot.asm" && false)
	truncate -s $(MAX_KERNEL_BYTES) $(BUILD)/kernel.bin

$(BUILD)/os-image.bin: $(BUILD)/boot.bin $(BUILD)/kernel.bin
	cat $(BUILD)/boot.bin $(BUILD)/kernel.bin > $(BUILD)/os-image.bin
	truncate -s $(DISK_IMAGE_BYTES) $(BUILD)/os-image.bin

$(BUILD)/vertexos_floppy.img: $(BUILD)/boot.bin $(BUILD)/kernel.bin | $(BUILD)
	cat $(BUILD)/boot.bin $(BUILD)/kernel.bin > $(BUILD)/vertexos_floppy.img
	truncate -s $(ELTORITO_FLOPPY_BYTES) $(BUILD)/vertexos_floppy.img

$(BUILD)/vertexos.iso: $(BUILD)/vertexos_floppy.img | $(BUILD)
	mkdir -p $(BUILD)/iso
	cp $(BUILD)/vertexos_floppy.img $(BUILD)/iso/
	@if [ -z "$(MKISOFS)" ]; then echo "Install genisoimage or mkisofs to build ISO" && false; fi
	$(MKISOFS) -o $(BUILD)/vertexos.iso -b vertexos_floppy.img -c boot.catalog -R -J $(BUILD)/iso

$(BUILD)/vertexos-uefi.iso: $(BUILD)/kernel.elf $(BOOT_DIR)/grub.cfg | $(BUILD)
	rm -rf $(BUILD)/grub
	mkdir -p $(BUILD)/grub/boot/grub
	cp $(BUILD)/kernel.elf $(BUILD)/grub/kernel.elf
	cp $(BOOT_DIR)/grub.cfg $(BUILD)/grub/boot/grub/grub.cfg
	@if [ -z "$(GRUB_MKRESCUE)" ]; then echo "Install grub-mkrescue to build a UEFI ISO" && false; fi
	@if [ -z "$(XORRISO)" ]; then echo "Install xorriso so grub-mkrescue can build a UEFI ISO" && false; fi
	$(GRUB_MKRESCUE) -o $(BUILD)/vertexos-uefi.iso $(BUILD)/grub

uefi-iso: $(BUILD)/vertexos-uefi.iso

iso: $(BUILD)/vertexos.iso

run: $(BUILD)/os-image.bin
	$(QEMU) $(QEMU_ACCEL) -drive format=raw,file=$(BUILD)/os-image.bin $(QEMU_NET)

run-kvm: $(BUILD)/os-image.bin
	$(MAKE) KVM=1 run

run-tcg: $(BUILD)/os-image.bin
	$(MAKE) KVM=0 run

run-iso: $(BUILD)/vertexos.iso $(BUILD)/os-image.bin
	$(QEMU) $(QEMU_ACCEL) -drive format=raw,file=$(BUILD)/os-image.bin,if=ide,index=0 -drive file=$(BUILD)/vertexos.iso,format=raw,media=cdrom -boot d $(QEMU_NET)

run-uefi: $(BUILD)/vertexos-uefi.iso
	$(QEMU_UEFI) $(QEMU_ACCEL) -cdrom $(BUILD)/vertexos-uefi.iso $(QEMU_NET)

run-capture: $(BUILD)/os-image.bin
	$(QEMU) $(QEMU_ACCEL) -drive format=raw,file=$(BUILD)/os-image.bin $(QEMU_NET) \
		-object filter-dump,id=f1,netdev=n1,file=/tmp/vertexos-net.pcap

run-1080: $(BUILD)/os-image.bin
	$(QEMU) $(QEMU_ACCEL) -g 1080x720 -drive format=raw,file=$(BUILD)/os-image.bin

run-headless: $(BUILD)/os-image.bin
	$(QEMU) $(QEMU_ACCEL) -drive format=raw,file=$(BUILD)/os-image.bin $(QEMU_NET) -nographic

run-debug: $(BUILD)/os-image.bin
	@echo "Serial log -> /tmp/vertexos.log  (Ctrl+C to stop QEMU)"
	$(QEMU) $(QEMU_ACCEL) -drive format=raw,file=$(BUILD)/os-image.bin $(QEMU_NET) \
		-serial file:/tmp/vertexos.log

# Phase 8: Tests and quality checks
test: $(BUILD)/os-image.bin
	@echo "Running kernel integration tests..."
	@bash tests/run-kernel-tests.sh

check: $(BUILD)/os-image.bin
	@echo "=== Build Quality Checks ==="
	@echo "1. Checking for compiler warnings..."
	@make clean > /dev/null 2>&1 && make 2>&1 | grep -i "warning" || echo "✓ No warnings detected"
	@echo "2. Build size check..."
	@du -h $(BUILD)/os-image.bin
	@echo "3. Kernel size check..."
	@du -h $(BUILD)/kernel.elf
	@echo "Quality checks complete."

clean:
	rm -rf $(BUILD)
