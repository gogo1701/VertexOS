#include "pit.h"
#include "io.h"

#define PIT_CMD       0x43
#define PIT_CHANNEL_0 0x40
#define PIT_BASE_HZ   1193182u

static volatile u32 pit_ticks = 0;
static u32 pit_frequency = 100;

void pit_init(u32 frequency_hz) {
    u32 divisor;

    if (frequency_hz == 0) {
        frequency_hz = 100;
    }

    pit_frequency = frequency_hz;
    divisor = PIT_BASE_HZ / frequency_hz;

    if (divisor == 0) {
        divisor = 1;
    }

    io_outb(PIT_CMD, 0x36);
    io_outb(PIT_CHANNEL_0, (u8)(divisor & 0xFF));
    io_outb(PIT_CHANNEL_0, (u8)((divisor >> 8) & 0xFF));
}

void pit_irq_handler(void) {
    pit_ticks++;
}

u32 pit_get_ticks(void) {
    return pit_ticks;
}

u32 pit_get_frequency(void) {
    return pit_frequency;
}
