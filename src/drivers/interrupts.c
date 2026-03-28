#include "interrupts.h"
#include "keyboard.h"
#include "panic.h"
#include "pic.h"
#include "pit.h"
#include "scheduler.h"
#include "syscall.h"
#include "types.h"

#define IDT_ENTRIES 256
#define KERNEL_CS   0x08
#define IDT_FLAG_INT_GATE 0x8E
#define IDT_FLAG_USER_INT_GATE 0xEE

typedef struct {
    u16 base_lo;
    u16 selector;
    u8 zero;
    u8 flags;
    u16 base_hi;
} __attribute__((packed)) idt_entry;

typedef struct {
    u16 limit;
    u32 base;
} __attribute__((packed)) idt_ptr;

static idt_entry idt[IDT_ENTRIES];
static idt_ptr idtp;

extern void idt_load(u32 idt_ptr_addr);
extern void irq0_stub(void);
extern void irq1_stub(void);
extern void isr128_stub(void);

static void idt_set_gate(u8 index, u32 handler, u16 selector, u8 flags) {
    idt[index].base_lo = (u16)(handler & 0xFFFF);
    idt[index].selector = selector;
    idt[index].zero = 0;
    idt[index].flags = flags;
    idt[index].base_hi = (u16)((handler >> 16) & 0xFFFF);
}

void irq_timer_handler(void) {
    pit_irq_handler();
    scheduler_on_tick();
    pic_send_eoi(0);
}

void irq_keyboard_handler(void) {
    keyboard_irq_handler();
    pic_send_eoi(1);
}

void interrupts_enable(void) {
    __asm__ __volatile__("sti");
}

void interrupts_disable(void) {
    __asm__ __volatile__("cli");
}

void interrupts_halt(void) {
    __asm__ __volatile__("hlt");
}

void interrupts_init(void) {
    u32 i;

    for (i = 0; i < IDT_ENTRIES; i++) {
        idt_set_gate((u8)i, 0, 0, 0);
    }

    idt_set_gate(32, (u32)irq0_stub, KERNEL_CS, IDT_FLAG_INT_GATE);
    idt_set_gate(33, (u32)irq1_stub, KERNEL_CS, IDT_FLAG_INT_GATE);
    idt_set_gate(0x80, (u32)isr128_stub, KERNEL_CS, IDT_FLAG_USER_INT_GATE);

    idtp.limit = (u16)(sizeof(idt_entry) * IDT_ENTRIES - 1);
    idtp.base = (u32)&idt;
    idt_load((u32)&idtp);

    pic_remap(0x20, 0x28);

    /* Mask all IRQ lines except timer (IRQ0) and keyboard (IRQ1). */
    for (i = 0; i < 16; i++) {
        pic_set_mask((u8)i);
    }
    pic_clear_mask(0);
    pic_clear_mask(1);

    pit_init(100);

    interrupts_enable();

    KASSERT(pit_get_frequency() != 0);
}
