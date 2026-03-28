BITS 32

GLOBAL idt_load
GLOBAL irq0_stub
GLOBAL irq1_stub
GLOBAL isr128_stub
GLOBAL context_switch

EXTERN irq_timer_handler
EXTERN irq_keyboard_handler
EXTERN syscall_handler

; Load IDT from pointer passed as the first argument.
idt_load:
    mov eax, [esp + 4]
    lidt [eax]
    ret

; IRQ0: PIT timer
irq0_stub:
    pusha
    call irq_timer_handler
    popa
    iretd

; IRQ1: PS/2 keyboard
irq1_stub:
    pusha
    call irq_keyboard_handler
    popa
    iretd

; int 0x80 syscall gate
isr128_stub:
    pusha
    push esp
    call syscall_handler
    add esp, 4
    mov [esp + 28], eax
    popa
    iretd

; void context_switch(u32* old_sp, u32 new_sp)
context_switch:
    mov eax, [esp + 4]
    mov edx, [esp + 8]

    push ebp
    push ebx
    push esi
    push edi

    mov [eax], esp
    mov esp, edx

    pop edi
    pop esi
    pop ebx
    pop ebp
    ret
