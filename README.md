# VertexOS

A modular 32-bit x86 operating system focused on practical learning and rapid iteration.

VertexOS boots via a custom BIOS bootloader, switches to protected mode, and provides a usable shell with persistent files, task scheduling, syscalls, ELF loading, serial logs, and optional graphics mode.

## At A Glance

| Area | Status |
|---|---|
| Boot + protected mode | Complete |
| Interrupts + timer + keyboard IRQ | Complete |
| Memory (PMM + paging + heap) | Complete |
| Scheduler + syscall base | Complete |
| Filesystem + VFS + persistence | Complete |
| Userland ELF loading | Complete |
| Terminal UX (history/tab/editing) | Complete |
| Serial + RTC diagnostics | Complete |
| Networking | Complete |

Roadmap: [TASKS.MD](TASKS.MD)

## Quick Start

### 1) Install prerequisites

```bash
sudo apt update
sudo apt install -y nasm gcc binutils qemu-system-x86
```

### 2) Build and run

```bash
make
make run
```

### 3) Try commands in the shell

```text
help
uptime
ls /
mkdir /docs
write /docs/readme "hello from vertexos"
cat /docs/readme
time
```

## Core Capabilities

- Bootloader with protected-mode handoff and E820 memory map transfer
- Interrupt subsystem (IDT, PIC remap, PIT timer, keyboard IRQ)
- Panic/assert diagnostics and serial output mirror (COM1)
- Physical memory manager, paging, and kernel heap allocator
- Round-robin task scheduler and int 0x80 syscall interface
- Block device layer + ATA PIO + SimpleFS + VFS path/cwd operations
- Shell with quoted arguments, editing, history, and tab completion
- ELF loader and seeded sample app under /bin
- Video mode manager with boot-applied text/graphics preference
- RTL8139 NIC driver + polling network path
- Minimal network stack (ARP, IPv4, ICMP, UDP, TCP parsing basics)
- DHCP client flow and ICMP ping command
- Power control commands: restart and shutdown

## Command Reference

Use help in the shell for full details.

### System and diagnostics

- help [command]
- clear
- echo [text]
- uptime
- time
- panic [message]
- meminfo
- alloc
- tasks
- yield
- syscall

### Filesystem and paths

- ls [path]
- cat <path>
- write <path> <text>
- touch <path>
- mkdir <path>
- rm <path>
- cp <src> <dst>
- mv <src> <dst>
- cd [path]
- pwd

### Program and video

- exec <elf-path>
- edit <path>
- video status
- video text
- video gfx
- video test [on|off|status]

### Power

- restart
- shutdown

### Networking

- ifconfig
- dhcp
- ping <ipv4> [timeout_ms]
- dns <hostname|ipv4> [timeout_ms]

## Video Mode Behavior

Video switching is intentionally boot-applied for reliability.

- video text and video gfx store next-boot preference in the boot sector
- Reboot is required for mode changes to take effect
- video test controls only the graphics diagnostics overlay in graphics mode

## Project Layout

```text
VertexOS/
  boot/
    boot.asm
    kernel_entry.asm
    interrupts.asm
    kernel.ld

  include/
    types.h
    io.h
    core/
    drivers/
    fs/
    mem/
    task/
    video/
    exec/

  src/
    core/
    drivers/
    fs/
    mem/
    task/
    video/
    exec/

  user/
    hello.c
    user.ld

  docs/
    DEVELOPING.md
    api/

  Makefile
  TASKS.MD
```

## Build Targets

```bash
make                  # build os image
make run              # run in qemu window
make run-headless     # run in terminal mode
make clean            # remove build artifacts
```

Important artifacts:

- build/boot.bin
- build/kernel.elf
- build/kernel.bin
- build/os-image.bin

## Architecture Snapshot

Boot flow:

1. BIOS loads boot/boot.asm from sector 0
2. Bootloader loads kernel payload and reads E820 map
3. Bootloader applies saved video preference
4. CPU enters 32-bit protected mode
5. boot/kernel_entry.asm calls kmain
6. src/core/kernel.c initializes subsystems and starts scheduler

Runtime model:

- CLI runs as a scheduled task
- Idle task halts CPU and yields on timer preemption points
- Keyboard and timer are IRQ-driven
- Shell commands operate through subsystem APIs (VFS, memory, scheduler, video, exec)

## Developer Documentation Hub

Start here:

- [docs/DEVELOPING.md](docs/DEVELOPING.md)

API references:

- [docs/api/commands.md](docs/api/commands.md)
- [docs/api/filesystem.md](docs/api/filesystem.md)
- [docs/api/display.md](docs/api/display.md)
- [docs/api/memory.md](docs/api/memory.md)
- [docs/api/tasks.md](docs/api/tasks.md)
- [docs/api/exec.md](docs/api/exec.md)
- [docs/api/drivers.md](docs/api/drivers.md)

Guides:

- [docs/DEVELOPING.md](docs/DEVELOPING.md)
- [docs/guides/creating-commands-and-apps.md](docs/guides/creating-commands-and-apps.md)

## Current Limits and Settings

- 32-bit freestanding kernel build (no libc)
- Kernel payload limit: MAX_KERNEL_BYTES=65536 in Makefile
- Disk image size: DISK_IMAGE_BYTES=33554432 in Makefile
- If kernel grows, update both:
1. MAX_KERNEL_BYTES in [Makefile](Makefile)
2. KERNEL_SECTORS in [boot/boot.asm](boot/boot.asm)

## Troubleshooting

### Kernel size check fails

1. Increase MAX_KERNEL_BYTES in [Makefile](Makefile)
2. Increase KERNEL_SECTORS in [boot/boot.asm](boot/boot.asm)
3. Rebuild with make clean && make

### File operations fail unexpectedly

Ensure you are booting the latest padded image at build/os-image.bin, not an older small image.

### Need serial-first debugging

Use:

```bash
make run-headless
```

## Networking Notes

Networking is implemented for QEMU user networking via RTL8139.

- `make run` and `make run-headless` now start QEMU with an RTL8139 NIC
- `ifconfig` prints link and IPv4 config state
- `dhcp` requests a lease from QEMU's built-in DHCP server
- `ping <ip>` sends one ICMP echo request
- `dns <host>` resolves an A record using the DHCP-provided DNS server

For gateway testing in default QEMU user-net setup, try:

```text
dhcp
ping 10.0.2.2
```

## License

Currently none, normal copyright rules apply.