# ELF Execution API

> Headers: `include/exec/exec.h`, `include/exec/userland.h`  
> Sources: `src/exec/exec.c`, `src/exec/userland.c`  
> Linker script: `user/user.ld`  
> Example program: `user/hello.c`

---

## Running programs

```c
#include "exec.h"

if (!exec_run_elf("/bin/hello.elf")) {
    display_print("exec: failed\n");
}
```

`exec_run_elf` opens the file from the VFS, validates the ELF header,
loads every `PT_LOAD` segment into kernel heap memory, and jumps to the
entry point.  It returns when (and if) the entry function returns.

### Supported ELF format

- Architecture: EM_386 (32-bit x86)
- Type: ET_EXEC (static executable)
- Linking: static only (no dynamic linker)

---

## Writing a user program

### 1. Create your source file in `user/`

```c
/* user/myapp.c */
#include "syscall.h"   /* for syscall_invoke */

void _start(void) {
    /* Your code here */
    syscall_invoke(SYS_YIELD, 0, 0, 0);
}
```

The entry point must be named `_start` to match the linker script.

### 2. Link with `user/user.ld`

The `user/user.ld` linker script places the program at virtual address
`0x00400000`.  Use the same Makefile pattern as `hello`:

```makefile
$(BUILD)/user/myapp.o: user/myapp.c user/user.ld
	$(CC) $(CFLAGS_USER) -c user/myapp.c -o $(BUILD)/user/myapp.o

$(BUILD)/user/myapp.elf: $(BUILD)/user/myapp.o user/user.ld
	$(LD) $(LDFLAGS_USER) -o $(BUILD)/user/myapp.elf $(BUILD)/user/myapp.o

$(BUILD)/user_myapp_elf.o: $(BUILD)/user/myapp.elf
	$(OBJCOPY) -I binary -O elf32-i386 -B i386 \
	    $(BUILD)/user/myapp.elf $(BUILD)/user_myapp_elf.o
```

Add `$(BUILD)/user_myapp_elf.o` to the linker command in `kernel.elf`.

### 3. Seed it on the VFS

In `src/exec/userland.c` add a seeding call (following the existing
`hello.elf` pattern):

```c
extern u8 _binary_build_user_myapp_elf_start[];
extern u8 _binary_build_user_myapp_elf_end[];

seed_program(
    "/bin/myapp.elf",
    _binary_build_user_myapp_elf_start,
    (u32)(_binary_build_user_myapp_elf_end - _binary_build_user_myapp_elf_start)
);
```

### 4. Run from the shell

```
/> exec /bin/myapp.elf
```

---

## Available kernel APIs from user programs

User programs currently run in *Ring 0* (kernel mode), so they can call
any kernel function directly.  Use the syscall interface (`syscall_invoke`)
for the services it covers, and call kernel functions for everything else
until a full user-mode separation is implemented.

### Syscalls (preferred way)

```c
syscall_invoke(SYS_YIELD,      0, 0, 0);   /* yield CPU          */
syscall_invoke(SYS_GET_TICKS,  0, 0, 0);   /* returns tick count */
syscall_invoke(SYS_GET_TID,    0, 0, 0);   /* returns task ID    */
```

See [tasks.md](tasks.md) for the full syscall table and how to add new ones.

---

## `userland_seed_programs`

```c
void userland_seed_programs(void);
```

Called once in `kmain()` after `vfs_init()`.  Copies all built-in ELF
binaries to `/bin/` on the VFS.  Safe to call on subsequent boots; files
that already exist are skipped.
