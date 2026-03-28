#ifndef INTERRUPTS_H
#define INTERRUPTS_H

#include "types.h"

/* Initialize IDT, remap PIC, configure PIT, and enable IRQs. */
void interrupts_init(void);

/* Explicit interrupt control helpers. */
void interrupts_enable(void);
void interrupts_disable(void);

/* Halt until the next interrupt arrives. */
void interrupts_halt(void);

#endif /* INTERRUPTS_H */
