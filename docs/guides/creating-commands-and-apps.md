# Creating Commands and Custom Apps

Practical guide for extending VertexOS with your own shell commands and ELF apps.

---

## What you will build

- A custom shell command (example: `greet`)
- A custom ELF app in `user/` (example: `/bin/mytool.elf`)
- Optional command alias to run your app quickly

---

## Part 1: Create a new shell command

### 1. Add a forward declaration

In `src/core/commands.c`, add:

```c
static void cmd_greet(const char* args);
```

### 2. Implement the handler

Add a handler near the other command functions:

```c
static void cmd_greet(const char* args) {
    char name[32];

    if (!read_arg(&args, name, sizeof(name)) || !name[0]) {
        display_print("Usage: greet <name>\n");
        return;
    }

    display_print("Hello, ");
    display_print(name);
    display_print("\n");
}
```

### 3. Register the command

Inside `commands_init()` in `src/core/commands.c`:

```c
command_register_full("greet", "greet <name>", "Print greeting", cmd_greet);
```

### 4. Build and test

```sh
make run
```

In shell:

```text
help greet
greet VertexOS
```

---

## Part 2: Create a custom ELF app

### 1. Add source file

Create `user/mytool.c`:

```c
#include "display.h"
#include "syscall.h"

void _start(void) {
    display_print("mytool: hello from custom app\n");
    syscall_invoke(SYS_YIELD, 0, 0, 0);
}
```

Use `_start` as entry point.

### 2. Extend Makefile build pipeline

Follow the existing `hello` pattern in `Makefile`:

```makefile
$(BUILD)/user/mytool.o: $(USER_DIR)/mytool.c $(USER_DIR)/user.ld | $(BUILD)
	$(CC) $(CFLAGS_USER) -c $(USER_DIR)/mytool.c -o $(BUILD)/user/mytool.o

$(BUILD)/user/mytool.elf: $(BUILD)/user/mytool.o $(USER_DIR)/user.ld | $(BUILD)
	$(LD) $(LDFLAGS_USER) -o $(BUILD)/user/mytool.elf $(BUILD)/user/mytool.o

$(BUILD)/user_mytool_elf.o: $(BUILD)/user/mytool.elf | $(BUILD)
	$(OBJCOPY) -I binary -O elf32-i386 -B i386 $(BUILD)/user/mytool.elf $(BUILD)/user_mytool_elf.o
```

Then add `$(BUILD)/user_mytool_elf.o` to:

- The dependency list of the `$(BUILD)/kernel.elf` rule
- The linker command line of the same rule

### 3. Seed the program in `/bin`

Edit `src/exec/userland.c` and follow the existing `hello.elf` pattern:

```c
extern const u8 _binary_build_user_mytool_elf_start[];
extern const u8 _binary_build_user_mytool_elf_end[];

/* inside userland_seed_programs() */
if (!vfs_stat_path("/bin/mytool.elf", &st)) {
    fd = vfs_open("/bin/mytool.elf", VFS_O_WRITE | VFS_O_CREATE | VFS_O_TRUNC);
    if (fd >= 0) {
        const u8* data = _binary_build_user_mytool_elf_start;
        u32 size = (u32)(_binary_build_user_mytool_elf_end - _binary_build_user_mytool_elf_start);
        u32 written = vfs_write(fd, data, size);
        vfs_close(fd);
        if (written != size) {
            display_print("userland: short write seeding mytool.elf\n");
        }
    }
}
```

### 4. Run the app

```text
exec /bin/mytool.elf
```

---

## Part 3: Optional app alias command

To run your app with a short name, add this command in `src/core/commands.c`:

```c
static void cmd_mytool(const char* args) {
    (void)args;
    (void)exec_run_elf("/bin/mytool.elf");
}

/* in commands_init() */
command_register_full("mytool", "mytool", "Run /bin/mytool.elf", cmd_mytool);
```

Now run:

```text
mytool
```

---

## Part 4: Adding a network command (example pattern)

If your command needs networking:

1. Validate network readiness with `net_is_ready()`.
2. Parse input with `read_arg()` and validate values.
3. Use high-level API:
   - `net_dhcp_request()`
   - `net_ping()`
   - `net_resolve_ipv4()`

Minimal skeleton:

```c
static void cmd_netcheck(const char* args) {
    (void)args;

    if (!net_is_ready()) {
        display_print("netcheck: link down\n");
        return;
    }

    display_print("netcheck: link up\n");
}
```

Keep default debug noise low in normal builds:

- `NET_DEBUG` in `src/drivers/net.c` should stay `0`
- `RTL8139_DEBUG` in `src/drivers/rtl8139.c` should stay `0`

---

## Troubleshooting checklist

- Command not found:
  - Verify `command_register_full(...)` exists in `commands_init()`.
- App not found at runtime:
  - Verify seeding path is exactly `/bin/<name>.elf`.
  - Verify symbol names match generated object name (`_binary_build_user_<name>_elf_start/end`).
- Linker error for app object:
  - Ensure `$(BUILD)/user_<name>_elf.o` is in kernel link dependencies and link command.
- Excessive network logs:
  - Ensure `NET_DEBUG` and `RTL8139_DEBUG` are `0`.

---

## Related docs

- [../DEVELOPING.md](../DEVELOPING.md)
- [../api/commands.md](../api/commands.md)
- [../api/exec.md](../api/exec.md)
- [../api/drivers.md](../api/drivers.md)
- [../api/tasks.md](../api/tasks.md)
