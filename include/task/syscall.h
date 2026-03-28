#ifndef SYSCALL_H
#define SYSCALL_H

#include "types.h"

typedef struct {
    u32 edi;
    u32 esi;
    u32 ebp;
    u32 esp;
    u32 ebx;
    u32 edx;
    u32 ecx;
    u32 eax;
} syscall_regs;

enum {
    SYS_YIELD = 0,
    SYS_GET_TICKS = 1,
    SYS_GET_TID = 2
};

u32 syscall_handler(syscall_regs* regs);

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
