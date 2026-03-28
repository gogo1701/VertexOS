#include "rtc.h"

#include "io.h"

static u8 cmos_read(u8 reg) {
    io_outb(0x70, reg);
    return io_inb(0x71);
}

static u8 rtc_updating(void) {
    return (cmos_read(0x0A) & 0x80) != 0;
}

static u8 bcd_to_bin(u8 v) {
    return (u8)((v & 0x0F) + ((v / 16) * 10));
}

u8 rtc_read_datetime(rtc_datetime* out) {
    rtc_datetime t;
    u8 reg_b;

    if (!out) {
        return 0;
    }

    while (rtc_updating()) {
        (void)0;
    }

    t.second = cmos_read(0x00);
    t.minute = cmos_read(0x02);
    t.hour = cmos_read(0x04);
    t.day = cmos_read(0x07);
    t.month = cmos_read(0x08);
    t.year = cmos_read(0x09);
    reg_b = cmos_read(0x0B);

    if ((reg_b & 0x04) == 0) {
        t.second = bcd_to_bin(t.second);
        t.minute = bcd_to_bin(t.minute);
        t.hour = bcd_to_bin((u8)(t.hour & 0x7F));
        t.day = bcd_to_bin(t.day);
        t.month = bcd_to_bin(t.month);
        t.year = bcd_to_bin(t.year);
    } else {
        t.hour &= 0x7F;
    }

    if ((reg_b & 0x02) == 0) {
        u8 is_pm = (u8)(cmos_read(0x04) & 0x80);
        if (is_pm && t.hour < 12) {
            t.hour = (u8)(t.hour + 12);
        } else if (!is_pm && t.hour == 12) {
            t.hour = 0;
        }
    }

    *out = t;
    return 1;
}
