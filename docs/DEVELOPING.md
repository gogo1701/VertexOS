# Developer Guide — Extending VertexOS

This guide covers the most common extension tasks a new developer will want
to do: adding shell commands, writing custom ELF apps, extending syscalls,
and wiring new background tasks. Read the per-subsystem API files in
`docs/api/` for full reference.

---

## Project layout

```
boot/          Bootloader (boot.asm), protected-mode entry, IDT stubs
               and the linker script

src/
  core/        Kernel entry (kernel.c), shell loop (cli.c),
               command table (commands.c), panic
  drivers/     ATA, block device, interrupts, keyboard, PIC, PIT,
               power, RTC, serial
  fs/          SimpleFS on-disk format + VFS file descriptor layer
  mem/         Physical memory manager, paging, heap allocator
  task/        Round-robin scheduler + int 0x80 syscall dispatch
  video/       VGA text display, mode 13h framebuffer, video mode manager
  exec/        ELF loader, initial /bin/ seeding

include/       Headers mirroring the src/ subdirectory structure
               include/types.h and include/io.h live at the root

user/          User-mode sample programs (linked as separate ELFs)
docs/          This documentation tree
build/         Generated artefacts (git-ignored)
```

---

## How to add a shell command

This is the most common extension task.

### 1. Forward-declare and implement in `src/core/commands.c`

```c
static void cmd_greet(const char* args);   /* add to the forward declarations */

static void cmd_greet(const char* args) {
    char name[32];
    if (!read_arg(&args, name, sizeof(name)) || !name[0]) {
        display_print("Usage: greet <name>\n");
        return;
    }
    display_print("Hello, ");
    display_print(name);
    display_put_char('!');
    display_put_char('\n');
}
```

### 2. Register at the end of `commands_init()`

```c
command_register_full(
    "greet",              /* shell name                */
    "greet <name>",       /* usage shown by 'help'     */
    "Print a greeting",   /* one-line description      */
    cmd_greet             /* function pointer          */
);
```

### 3. Build and test

```sh
make run
```

The `help` command and tab-completion pick up new commands automatically —
no other changes are needed.

### Rules for command handlers

- Never return a value; the signature is always `void cmd_xxx(const char* args)`.
- `args` is trimmed of leading spaces but is never NULL.
- Use `display_print()` / `display_put_char()` / `display_print_num()` for output.
- Use `vfs_open()` / `vfs_read()` / `vfs_write()` / `vfs_close()` for files.
- Call `display_print("Usage: ...\n")` when arguments are missing, do not panic.

---

## How to write a user-mode program

User programs are compiled as separate 32-bit ELF executables and seeded
into `/bin/` on the virtual filesystem at boot.

### 1. Create `user/myapp.c`

```c
/* Kernel headers are available because the program runs in Ring 0 for now. */
#include "syscall.h"
#include "display.h"

void _start(void) {
    display_print("myapp: running!\n");
    syscall_invoke(SYS_YIELD, 0, 0, 0);
}
```

### 2. Add build rules to `Makefile`

```makefile
$(BUILD)/user/myapp.o: user/myapp.c user/user.ld | $(BUILD)
	$(CC) $(CFLAGS_USER) -c user/myapp.c -o $(BUILD)/user/myapp.o

$(BUILD)/user/myapp.elf: $(BUILD)/user/myapp.o user/user.ld | $(BUILD)
	$(LD) $(LDFLAGS_USER) -o $(BUILD)/user/myapp.elf $(BUILD)/user/myapp.o

$(BUILD)/user_myapp_elf.o: $(BUILD)/user/myapp.elf | $(BUILD)
	$(OBJCOPY) -I binary -O elf32-i386 -B i386 \
	    $(BUILD)/user/myapp.elf $(BUILD)/user_myapp_elf.o
```

Add `$(BUILD)/user_myapp_elf.o` to the `$(BUILD)/kernel.elf` linker line.

### 3. Seed it in `src/exec/userland.c`

```c
extern u8 _binary_build_user_myapp_elf_start[];
extern u8 _binary_build_user_myapp_elf_end[];

{
    s32 fd = vfs_open("/bin/myapp.elf", VFS_O_WRITE | VFS_O_CREATE | VFS_O_TRUNC);
    if (fd >= 0) {
        u32 size = (u32)(_binary_build_user_myapp_elf_end - _binary_build_user_myapp_elf_start);
        (void)vfs_write(fd, _binary_build_user_myapp_elf_start, size);
        vfs_close(fd);
    }
}
```

### 4. Run from the shell

