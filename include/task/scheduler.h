/*
 * Kernel Task Scheduler
 *
 * Implements cooperative/pre-emptive round-robin multitasking for kernel
 * tasks.  Up to MAX_TASKS tasks may be alive simultaneously.
 *
 * Task lifecycle:
 *   1. scheduler_init()       - called once near the end of kmain()
 *   2. scheduler_create_task() - register one or more tasks
 *   3. scheduler_start()      - enter the first task; never returns
 *
 * Pre-emption is driven by the PIT (IRQ0).  Each tick sets a pending-
 * preempt flag; tasks that call scheduler_maybe_preempt() (e.g. the idle
 * task) will yield immediately when the flag is set.
 *
 * Voluntary yielding is also available via scheduler_yield().
 */

#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "types.h"

#define MAX_TASKS       8       /* Maximum concurrently live tasks            */
#define TASK_STACK_SIZE 4096u   /* Stack size in bytes for each task          */

/*
 * task_state - Lifecycle state of a task.
 *
 * TASK_UNUSED    : Slot is free; no task occupies it.
 * TASK_READY     : Task is runnable and waiting for the CPU.
 * TASK_RUNNING   : Task currently holds the CPU.
 * TASK_BLOCKED   : Task is waiting on an event (not yet implemented).
 * TASK_TERMINATED: Task's entry function returned; slot will be reused.
 */
typedef enum {
    TASK_UNUSED = 0,
    TASK_READY,
    TASK_RUNNING,
    TASK_BLOCKED,
    TASK_TERMINATED
} task_state;

/*
 * task_mode - Privilege level a task runs at.
 *
 * TASK_MODE_KERNEL : Ring 0 (full hardware access, all instructions).
 * TASK_MODE_USER   : Ring 3 (restricted; requires syscall for kernel ops).
 */
typedef enum {
    TASK_MODE_KERNEL = 0,
    TASK_MODE_USER = 3
} task_mode;

/*
 * task - Internal task control block.
 *
 * @id:         Unique task ID (1-based, 0 = invalid).
 * @stack_top:  Saved stack pointer, updated on every context switch.
 * @stack_base: Base of the allocated stack buffer.
 * @state:      Current lifecycle state.
 * @mode:       Kernel or user privilege ring.
 * @entry:      Task entry point function.
 * @arg:        Argument passed to the entry function.
 * @name:       Human-readable name for debugging (e.g. "cli", "idle").
 */
typedef struct {
    u32 id;
    u32* stack_top;
    u32 stack_base;
    task_state state;
    task_mode mode;
    void (*entry)(void* arg);
    void* arg;
    const char* name;
} task;

/*
 * scheduler_init - Initialise the task table.
 *
 * Must be called after the heap is ready and before creating any tasks.
 */
void scheduler_init(void);

/*
 * scheduler_create_task - Register a new kernel or user task.
 *
 * Allocates a stack from the kernel heap and sets up the initial CPU
 * context so the task will begin execution at entry(arg) when scheduled.
 *
 * @entry: Function where the task starts.  Must not return.
 * @arg:   Arbitrary pointer passed to entry as its only argument.
 * @name:  Short human-readable label (used by the 'tasks' command).
 * @mode:  TASK_MODE_KERNEL or TASK_MODE_USER.
 *
 * @return: Assigned task ID (> 0) on success, 0 if the task table is full.
 *
 * Example:
 *   scheduler_create_task(my_task, NULL, "worker", TASK_MODE_KERNEL);
 */
u32 scheduler_create_task(void (*entry)(void*), void* arg, const char* name, task_mode mode);

/*
 * scheduler_start - Begin scheduling and enter the first ready task.
 *
 * Picks the first TASK_READY task and switches to it.  Never returns.
 * Call this at the very end of kmain() after all tasks are registered.
 */
void scheduler_start(void);

/*
 * scheduler_yield - Voluntarily give up the CPU.
 *
 * Switches to the next TASK_READY task immediately.
 * Safe to call from any kernel task at any time.
 */
void scheduler_yield(void);

/*
 * scheduler_on_tick - Advance the scheduler time base (called by IRQ0).
 *
 * Do not call directly; invoked automatically from the PIT interrupt handler.
 */
void scheduler_on_tick(void);

/*
 * scheduler_maybe_preempt - Yield if a preemption is pending.
 *
 * Should be called periodically by long-running or idle tasks.
 * Has no effect if no preemption tick has fired since the last call.
 */
void scheduler_maybe_preempt(void);

/*
 * scheduler_current_tid - Return the task ID of the currently running task.
 */
u32 scheduler_current_tid(void);

/*
 * scheduler_current_task - Return a pointer to the current task control block.
 *
 * @return: Pointer to the active task, or NULL before scheduler_start().
 */
const task* scheduler_current_task(void);

/*
 * scheduler_task_count - Return the number of non-UNUSED task slots.
 */
u32 scheduler_task_count(void);

/*
 * scheduler_get_task_info - Copy task info for a task table index.
 *
 * @index: Zero-based task table index in [0, scheduler_task_count()).
 * @out:   Output task snapshot.
 *
 * @return: 1 if index is valid, 0 otherwise.
 */
u8 scheduler_get_task_info(u32 index, task* out);

#endif /* SCHEDULER_H */
