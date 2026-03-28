/*
 * System Call Interface (int 0x80)
 *
 * Provides the boundary between user-mode programs and the kernel.
 * User programs invoke syscalls with a software interrupt:
 *
 *   int $0x80
 *
 * Calling convention (mirrors Linux i386 for familiarity):
 *   EAX = syscall number
 *   EBX = argument 0
 *   ECX = argument 1
 *   EDX = argument 2
 *   EAX = return value (after call returns)
 *
 * From C code, use the syscall_invoke() inline helper.
 *
 * Defined syscall numbers:
 *   SYS_YIELD      0  - Voluntarily yield the CPU to the scheduler
 *   SYS_GET_TICKS  1  - Return total PIT ticks since boot
 *   SYS_GET_TID    2  - Return current task ID
 */

#ifndef SYSCALL_H
#define SYSCALL_H

#include "types.h"

/*
 * syscall_regs - Register snapshot pushed by the int 0x80 stub in
 * boot/interrupts.asm.  Passed to syscall_handler by the stub.
 */
typedef struct {
    u32 edi;
    u32 esi;
    u32 ebp;
    u32 esp;
    u32 ebx;
    u32 edx;
    u32 ecx;
    u32 eax;  /* syscall number on entry; return value on exit */
} syscall_regs;

/* Syscall number constants. */
enum {
    SYS_YIELD     = 0,  /* scheduler_yield()      */
    SYS_GET_TICKS = 1,  /* pit_get_ticks()         */
    SYS_GET_TID   = 2   /* scheduler_current_tid() */
};

/*
 * syscall_handler - Kernel-side syscall dispatcher.
 *
 * Called from the int 0x80 assembly stub.  Reads regs->eax to determine
 * the requested syscall, dispatches to the appropriate kernel function,
 * and returns the result (which the stub writes back to EAX).
 *
 * Do not call directly from C kernel code; use the relevant API function
 * instead.  Use syscall_invoke() from user-mode programs.
 */
u32 syscall_handler(syscall_regs* regs);

/*
 * syscall_invoke - Issue a system call from user or kernel mode.
 *
 * Executes "int $0x80" with the given arguments.  This is the only
 * supported way for user programs to request kernel services.
 *
 * @num:  Syscall number (SYS_* constant).
 * @arg0: First argument (goes into EBX).
 * @arg1: Second argument (goes into ECX).
 * @arg2: Third argument (goes into EDX).
 *
 * @return: Value returned by the kernel handler (from EAX).
 *
 * Example (from a user program):
 *   u32 my_tid = syscall_invoke(SYS_GET_TID, 0, 0, 0);
 */
static inline u32 syscall_invoke(u32 num, u32 arg0, u32 arg1, u32 arg2) {
    u32 ret;
    __asm__ __volatile__(
        "int $0x80"
        : "=a"(ret)
        : "a"(num), "b"(arg0), "c"(arg1), "d"(arg2)
        : "memory"
    );
    return ret;
}

#endif /* SYSCALL_H */
