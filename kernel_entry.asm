BITS 32
GLOBAL _start
EXTERN kmain

_start:
    mov esp, 0x9FC00
    call kmain

.hang:
    cli
    hlt
    jmp .hang
