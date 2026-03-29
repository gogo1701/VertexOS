# Command System API

> Header: `include/core/commands.h`  
> Source: `src/core/commands.c`

The command system is the easiest place to extend the shell.  Everything
needed to add a new command is in this one header.

---

## Quick start — adding a new command

```c
#include "commands.h"
#include "display.h"

static void cmd_hello(const char* args) {
    (void)args;
    display_print("Hello, VertexOS!\n");
}

/* Call this from commands_init() in src/core/commands.c */
command_register_full("hello", "hello", "Print a greeting", cmd_hello);
```

1. Write a `static void my_cmd(const char* args)` function.
2. Call `command_register_full()` with name, usage, summary, and the function pointer.
3. That is it — `help` will list it automatically.

---

## Types

### `command_func`

```c
typedef void (*command_func)(const char* args);
```

Signature every command handler must match.  
`args` is the raw argument string that follows the command name (trimmed of
leading spaces).  It is never NULL; it may be an empty string `""`.

---

## Functions

### `command_register`

```c
u8 command_register(const char* name, command_func func);
```

Register a command with no usage string or summary text.  Suitable for
quick one-off commands during development; prefer `command_register_full`
for anything permanent.

| Parameter | Description |
|-----------|-------------|
| `name`    | Command name typed at the shell (e.g. `"hello"`). |
| `func`    | Handler function. |

**Returns** `1` on success, `0` if the name already exists or the table
(`MAX_COMMANDS = 32`) is full.

---

### `command_register_full`

```c
/* defined as static in commands.c; call from commands_init() */
command_register_full(name, usage, summary, func);
```

Preferred registration path.  Provides help text visible via `help <name>`.

| Parameter | Description |
|-----------|-------------|
| `name`    | Shell name (e.g. `"write"`). |
| `usage`   | One-line usage string shown by `help` (e.g. `"write <path> <text>"`). |
| `summary` | Short sentence describing the command. |
| `func`    | Handler function pointer. |

---

### `command_execute`

```c
u8 command_execute(const char* input);
```

Parse `input` as `<name> [args]` and dispatch to the registered handler.
Used internally by the CLI; you will not normally call this directly.

**Returns** `1` if a matching command was found and executed, `0` if not found.

---

### `command_count` / `command_name_at`

```c
u32        command_count(void);
const char* command_name_at(u32 index);
```

Used by the CLI for tab-completion.  `command_name_at(i)` returns `NULL`
when `i >= command_count()`.

---

## Argument parsing tips

The raw `args` string passed to your handler:

```
input line: "write /docs/notes.txt hello world"
name:        write
args:        /docs/notes.txt hello world
```

The command system ships a private `read_arg()` helper (not public API)
that handles quoted strings.  If you need to parse arguments inside a
command body, copy the pattern from existing commands in
`src/core/commands.c`.

```c
static void cmd_myfile(const char* args) {
    char path[64];
    /* read_arg advances the pointer and fills path */
    if (!read_arg(&args, path, sizeof(path)) || !path[0]) {
        display_print("Usage: myfile <path>\n");
        return;
    }
    /* path now holds the first token; args points past it */
}
```

---

## Built-in commands reference

| Command   | Usage                                | Summary |
|-----------|--------------------------------------|---------|
| `help`    | `help [command]`                     | List commands or show per-command help |
| `clear`   | `clear`                              | Clear the screen |
| `echo`    | `echo [text]`                        | Print text (quotes supported) |
| `uptime`  | `uptime`                             | Show seconds since boot |
| `time`    | `time`                               | Show RTC date/time |
| `panic`   | `panic [message]`                    | Trigger kernel panic |
| `restart` | `restart`                            | Reset the machine |
| `shutdown`| `shutdown`                           | Power off the machine |
| `meminfo` | `meminfo`                            | PMM and heap usage |
| `alloc`   | `alloc`                              | Heap self-test |
| `tasks`   | `tasks`                              | Scheduler task summary |
| `yield`   | `yield`                              | Yield CPU to next task |
| `syscall` | `syscall`                            | Demo int 0x80 syscalls |
| `ls`      | `ls [path]`                          | List directory |
| `cat`     | `cat <path>`                         | Print file contents |
| `write`   | `write <path> <text>`                | Write text to file |
| `touch`   | `touch <path>`                       | Create empty file |
| `mkdir`   | `mkdir <path>`                       | Create directory |
| `rm`      | `rm <path>`                          | Remove file or empty dir |
| `cp`      | `cp <src> <dst>`                     | Copy file |
| `mv`      | `mv <src> <dst>`                     | Move/rename file |
| `cd`      | `cd [path]`                          | Change directory |
| `pwd`     | `pwd`                                | Print working directory |
| `exec`    | `exec <elf-path>`                    | Load and run ELF |
| `video`   | `video [status\|text\|gfx\|test …]` | Video mode control |
| `ifconfig`| `ifconfig`                            | Show NIC/IP/DHCP configuration |
| `dhcp`    | `dhcp`                                | Request a DHCP lease |
| `ping`    | `ping <ipv4> [timeout_ms]`            | Send one ICMP echo request |
| `dns`     | `dns <hostname\|ipv4> [timeout_ms]`  | Resolve hostname to IPv4 (A record) |
