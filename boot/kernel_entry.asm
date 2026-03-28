; VertexOS Kernel Entry Point (32-bit Protected Mode)
;
; This assembly module is the first code executed after the bootloader.
; It sets up the stack and jumps to the C kernel main function.

BITS 32                 ; 32-bit assembly code
GLOBAL _start           ; Export this symbol for the bootloader
EXTERN kmain            ; C function defined in kernel.c

_start:
    ; Initialize the stack pointer
    ; 0x9FC00 is selected as a safe memory location below the 640KB limit
    mov esp, 0x9FC00
    
    ; Pass BIOS E820 map pointer/count from bootloader.
    ; cdecl: push args right-to-left -> (map_ptr, map_count)
    push ecx
    push ebx
    call kmain

; Infinite loop if main function returns (shouldn't happen normally)
.hang:
    cli                 ; Disable interrupts
    hlt                 ; Halt CPU execution
    jmp .hang            ; Loop forever (safety measure)
