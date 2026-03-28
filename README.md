# VertexOS - Modular 32-bit Kernel

A minimal x86 operating system demonstrating OS fundamentals with a modular, extensible command system.

## Project Structure

```
├── boot.asm             # Stage 1 bootloader (512 bytes)
├── kernel_entry.asm     # 32-bit kernel entry point
├── kernel.c             # Main kernel & command-line interface
├── kernel.ld            # Linker script
├── types.h              # Common type definitions
├── io.h                 # Low-level I/O port operations
├── display.h/.c         # VGA display management
├── keyboard.h/.c        # Keyboard input handling
├── commands.h/.c        # Command system
├── cli.h/.c             # Command-line interface
├── Makefile            # Build configuration
└── build/              # Build artifacts
```

## Building & Running

### Ubuntu Setup

Install tools:

```bash
sudo apt update
sudo apt install -y nasm gcc binutils qemu-system-x86
```

### Build & Run

```bash
make                # Build the OS image
make run            # Run in QEMU
make run-headless   # Run without GUI
make clean          # Clean build artifacts
```

## Adding New Commands

### Method 1: Quick Addition (Edit commands.c)

Add your command function and register it in `commands_init()`:

```c
/* In commands.c */

static void cmd_hello(const char* args) {
    display_print("Hello, ");
    display_print(args);
    display_print("!\n");
}

void commands_init(void) {
    command_register("help", cmd_help);
    command_register("clear", cmd_clear);
    command_register("echo", cmd_echo);
    command_register("hello", cmd_hello);  // Add this line
}
```

Run `make clean && make run` to rebuild and test.

### Method 2: Separate Module (Advanced)

Create `mymodule.h`:

```c
#ifndef MYMODULE_H
#define MYMODULE_H

void mymodule_init(void);

#endif
```

Create `mymodule.c`:

```c
#include "mymodule.h"
#include "commands.h"
#include "display.h"

static void cmd_mycommand(const char* args) {
    display_print("My command: ");
    display_print(args);
    display_put_char('\n');
}

void mymodule_init(void) {
    command_register("mycommand", cmd_mycommand);
}
```

Update `kernel.c` to call `mymodule_init()` after `commands_init()`.

Update `Makefile`:
```makefile
C_SOURCES=kernel.c display.c keyboard.c commands.c mymodule.c
```

## Architecture Overview

**Boot Sequence:**
1. BIOS loads `boot.asm` (512 bytes)
2. `boot.asm` reads kernel from disk, enables protected mode
3. `kernel_entry.asm` sets up stack, calls `kmain()`
4. `kmain()` initializes subsystems and enters command loop

**Subsystems:**

- **display.c** - VGA text mode (80×25)
  - Character output with automatic scrolling
  - Hardware cursor positioning
  
- **keyboard.c** - PS/2 keyboard input
  - Scancode-to-ASCII conversion (US layout)
  - IRQ-driven buffered input (no busy-poll loop)

- **interrupts.c / interrupts.asm / pic.c / pit.c** - Core IRQ subsystem
  - IDT setup with timer and keyboard IRQ gates
  - PIC remap + IRQ masking helpers
  - PIT fixed-rate timer ticks

- **panic.c** - Kernel panic and assert diagnostics
  - Fatal error output with halt path

- **commands.c** - Command registry & execution
  - Simple registration system (max 16 commands)
  - Built-in: `help`, `clear`, `echo`
  
- **io.h** - Low-level I/O operations
  - `io_inb()` - Read from port
  - `io_outb()` - Write to port

## Key Concepts

- **Memory-mapped I/O** - VGA buffer at 0xB8000
- **Port-based I/O** - Keyboard & cursor control
- **Modular design** - Separate, reusable components
- **32-bit protected mode** - Modern x86 CPU mode
- **Multi-stage bootloader** - Real boot process

## Built-in Commands

```
help      # Show available commands
clear     # Clear screen
echo TEXT # Echo text to console
```

## Example Session

```
VertexOS - Simple Console
Type 'help' for available commands.

> help
Available commands:
  help
  clear
  echo

> echo Hello from VertexOS
Hello from VertexOS

> clear
(screen clears and prompt reappears)
```
objcopy -O binary kernel.elf kernel.bin
cat boot.bin kernel.bin > os-image.bin
qemu-system-i386 -drive format=raw,file=os-image.bin
```

## Notes

- `boot.asm` currently loads `8` sectors for the kernel.
- `Makefile` checks `kernel.bin` size and fails if it exceeds 4096 bytes.
- If kernel size grows, increase `KERNEL_SECTORS` in `boot.asm` and `MAX_KERNEL_BYTES` in `Makefile` together.
