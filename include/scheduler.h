#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "types.h"

#define MAX_TASKS 8
#define TASK_STACK_SIZE 4096u

typedef enum {
    TASK_UNUSED = 0,
    TASK_READY,
    TASK_RUNNING,
    TASK_BLOCKED,
    TASK_TERMINATED
} task_state;

typedef enum {
    TASK_MODE_KERNEL = 0,
    TASK_MODE_USER = 3
} task_mode;

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

void scheduler_init(void);
u32 scheduler_create_task(void (*entry)(void*), void* arg, const char* name, task_mode mode);
void scheduler_start(void);
void scheduler_yield(void);
void scheduler_on_tick(void);
void scheduler_maybe_preempt(void);

u32 scheduler_current_tid(void);
const task* scheduler_current_task(void);
u32 scheduler_task_count(void);

#endif /* SCHEDULER_H */
