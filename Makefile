ASM=nasm
CC=gcc
LD=ld
OBJCOPY=objcopy
QEMU=/usr/bin/qemu-system-i386

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
MAX_KERNEL_BYTES=49152
DISK_IMAGE_BYTES=33554432

BUILD=build
SRC_DIR=src
BOOT_DIR=boot
USER_DIR=user

# Source files grouped by subsystem
CORE_SRCS  = $(addprefix $(SRC_DIR)/core/,    kernel.c cli.c commands.c panic.c)
DRIVER_SRCS= $(addprefix $(SRC_DIR)/drivers/, ata.c blockdev.c interrupts.c keyboard.c pic.c pit.c power.c rtc.c serial.c)
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

run: $(BUILD)/os-image.bin
	$(QEMU) -drive format=raw,file=$(BUILD)/os-image.bin

run-headless: $(BUILD)/os-image.bin
	$(QEMU) -drive format=raw,file=$(BUILD)/os-image.bin -nographic

clean:
	rm -rf $(BUILD)
