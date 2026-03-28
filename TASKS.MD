# VertexOS Roadmap

Goal: build a stable, educational, and practically useful 32-bit OS with a clear path to 64-bit.

## Phase 0 - Foundation (done)
- [x] Working bootloader
- [x] Basic command input
- [x] Clean command system (modular command registration and parser)
- [x] Project structure cleanup (src/include/boot, modular build)

Definition of done:
- Clean build from scratch with one command
- Reproducible QEMU run
- Core modules separated and documented

## Phase 1 - Reliable Kernel Core
- [ ] Interrupt Descriptor Table (IDT) setup
- [ ] Programmable Interrupt Controller (PIC) remap and masking helpers
- [ ] Timer (PIT) interrupt at fixed tick rate
- [ ] Keyboard interrupt driver (replace polling)
- [ ] Basic panic/assert path with error output

Definition of done:
- Keyboard input works entirely via interrupts
- Timer ticks are visible/usable in kernel
- Faults print useful diagnostics instead of silent hangs

## Phase 2 - Memory Management
- [ ] Physical Memory Manager (bitmap or stack of free frames)
- [ ] Paging enabled (identity map kernel, guard critical regions)
- [ ] Kernel heap allocator (kmalloc/kfree)
- [ ] Memory map parsing and validation

Definition of done:
- Kernel can allocate/free dynamic memory safely
- Paging is stable under stress tests
- Memory corruption bugs are detectable with debug checks

## Phase 3 - Process and Scheduling
- [ ] Task structure and context switch assembly path
- [ ] Basic round-robin scheduler
- [ ] User/kernel mode separation ring transitions
- [ ] Minimal syscall interface

Definition of done:
- Multiple tasks run without freezing system
- Syscalls work from user tasks
- Crashed task does not always crash whole kernel

## Phase 4 - File System and Storage
- [ ] Block device abstraction (start with ATA PIO or virtio-blk later)
- [ ] VFS layer (open/read/write/close abstractions)
- [ ] Simple filesystem (recommended: FAT12/FAT16 first)
- [ ] Path parser and current working directory support
- [ ] File permissions model (simple initially)

Definition of done:
- Can create/read/write/delete files and directories
- Data persists across reboot
- Shell can load commands/programs from disk

## Phase 5 - Useful Shell and Userland
- [ ] Command parser with arguments, quotes, and help text
- [ ] Built-in commands: help, clear, echo, ls, cat, mkdir, rm, cp, mv
- [ ] Program loader (ELF preferred)
- [ ] User programs toolchain integration

Definition of done:
- OS is usable from shell for real file operations
- External user programs can run from disk
- Error messages are understandable and consistent

## Phase 6 - Device and Console Improvements
- [ ] Better terminal (line editing, history, tab completion)
- [ ] Serial logging (COM1) for debug output
- [ ] RTC/clock readout
- [ ] Optional: basic graphics mode and framebuffer text

Definition of done:
- Debugging works via serial even when VGA fails
- Terminal interaction is comfortable for daily use

## Phase 7 - Networking (optional but highly useful)
- [ ] NIC driver (start with emulated card in QEMU)
- [ ] Minimal network stack: ARP, IPv4, ICMP, UDP, TCP basics
- [ ] DHCP client and simple tools (ping)

Definition of done:
- OS gets IP automatically in QEMU network setup
- ping works to gateway/host

## Phase 8 - Hardening and Quality
- [ ] Unit-style tests for kernel subsystems where possible
- [ ] Integration tests in QEMU scripts
- [ ] Static analysis and warning-free build
- [ ] Crash dump/log format for postmortem debugging

Definition of done:
- Repeatable CI build/test pass
- Regressions are caught automatically
- Clear release checklist exists

## Phase 9 - Toward Production-Grade Architecture
- [ ] Migrate or dual-target 64-bit long mode
- [ ] SMP basics (multi-core bring-up)
- [ ] Improved scheduler and memory protections
- [ ] Security hardening (stack canaries in userland, syscall validation)

Definition of done:
- Stable on 64-bit path
- Core subsystems support future scaling

## Suggested Milestone Releases
- [ ] v0.1: Boot + interrupt-driven console + modular command system
- [ ] v0.2: Paging + heap + stable kernel diagnostics
- [ ] v0.3: Filesystem + VFS + persistent shell commands
- [ ] v0.4: User programs + syscalls + process scheduler
- [ ] v0.5: Networking basics + improved terminal

## Immediate Next 2 Weeks (practical sprint)
- [ ] Refactor into clean folders and module boundaries
- [ ] Finish clean command system with registration table and parser cleanup
- [ ] Implement IDT + PIC remap + keyboard IRQ handler
- [ ] Add timer tick and uptime command
- [ ] Start block device abstraction skeleton for filesystem work

## Success Criteria for a "Useful OS"
- Boots reliably every time
- Interactive shell is responsive and supports real file operations
- Runs multiple user programs
- Persists data across reboot
- Has logs, diagnostics, and repeatable tests
