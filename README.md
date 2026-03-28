# VertexOS

VertexOS is a modular 32-bit x86 operating system for learning and building.
It boots with a custom BIOS bootloader, enters protected mode, and provides a
shell, filesystem, task scheduler, syscalls, ELF program loading, serial logs,
and optional graphics mode.

The project is designed to be educational first, while still being practical
for iterative OS development.

## Current Status

Completed roadmap phases:

- Phase 0: foundation and modular project layout
- Phase 1: interrupts, PIC, PIT, keyboard IRQ, panic/assert
- Phase 2: memory map parsing, PMM, paging, heap
- Phase 3: tasking, scheduler, syscall base
- Phase 4: block device, VFS, persistent filesystem and paths
- Phase 5: richer shell, ELF loader, userland integration
- Phase 6: history/tab-completion, serial output, RTC time command

Pending:

- Phase 7+: networking, hardening, testing/CI, long-term 64-bit path

See roadmap: [TASKS.MD](TASKS.MD)

## Features

- BIOS bootloader and protected-mode kernel entry
- Interrupt-driven keyboard input (no polling shell loop)
- PIT timer tick and uptime reporting
- Panic/assert diagnostics
- PMM + paging + kernel heap allocator
- Round-robin task scheduler
- int 0x80 syscall interface
- ATA PIO block driver + VFS + SimpleFS
- Persistent file and directory operations
- Command-line shell with:
  - quoted argument parsing
  - line editing
  - history
  - tab completion
- ELF program loader and seeded sample user program
- COM1 serial logging mirror for debugging
- RTC date/time readout
- Video mode manager:
  - text mode by default
  - graphics mode as boot preference
  - optional graphics test overlay in gfx mode
- Power commands:
  - restart
  - shutdown

## Repository Layout

```text
VertexOS/
  boot/
    boot.asm              Stage-1 bootloader (real mode)
    kernel_entry.asm      Protected-mode entry and C handoff
    interrupts.asm        ISR/IRQ/syscall stubs and context switch helpers
    kernel.ld             Kernel linker script

  include/
    types.h               Common integer types
    io.h                  Low-level port I/O helpers
    core/                 CLI, commands, panic headers
    drivers/              ATA, PIC, PIT, keyboard, serial, RTC, power, etc.
    fs/                   VFS + SimpleFS headers
    mem/                  bootinfo, PMM, paging, heap headers
    task/                 scheduler + syscall headers
    video/                display, framebuffer, video manager headers
    exec/                 ELF loader and userland seeding headers

  src/
    core/                 kernel.c, cli.c, commands.c, panic.c
    drivers/              low-level hardware drivers
    fs/                   filesystem and VFS implementation
    mem/                  memory subsystem implementation
    task/                 scheduler and syscall implementation
    video/                text/graphics display implementation
    exec/                 ELF execution and seeded user program support

  user/
    hello.c               Sample user program
    user.ld               User program linker script

  docs/
    DEVELOPING.md         Developer workflow guide
    api/                  Subsystem API references

  Makefile                Build, image, and run targets
  TASKS.MD                Project roadmap and milestone tracking
```

## Build Requirements

On Ubuntu/Debian:

```bash
sudo apt update
sudo apt install -y nasm gcc binutils qemu-system-x86
```

## Build and Run

```bash
make                  # Build build/os-image.bin
make run              # Run in QEMU (windowed)
make run-headless     # Run in QEMU with -nographic
make clean            # Remove build artifacts
```

Build artifacts:

- boot sector: `build/boot.bin`
- linked kernel: `build/kernel.elf`
- kernel binary payload: `build/kernel.bin`
- disk image: `build/os-image.bin`

## Shell Commands

Use `help` in-shell for the authoritative list.

Core and diagnostics:

- `help [command]`
- `clear`
- `echo [text]`
- `uptime`
- `time`
- `panic [message]`
- `meminfo`
- `alloc`
- `tasks`
- `yield`
- `syscall`

Filesystem and paths:

- `ls [path]`
- `cat <path>`
- `write <path> <text>`
- `touch <path>`
- `mkdir <path>`
- `rm <path>`
- `cp <src> <dst>`
- `mv <src> <dst>`
- `cd [path]`
- `pwd`

Program and video:

- `exec <elf-path>`
- `video status`
- `video text`
- `video gfx`
- `video test [on|off|status]`

Power:

- `restart`
- `shutdown`

## Video Mode Notes

Video mode selection is boot-applied (safer than live protected-mode mode
switching in this setup).

- `video text` and `video gfx` save next-boot preference in the boot sector
- Reboot is required for the selected mode to apply
- `video test` controls the graphics diagnostics overlay only when currently
  booted into graphics mode

## Developer Documentation

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

## Typical Development Tasks

### Add a new shell command

- Implement command handler in `src/core/commands.c`
- Register it in `commands_init()`
- Rebuild and test with `make run`

Detailed walkthrough: [docs/api/commands.md](docs/api/commands.md)

### Add a new user program

- Add source under `user/`
- Add build/link rules in `Makefile`
- Embed resulting ELF object into kernel link
- Seed program via `src/exec/userland.c`
- Run with `exec /bin/<name>.elf`

Detailed walkthrough: [docs/api/exec.md](docs/api/exec.md)

### Add a new subsystem module

- Put headers in the matching `include/<group>/`
- Put implementation in the matching `src/<group>/`
- Add source file to the grouped source list in `Makefile`
- Initialize subsystem from `src/core/kernel.c` when needed

## Architecture Summary

Boot sequence:

1. BIOS loads `boot/boot.asm` (sector 0)
2. Bootloader reads kernel payload, gathers E820 memory map
3. Bootloader applies saved video preference via BIOS
4. CPU enters protected mode
5. `boot/kernel_entry.asm` sets stack and calls `kmain`
6. `src/core/kernel.c` initializes subsystems and starts scheduler

Runtime model:

- CLI runs as a task under the scheduler
- Idle task executes HLT and cooperates with preemption checks
- Keyboard and timer are IRQ-driven
- Shell commands call subsystem APIs (VFS, memory, tasking, video, etc.)

## Constraints and Important Settings

- 32-bit freestanding kernel (`-m32`, no libc)
- Kernel binary padded to `MAX_KERNEL_BYTES` (currently 49152)
- Disk image padded to `DISK_IMAGE_BYTES` (currently 33554432)
- If kernel size grows, update both:
  - `MAX_KERNEL_BYTES` in `Makefile`
  - `KERNEL_SECTORS` in `boot/boot.asm`

## Troubleshooting

### Build fails due to kernel size

If the kernel exceeds the configured payload size:

1. Increase `MAX_KERNEL_BYTES` in `Makefile`
2. Increase `KERNEL_SECTORS` in `boot/boot.asm`
3. Rebuild with `make clean && make`

### Filesystem operations fail on image

Ensure `build/os-image.bin` is the padded image produced by current Makefile.
Do not run with an old tiny image.

### Need serial logs

Use:

```bash
make run-headless
```

or run QEMU manually with serial routed to stdio.

## License

No explicit license file is included currently. Add one if you plan to
redistribute or accept external contributions.
