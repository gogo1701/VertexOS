#include "serial.h"

#include "io.h"

#define COM1_PORT 0x3F8

#define REG_DATA 0
#define REG_INT_EN 1
#define REG_FIFO_CTRL 2
#define REG_LINE_CTRL 3
#define REG_MODEM_CTRL 4
#define REG_LINE_STATUS 5

#define LSR_DATA_READY 0x01
#define LSR_TX_EMPTY 0x20

static u8 g_serial_ready = 0;

static void serial_wait_tx(void) {
    while ((io_inb(COM1_PORT + REG_LINE_STATUS) & LSR_TX_EMPTY) == 0) {
        (void)0;
    }
}

void serial_init(void) {
    io_outb(COM1_PORT + REG_INT_EN, 0x00);
    io_outb(COM1_PORT + REG_LINE_CTRL, 0x80);
    io_outb(COM1_PORT + REG_DATA, 0x03);
    io_outb(COM1_PORT + REG_INT_EN, 0x00);
    io_outb(COM1_PORT + REG_LINE_CTRL, 0x03);
    io_outb(COM1_PORT + REG_FIFO_CTRL, 0xC7);
    io_outb(COM1_PORT + REG_MODEM_CTRL, 0x0B);

    g_serial_ready = 1;
}

u8 serial_is_ready(void) {
    return g_serial_ready;
}

void serial_write_char(char c) {
    if (!g_serial_ready) {
        return;
    }

    if (c == '\n') {
        serial_wait_tx();
        io_outb(COM1_PORT + REG_DATA, '\r');
    }

    serial_wait_tx();
    io_outb(COM1_PORT + REG_DATA, (u8)c);
}

void serial_write(const char* s) {
    while (s && *s) {
        serial_write_char(*s);
        s++;
    }
}

void serial_write_hex32(u32 val) {
    static const char hex[] = "0123456789ABCDEF";
    char buf[11];
    int i;
    buf[0] = '0'; buf[1] = 'x';
    for (i = 0; i < 8; i++) {
        buf[2 + i] = hex[(val >> (28 - i * 4)) & 0xFu];
    }
    buf[10] = '\0';
    serial_write(buf);
}

void serial_write_dec(u32 val) {
    char buf[12];
    char* p = buf + 11;
    *p = '\0';
    if (val == 0) {
        serial_write_char('0');
        return;
    }
    while (val > 0) {
        p--;
        *p = (char)('0' + (val % 10u));
        val /= 10u;
    }
    serial_write(p);
}
