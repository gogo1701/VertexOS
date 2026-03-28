ASM=nasm
CC=gcc
LD=ld
OBJCOPY=objcopy
QEMU=/usr/bin/qemu-system-i386

CFLAGS=-m32 -ffreestanding -fno-stack-protector -fno-pie -nostdlib -Wall -Wextra -O2 -Iinclude
CFLAGS_USER=-m32 -ffreestanding -fno-stack-protector -fno-pie -nostdlib -Wall -Wextra -O2
LDFLAGS=-m elf_i386 -nostdlib -T boot/kernel.ld
LDFLAGS_USER=-m elf_i386 -nostdlib -T user/user.ld
MAX_KERNEL_BYTES=49152
DISK_IMAGE_BYTES=33554432

BUILD=build
SRC_DIR=src
BOOT_DIR=boot
USER_DIR=user

# Source files
C_SOURCES=$(addprefix $(SRC_DIR)/,kernel.c cli.c display.c keyboard.c commands.c interrupts.c pic.c pit.c panic.c power.c bootinfo.c pmm.c paging.c heap.c scheduler.c syscall.c ata.c blockdev.c simplefs.c vfs.c exec.c userland.c serial.c rtc.c framebuffer.c video.c)
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

$(BUILD)/%.o: $(SRC_DIR)/%.c | $(BUILD)
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
