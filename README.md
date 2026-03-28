# BIOS Bootloader + C Kernel (x86, NASM + GCC)

This folder now builds a tiny x86 OS image with:

- a 16-bit BIOS boot sector in assembly (`boot.asm`)
- a small 32-bit freestanding C kernel (`kernel.c`)

Flow:

1. BIOS loads the boot sector at `0x7C00`
2. Boot sector reads kernel sectors into memory at `0x10000`
3. Boot sector switches to 32-bit protected mode
4. Control jumps to the C kernel entry and prints a message to VGA text memory

## Ubuntu setup

Install tools:

```bash
sudo apt update
sudo apt install -y nasm gcc binutils qemu-system-x86
```

## Build

```bash
cd /home/georgi/Desktop/OS
make
```

This creates `os-image.bin`.

## Run in QEMU

```bash
cd /home/georgi/Desktop/OS
make run
```

You should see:

`Hello from C kernel!`

## Manual commands (without Makefile)

```bash
nasm -f bin boot.asm -o boot.bin
nasm -f elf32 kernel_entry.asm -o kernel_entry.o
gcc -m32 -ffreestanding -fno-stack-protector -fno-pie -nostdlib -c kernel.c -o kernel.o
ld -m elf_i386 -nostdlib -T kernel.ld -o kernel.elf kernel_entry.o kernel.o
objcopy -O binary kernel.elf kernel.bin
cat boot.bin kernel.bin > os-image.bin
qemu-system-i386 -drive format=raw,file=os-image.bin
```

## Notes

- `boot.asm` currently loads `8` sectors for the kernel.
- `Makefile` checks `kernel.bin` size and fails if it exceeds 4096 bytes.
- If kernel size grows, increase `KERNEL_SECTORS` in `boot.asm` and `MAX_KERNEL_BYTES` in `Makefile` together.
