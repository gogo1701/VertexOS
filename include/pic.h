#ifndef PIC_H
#define PIC_H

#include "types.h"

/* Remap master/slave PIC vector offsets (commonly 0x20/0x28). */
void pic_remap(u8 master_offset, u8 slave_offset);

/* Mask/unmask a specific IRQ line (0-15). */
void pic_set_mask(u8 irq_line);
void pic_clear_mask(u8 irq_line);

/* Send end-of-interrupt to PIC for a handled IRQ. */
void pic_send_eoi(u8 irq_line);

#endif /* PIC_H */
