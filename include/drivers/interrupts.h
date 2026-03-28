/*
 * Interrupt Subsystem
 *
 * Sets up the x86 Interrupt Descriptor Table (IDT), remaps the 8259 PIC
 * so hardware IRQs start at vector 0x20, configures the PIT for the
 * scheduler tick, and enables interrupts.
 *
 * After interrupts_init() returns, IRQ0 (timer) and IRQ1 (keyboard) are
 * active.  All other IRQs are masked by default.
 */

#ifndef INTERRUPTS_H
#define INTERRUPTS_H

#include "types.h"

/*
 * interrupts_init - Fully initialise the interrupt subsystem.
 *
 * Must be called after the heap is ready and before scheduler_start().
 * Sets up the IDT, remaps the PIC, starts the PIT, and issues STI.
 */
void interrupts_init(void);

/*
 * interrupts_enable - Enable hardware interrupts (STI).
 */
void interrupts_enable(void);

/*
 * interrupts_disable - Disable hardware interrupts (CLI).
 *
 * Use sparingly; holding interrupts off for long periods will stall the
 * keyboard handler and the scheduler tick counter.
 */
void interrupts_disable(void);

/*
 * interrupts_halt - Execute HLT and return on the next interrupt.
 *
 * Intended for the idle task.  The CPU wakes on any enabled IRQ,
 * processes it, then returns from this call.
 */
void interrupts_halt(void);

#endif /* INTERRUPTS_H */
