#include "scheduler.h"
#include "display.h"
#include "interrupts.h"
#include "panic.h"

static task tasks[MAX_TASKS];
static u8 task_stacks[MAX_TASKS][TASK_STACK_SIZE];
static u32 task_count = 0;
static s32 current_index = -1;
static volatile u8 preempt_pending = 0;
static volatile u32 tick_quantum_counter = 0;

extern void context_switch(u32* old_sp, u32 new_sp);

static void scheduler_pick_next(void) {
    u32 i;

    if (task_count == 0) {
        return;
    }

    for (i = 0; i < task_count; i++) {
        u32 idx = (u32)((current_index + 1 + (s32)i) % (s32)task_count);
        if (tasks[idx].state == TASK_READY || tasks[idx].state == TASK_RUNNING) {
            if (current_index >= 0 && tasks[current_index].state == TASK_RUNNING) {
                tasks[current_index].state = TASK_READY;
            }
            current_index = (s32)idx;
            tasks[idx].state = TASK_RUNNING;
            return;
        }
    }
}

static void task_bootstrap(void) {
    const task* t = scheduler_current_task();
    if (!t) {
        panic("Scheduler bootstrap without current task");
    }

    t->entry(t->arg);

    /* A task returning is considered terminated. */
    tasks[current_index].state = TASK_TERMINATED;
    for (;;) {
        scheduler_yield();
    }
}

void scheduler_init(void) {
    u32 i;
    for (i = 0; i < MAX_TASKS; i++) {
        tasks[i].id = i;
        tasks[i].stack_top = 0;
        tasks[i].stack_base = (u32)&task_stacks[i][0];
        tasks[i].state = TASK_UNUSED;
        tasks[i].mode = TASK_MODE_KERNEL;
        tasks[i].entry = 0;
        tasks[i].arg = 0;
        tasks[i].name = "unused";
    }

    task_count = 0;
    current_index = -1;
    preempt_pending = 0;
    tick_quantum_counter = 0;
}

u32 scheduler_create_task(void (*entry)(void*), void* arg, const char* name, task_mode mode) {
    u32* sp;
    u32 idx;

    if (!entry || task_count >= MAX_TASKS) {
        return 0xFFFFFFFFu;
    }

    idx = task_count;
    tasks[idx].entry = entry;
    tasks[idx].arg = arg;
    tasks[idx].mode = mode;
    tasks[idx].state = TASK_READY;
    tasks[idx].name = name ? name : "task";

    /*
     * Initial stack for context_switch pop/ret sequence:
     * [edi][esi][ebx][ebp][ret_eip]
     */
    sp = (u32*)(tasks[idx].stack_base + TASK_STACK_SIZE);
    *--sp = (u32)task_bootstrap;
    *--sp = 0; /* ebp */
    *--sp = 0; /* ebx */
    *--sp = 0; /* esi */
    *--sp = 0; /* edi */

    tasks[idx].stack_top = sp;
    task_count++;
    return idx;
}

void scheduler_start(void) {
    u32 dummy_sp = 0;

    if (task_count == 0) {
        panic("Scheduler start with no tasks");
    }

    scheduler_pick_next();
    context_switch(&dummy_sp, (u32)tasks[current_index].stack_top);

    panic("Returned from scheduler_start unexpectedly");
}

void scheduler_yield(void) {
    s32 old_index;
    u32* old_sp_ptr;

    if (task_count < 2) {
        return;
    }

    old_index = current_index;
    scheduler_pick_next();

    if (current_index < 0 || current_index == old_index) {
        return;
    }

    old_sp_ptr = tasks[old_index].stack_top;
    context_switch((u32*)&tasks[old_index].stack_top, (u32)tasks[current_index].stack_top);
    (void)old_sp_ptr;
}

void scheduler_on_tick(void) {
    tick_quantum_counter++;
    if (tick_quantum_counter >= 4) {
        tick_quantum_counter = 0;
        preempt_pending = 1;
    }
}

void scheduler_maybe_preempt(void) {
    if (preempt_pending) {
        preempt_pending = 0;
        scheduler_yield();
    }
}

u32 scheduler_current_tid(void) {
    if (current_index < 0) {
        return 0xFFFFFFFFu;
    }
    return tasks[current_index].id;
}

const task* scheduler_current_task(void) {
    if (current_index < 0) {
        return 0;
    }
    return &tasks[current_index];
}

u32 scheduler_task_count(void) {
    return task_count;
}
