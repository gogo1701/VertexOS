/*
 * PIT — Programmable Interval Timer (Intel 8253/8254)
 *
 * Configures channel 0 of the PIT to generate periodic IRQ0 pulses.
 * The tick counter incremented by each IRQ0 is the kernel's primary
 * time source and drives scheduler pre-emption.
 */

#ifndef PIT_H
#define PIT_H

#include "types.h"

/*
 * pit_init - Program the PIT to fire IRQ0 at the given frequency.
 *
 * @frequency_hz: Desired timer rate in Hz (e.g. 100 for 10 ms ticks,
 *                1000 for 1 ms ticks).  Clamped to a minimum of 18 Hz
 *                (the lowest achievable with a 16-bit divisor).
 *
 * Must be called after pic_remap() and before interrupts_enable().
 */
void pit_init(u32 frequency_hz);

/*
 * pit_irq_handler - Advance the internal tick counter.
 *
 * Called automatically by the IRQ0 dispatcher.  Do not call directly.
 * Also notifies the scheduler that a tick has occurred.
 */
void pit_irq_handler(void);

/*
 * pit_get_ticks - Return the total number of timer ticks since boot.
 *
 * Useful for measuring elapsed time:
 *   u32 elapsed = pit_get_ticks() - start_ticks;
 *   u32 ms = (elapsed * 1000) / pit_get_frequency();
 */
u32 pit_get_ticks(void);

/*
 * pit_get_frequency - Return the configured timer frequency in Hz.
 */
u32 pit_get_frequency(void);

#endif /* PIT_H */
