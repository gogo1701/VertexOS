; Stage-1 x86 bootloader (BIOS, 16-bit real mode)
; Loads a tiny 32-bit C kernel from disk and jumps to it.

BITS 16
ORG 0x7C00

KERNEL_SEGMENT   equ 0x1000
KERNEL_OFFSET    equ 0x0000
KERNEL_SECTORS   equ 192
KERNEL_FIRST_READ_SECTORS equ 127
KERNEL_SECOND_READ_SECTORS equ (KERNEL_SECTORS - KERNEL_FIRST_READ_SECTORS)

E820_BUFFER      equ 0x5000
E820_COUNT_ADDR  equ 0x4FF0
E820_MAX_ENTRIES equ 32

CODE_SEG         equ 0x08
DATA_SEG         equ 0x10

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    sti

    mov [boot_drive], dl

    ; Set video mode based on saved preference (stored in boot sector).
    ; 0 = text mode (0x03), 1 = graphics mode (0x13)
    movzx ax, byte [video_mode_pref]
    cmp al, 1
    je .set_graphics
    
    ; Default to text mode
    mov ax, 0x0003
    int 0x10
    jmp .video_ready
    
.set_graphics:
    ; Try to set graphics mode (VGA mode 0x13: 320x200, 256 colors)
    mov ax, 0x0013
    int 0x10
    
.video_ready:
    call detect_memory_map

    mov si, msg_loading
    call print_string

    mov al, [boot_drive]
    cmp al, 0x80
    jb .read_chs

    call read_kernel_lba
    jmp .read_done

.read_chs:
    call read_kernel_chs

.read_done:
    jc disk_error

    cli
    lgdt [gdt_descriptor]

    mov eax, cr0
    or eax, 1
    mov cr0, eax
    jmp CODE_SEG:protected_mode

disk_error:
    mov si, msg_disk_error
    call print_string

hang:
    cli
    hlt
    jmp hang

print_string:
    lodsb
    test al, al
    jz .done
    mov ah, 0x0E
    mov bh, 0x00
    mov bl, 0x07
    int 0x10
    jmp print_string
.done:
    ret

read_kernel_chs:
    mov ax, KERNEL_SEGMENT
    mov es, ax
    xor ch, ch
    xor dh, dh
    mov cl, 2
    mov si, KERNEL_SECTORS

.sector_loop:
    cmp si, 0
    je .ok

    mov bx, KERNEL_OFFSET

    mov ah, 0x02
    mov al, 0x01
    mov dl, [boot_drive]
    int 0x13
    jc .fail

    ; Advance destination by one sector (512 bytes -> 0x20 paragraphs)
    mov ax, es
    add ax, 0x20
    mov es, ax
    dec si

    ; Advance CHS for 1.44MB floppy geometry (18 sectors/track, 2 heads)
    inc cl
    cmp cl, 19
    jb .sector_loop

    mov cl, 1
    inc dh
    cmp dh, 2
    jb .sector_loop

    xor dh, dh
    inc ch
    jmp .sector_loop

.ok:
    clc
    ret

.fail:
    stc
    ret

read_kernel_lba:
    ; First chunk: read 127 sectors from LBA 1
    mov word [dap_count], KERNEL_FIRST_READ_SECTORS
    mov word [dap_offset], KERNEL_OFFSET
    mov word [dap_segment], KERNEL_SEGMENT
    mov dword [dap_lba_lo], 1
    mov dword [dap_lba_hi], 0

    mov si, dap
    mov ah, 0x42
    mov dl, [boot_drive]
    int 0x13
    jc .fail

    ; Second chunk: read the remaining sectors from LBA 128
    mov word [dap_count], KERNEL_SECOND_READ_SECTORS
    mov word [dap_offset], KERNEL_OFFSET
    mov word [dap_segment], KERNEL_SEGMENT + (KERNEL_FIRST_READ_SECTORS * 32)
    mov dword [dap_lba_lo], 1 + KERNEL_FIRST_READ_SECTORS
    mov dword [dap_lba_hi], 0

    mov si, dap
    mov ah, 0x42
    mov dl, [boot_drive]
    int 0x13
    jc .fail

    clc
    ret

.fail:
    stc
    ret

boot_drive db 0
msg_loading db "Loading C kernel...", 0x0D, 0x0A, 0
msg_disk_error db "Disk read error", 0x0D, 0x0A, 0

dap:
    db 16
    db 0
dap_count:
    dw 0
dap_offset:
    dw 0
dap_segment:
    dw 0
dap_lba_lo:
    dd 0
dap_lba_hi:
    dd 0

gdt_start:
gdt_null:
    dq 0x0000000000000000
gdt_code:
    dq 0x00CF9A000000FFFF
gdt_data:
    dq 0x00CF92000000FFFF
gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

BITS 32
protected_mode:
    mov ax, DATA_SEG
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x90000

    ; Pass boot memory info to kernel entry in registers.
    mov ebx, E820_BUFFER
    movzx ecx, word [E820_COUNT_ADDR]
    movzx edx, byte [video_mode_pref]

    mov eax, KERNEL_SEGMENT * 16
    jmp eax

BITS 16
detect_memory_map:
    pushad

    xor ebx, ebx
    xor bp, bp
    mov di, E820_BUFFER

.next_entry:
    mov eax, 0xE820
    mov edx, 0x534D4150
    mov ecx, 20
    int 0x15
    jc .done

    cmp eax, 0x534D4150
    jne .done

    inc bp
    add di, 20

    cmp bp, E820_MAX_ENTRIES
    jae .done

    cmp ebx, 0
    jne .next_entry

.done:
    mov [E820_COUNT_ADDR], bp
    popad
    ret

times 508 - ($ - $$) db 0
video_mode_pref db 0
boot_flags db 0
dw 0xAA55
