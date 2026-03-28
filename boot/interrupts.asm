BITS 32

GLOBAL idt_load
GLOBAL irq0_stub
GLOBAL irq1_stub

EXTERN irq_timer_handler
EXTERN irq_keyboard_handler

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
