#include "power.h"

#include "interrupts.h"
#include "io.h"

static void power_halt_forever(void) {
    for (;;) {
        __asm__ __volatile__("hlt");
    }
}

void power_restart(void) {
    u32 wait;

    interrupts_disable();

    for (wait = 0; wait < 0x10000u; wait++) {
        if ((io_inb(0x64) & 0x02u) == 0) {
            io_outb(0x64, 0xFE);
        }
    }

    power_halt_forever();
}

void power_shutdown(void) {
    interrupts_disable();

    /* Common VM shutdown ports used by QEMU, Bochs, and VirtualBox. */
    io_outw(0x604, 0x2000);
    io_outw(0xB004, 0x2000);
    io_outw(0x4004, 0x3400);

    power_halt_forever();
}