```
/> exec /bin/myapp.elf
```

See [docs/api/exec.md](api/exec.md) for full details.

### 5. Add a command to launch your app quickly (optional)

If you want a short alias instead of typing `exec /bin/myapp.elf` each time,
add a command handler in `src/core/commands.c`:

```c
static void cmd_myapp(const char* args) {
    (void)args;
    (void)exec_run_elf("/bin/myapp.elf");
}

/* inside commands_init() */
command_register_full("myapp", "myapp", "Run /bin/myapp.elf", cmd_myapp);
```

---

## How to add a background kernel task

```c
#include "scheduler.h"
#include "interrupts.h"

static void my_background_task(void* arg) {
    (void)arg;
    for (;;) {
        /* periodic work here */
        scheduler_yield();   /* give other tasks a turn */
    }
}

/* In kmain(), before scheduler_start() */
scheduler_create_task(my_background_task, NULL, "mybg", TASK_MODE_KERNEL);
```

Tasks must not return.  Use `scheduler_yield()` liberally; a task that
never yields will starve the keyboard handler and the CLI.

---

## How to add a new syscall

1. **Add a constant** in `include/task/syscall.h`:
   ```c
   SYS_MY_CALL = 3,
   ```
2. **Handle it** in `src/task/syscall.c` inside `syscall_handler()`:
   ```c
   case SYS_MY_CALL:
       return my_kernel_function(regs->ebx, regs->ecx, regs->edx);
   ```
3. **Call it** from user or kernel code:
   ```c
   u32 result = syscall_invoke(SYS_MY_CALL, arg0, arg1, arg2);
   ```

---

## Common APIs quick-reference

| Task | Header | Key functions |
|------|--------|---------------|
| Print output | `display.h` | `display_print`, `display_put_char`, `display_print_num` |
| Read/write files | `vfs.h` | `vfs_open`, `vfs_read`, `vfs_write`, `vfs_close` |
| Dynamic memory | `heap.h` | `kmalloc`, `kfree` |
| Time / uptime | `pit.h`, `rtc.h` | `pit_get_ticks`, `rtc_read_datetime` |
| Multitasking | `scheduler.h` | `scheduler_create_task`, `scheduler_yield` |
| Syscalls | `syscall.h` | `syscall_invoke` |
| Panic / assert | `panic.h` | `panic`, `KASSERT` |
| Serial debug | `serial.h` | `serial_write`, `serial_write_char` |
| Pixel graphics | `framebuffer.h` | `framebuffer_put_pixel`, `framebuffer_fill_rect` |

---

## Build system

```sh
make              # build the full OS image
make run          # build and launch in QEMU
make run-headless # build and launch with serial on stdout
make run-capture  # launch and dump traffic to /tmp/vertexos-net.pcap
make clean        # remove all build artefacts
```

The disk image is padded to **32 MiB** so the VFS has plenty of space.
The kernel image is padded to **64 KiB**; if you grow it past that, increase
`MAX_KERNEL_BYTES` in the Makefile and `KERNEL_SECTORS` in `boot/boot.asm`.

---

## New implementation notes

### Network commands and stack

- Shell commands: `ifconfig`, `dhcp`, `ping`, `dns`
- Driver path: `rtl8139` (NIC) + `net` (ARP/IPv4/ICMP/UDP/DHCP/DNS)
- In command code, always check `net_is_ready()` before network operations.
- Keep driver debug logs disabled by default:
    - `#define NET_DEBUG 0` in `src/drivers/net.c`
    - `#define RTL8139_DEBUG 0` in `src/drivers/rtl8139.c`

### Command metadata and parser behavior

- Register commands with `command_register_full(name, usage, summary, func)`.
- `help` output is generated from this metadata automatically.
- The argument helpers support quoted tokens for paths/text with spaces.

For a dedicated hands-on walkthrough, see
[docs/guides/creating-commands-and-apps.md](guides/creating-commands-and-apps.md).

---

## Documentation

| File | Contents |
|------|----------|
| [docs/api/commands.md](api/commands.md) | Shell command system |
| [docs/api/filesystem.md](api/filesystem.md) | VFS and SimpleFS |
| [docs/api/display.md](api/display.md) | Text output and graphics |
| [docs/api/memory.md](api/memory.md) | Heap, PMM, paging |
| [docs/api/tasks.md](api/tasks.md) | Scheduler and syscalls |
| [docs/api/exec.md](api/exec.md) | ELF loading and user programs |
| [docs/api/drivers.md](api/drivers.md) | Low-level hardware drivers |
