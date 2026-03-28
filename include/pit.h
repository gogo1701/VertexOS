#ifndef PIT_H
#define PIT_H

#include "types.h"

/* Configure PIT channel 0 to the requested frequency in Hz. */
void pit_init(u32 frequency_hz);

/* Called by IRQ0 handler to advance tick counters. */
void pit_irq_handler(void);

/* Read-only timer state helpers. */
u32 pit_get_ticks(void);
u32 pit_get_frequency(void);

#endif /* PIT_H */
