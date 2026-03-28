# Tasks & Syscalls API

> Headers: `include/task/scheduler.h`, `include/task/syscall.h`  
> Sources: `src/task/scheduler.c`, `src/task/syscall.c`

---

## Scheduler

The scheduler implements round-robin multitasking.  All tasks are kernel
threads (Ring 0) unless created with `TASK_MODE_USER`.

### Limits

```c
#define MAX_TASKS       8       /* Maximum concurrent tasks  */
#define TASK_STACK_SIZE 4096u   /* Stack per task (4 KiB)    */
```

### Creating a task

```c
#include "scheduler.h"

static void my_task(void* arg) {
    (void)arg;
    for (;;) {
        /* do work */
        scheduler_yield();   /* give other tasks a turn */
    }
}

/* Register before calling scheduler_start() */
scheduler_create_task(my_task, NULL, "worker", TASK_MODE_KERNEL);
```

`scheduler_create_task` returns the task ID (> 0) or `0` if the table
is full.

### Task entry functions

- Must **not** return.  Either loop forever or call `scheduler_yield()`
  and let another task terminate the process.
- Must call `scheduler_yield()` or `scheduler_maybe_preempt()` regularly
  so other tasks get CPU time.

### `scheduler_yield`

```c
void scheduler_yield(void);
```

Immediately transfer control to the next runnable task.  Use this at
natural pause points in long-running tasks.

### `scheduler_maybe_preempt`

```c
void scheduler_maybe_preempt(void);
```

Yield only if the timer has fired a preemption tick since the last call.
Suitable for polling loops where you do not want to yield on every iteration:

```c
static void idle_task(void* arg) {
    (void)arg;
    for (;;) {
        scheduler_maybe_preempt();
        interrupts_halt();   /* sleep until next IRQ */
    }
}
```

### Inspection helpers

```c
u32         scheduler_current_tid(void);    /* ID of the running task  */
const task* scheduler_current_task(void);  /* full task control block  */
u32         scheduler_task_count(void);    /* number of live tasks      */
```

### `task` struct fields

| Field        | Type         | Description |
|--------------|--------------|-------------|
| `id`         | `u32`        | Unique task ID |
| `name`       | `const char*`| Debug label |
| `state`      | `task_state` | `TASK_READY`, `TASK_RUNNING`, etc. |
| `mode`       | `task_mode`  | `TASK_MODE_KERNEL` or `TASK_MODE_USER` |
| `stack_top`  | `u32*`       | Saved ESP (updated each context switch) |

---

## Syscall interface (int 0x80)

User programs (and kernel tasks that want to test the path) call kernel
services via software interrupt `0x80`.

### Calling convention

```
EAX = syscall number
EBX = argument 0
ECX = argument 1
EDX = argument 2
---
EAX = return value (on return)
```

### `syscall_invoke` (from C)

```c
#include "syscall.h"

u32 ret = syscall_invoke(SYS_GET_TID, 0, 0, 0);
```

Works from both kernel and user code.

### Available syscalls

| Number | Constant       | Action                          | Returns |
|--------|----------------|---------------------------------|---------|
| 0      | `SYS_YIELD`    | Yield to scheduler              | 0 |
| 1      | `SYS_GET_TICKS`| PIT ticks since boot            | tick count |
| 2      | `SYS_GET_TID`  | Current task ID                 | task ID |

### Adding a new syscall

1. Add a new constant to the `enum` in `include/task/syscall.h`:
   ```c
   SYS_MY_CALL = 3,
   ```
2. Add a `case` in `syscall_handler()` in `src/task/syscall.c`:
   ```c
   case SYS_MY_CALL:
       return my_kernel_function(regs->ebx, regs->ecx);
   ```
3. Call it from user code:
   ```c
   u32 result = syscall_invoke(SYS_MY_CALL, arg0, arg1, 0);
   ```

---

## User programs (ELF)

See [exec.md](exec.md) for how to build and run user-mode ELF executables.
