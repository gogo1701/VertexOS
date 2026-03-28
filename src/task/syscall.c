#include "scheduler.h"
#include "syscall.h"
#include "pit.h"

u32 syscall_handler(syscall_regs* regs) {
    if (!regs) {
        return 0xFFFFFFFFu;
    }

    switch (regs->eax) {
        case SYS_YIELD:
            scheduler_yield();
            return 0;
        case SYS_GET_TICKS:
            return pit_get_ticks();
        case SYS_GET_TID:
            return scheduler_current_tid();
        default:
            return 0xFFFFFFFFu;
    }
}
