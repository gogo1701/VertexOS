; VertexOS Kernel Entry Point (32-bit Protected Mode)
;
; This assembly module is the first code executed after the bootloader.
; It sets up the stack and jumps to the C kernel main function.
; It also includes a Multiboot-compatible header so GRUB can boot VertexOS
; directly from UEFI in protected mode.

BITS 32                 ; 32-bit assembly code
ALIGN 4
multiboot_header:
    dd 0x1BADB002
    dd 0x00010001
    dd -(0x1BADB002 + 0x00010001)

GLOBAL _start           ; Export this symbol for the bootloader
EXTERN kmain            ; C function defined in kernel.c
EXTERN bootinfo_parse_multiboot
EXTERN bootinfo_multiboot_count

%define MULTIBOOT_MAGIC 0x2BADB002

_start:
    cmp eax, MULTIBOOT_MAGIC
    je .multiboot_entry

    ; BIOS boot path: existing bootloader registers.
    mov esp, 0x9FC00
    push ecx
    push ebx
    push edx
    call kmain
    jmp .hang

.multiboot_entry:
    mov esp, 0x9FC00
    push ebx
    call bootinfo_parse_multiboot
    mov ebx, eax
    mov ecx, [bootinfo_multiboot_count]
    push ecx
    push ebx
    push 0
    call kmain
    jmp .hang

; Infinite loop if main function returns (shouldn't happen normally)
.hang:
    cli                 ; Disable interrupts
    hlt                 ; Halt CPU execution
    jmp .hang            ; Loop forever (safety measure)
