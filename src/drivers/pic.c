#include "pic.h"
#include "io.h"

#define PIC1_CMD  0x20
#define PIC1_DATA 0x21
#define PIC2_CMD  0xA0
#define PIC2_DATA 0xA1

#define PIC_EOI 0x20

#define ICW1_ICW4 0x01
#define ICW1_INIT 0x10
#define ICW4_8086 0x01

static void io_wait(void) {
    io_outb(0x80, 0);
}

void pic_remap(u8 master_offset, u8 slave_offset) {
    u8 master_mask = io_inb(PIC1_DATA);
    u8 slave_mask = io_inb(PIC2_DATA);

    io_outb(PIC1_CMD, ICW1_INIT | ICW1_ICW4);
    io_wait();
    io_outb(PIC2_CMD, ICW1_INIT | ICW1_ICW4);
    io_wait();

    io_outb(PIC1_DATA, master_offset);
    io_wait();
    io_outb(PIC2_DATA, slave_offset);
    io_wait();

    io_outb(PIC1_DATA, 4);
    io_wait();
    io_outb(PIC2_DATA, 2);
    io_wait();

    io_outb(PIC1_DATA, ICW4_8086);
    io_wait();
    io_outb(PIC2_DATA, ICW4_8086);
    io_wait();

    io_outb(PIC1_DATA, master_mask);
    io_outb(PIC2_DATA, slave_mask);
}

void pic_set_mask(u8 irq_line) {
    u16 port;
    u8 value;

    if (irq_line < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq_line -= 8;
    }

    value = io_inb(port) | (1u << irq_line);
    io_outb(port, value);
}

void pic_clear_mask(u8 irq_line) {
    u16 port;
    u8 value;

    if (irq_line < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq_line -= 8;
    }

    value = io_inb(port) & (u8)~(1u << irq_line);
    io_outb(port, value);
}

void pic_send_eoi(u8 irq_line) {
    if (irq_line >= 8) {
        io_outb(PIC2_CMD, PIC_EOI);
    }
    io_outb(PIC1_CMD, PIC_EOI);
}
