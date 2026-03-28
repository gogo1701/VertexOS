ASM=nasm
CC=gcc
LD=ld
OBJCOPY=objcopy
QEMU=/usr/bin/qemu-system-i386

CFLAGS=-m32 -ffreestanding -fno-stack-protector -fno-pie -nostdlib -Wall -Wextra -O2
LDFLAGS=-m elf_i386 -nostdlib -T kernel.ld
MAX_KERNEL_BYTES=4096

all: os-image.bin

boot.bin: boot.asm
	$(ASM) -f bin boot.asm -o boot.bin

kernel_entry.o: kernel_entry.asm
	$(ASM) -f elf32 kernel_entry.asm -o kernel_entry.o

kernel.o: kernel.c
	$(CC) $(CFLAGS) -c kernel.c -o kernel.o

kernel.elf: kernel_entry.o kernel.o kernel.ld
	$(LD) $(LDFLAGS) -o kernel.elf kernel_entry.o kernel.o

kernel.bin: kernel.elf
	$(OBJCOPY) -O binary kernel.elf kernel.bin
	@test $$(wc -c < kernel.bin) -le $(MAX_KERNEL_BYTES) || \
		(echo "kernel.bin is too large; increase KERNEL_SECTORS in boot.asm" && false)
	truncate -s $(MAX_KERNEL_BYTES) kernel.bin

os-image.bin: boot.bin kernel.bin
	cat boot.bin kernel.bin > os-image.bin

run: os-image.bin
	$(QEMU) -drive format=raw,file=os-image.bin

run-headless: os-image.bin
	$(QEMU) -drive format=raw,file=os-image.bin -nographic

clean:
	rm -f boot.bin kernel_entry.o kernel.o kernel.elf kernel.bin os-image.bin
