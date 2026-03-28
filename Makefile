ASM=nasm
CC=gcc
LD=ld
OBJCOPY=objcopy
QEMU=/usr/bin/qemu-system-i386

CFLAGS=-m32 -ffreestanding -fno-stack-protector -fno-pie -nostdlib -Wall -Wextra -O2 -Iinclude
LDFLAGS=-m elf_i386 -nostdlib -T boot/kernel.ld
MAX_KERNEL_BYTES=4096

BUILD=build
SRC_DIR=src
BOOT_DIR=boot

# Source files
C_SOURCES=$(addprefix $(SRC_DIR)/,kernel.c cli.c display.c keyboard.c commands.c)
C_OBJECTS=$(notdir $(C_SOURCES:.c=.o))
C_OBJECTS_BUILD=$(addprefix $(BUILD)/,$(C_OBJECTS))

all: $(BUILD)/os-image.bin

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/boot.bin: $(BOOT_DIR)/boot.asm | $(BUILD)
	$(ASM) -f bin $(BOOT_DIR)/boot.asm -o $(BUILD)/boot.bin

$(BUILD)/kernel_entry.o: $(BOOT_DIR)/kernel_entry.asm | $(BUILD)
	$(ASM) -f elf32 $(BOOT_DIR)/kernel_entry.asm -o $(BUILD)/kernel_entry.o

$(BUILD)/%.o: $(SRC_DIR)/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/kernel.elf: $(BUILD)/kernel_entry.o $(C_OBJECTS_BUILD) $(BOOT_DIR)/kernel.ld
	$(LD) $(LDFLAGS) -o $(BUILD)/kernel.elf $(BUILD)/kernel_entry.o $(C_OBJECTS_BUILD)

$(BUILD)/kernel.bin: $(BUILD)/kernel.elf
	$(OBJCOPY) -O binary $(BUILD)/kernel.elf $(BUILD)/kernel.bin
	@test $$(wc -c < $(BUILD)/kernel.bin) -le $(MAX_KERNEL_BYTES) || \
		(echo "kernel.bin is too large; increase KERNEL_SECTORS in $(BOOT_DIR)/boot.asm" && false)
	truncate -s $(MAX_KERNEL_BYTES) $(BUILD)/kernel.bin

$(BUILD)/os-image.bin: $(BUILD)/boot.bin $(BUILD)/kernel.bin
	cat $(BUILD)/boot.bin $(BUILD)/kernel.bin > $(BUILD)/os-image.bin

run: $(BUILD)/os-image.bin
	$(QEMU) -drive format=raw,file=$(BUILD)/os-image.bin

run-headless: $(BUILD)/os-image.bin
	$(QEMU) -drive format=raw,file=$(BUILD)/os-image.bin -nographic

clean:
	rm -rf $(BUILD)
