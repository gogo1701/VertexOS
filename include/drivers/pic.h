/*
 * 8259 PIC (Programmable Interrupt Controller)
 *
 * Manages the master/slave PIC pair that routes hardware IRQs to the CPU.
 * After pic_remap() the IRQ vector layout is:
 *
 *   IRQ 0  -> vector 0x20  (PIT / system timer)
 *   IRQ 1  -> vector 0x21  (PS/2 keyboard)
 *   IRQ 2  -> vector 0x22  (cascade, not used directly)
 *   ...             ...
 *   IRQ 15 -> vector 0x2F
 *
 * Drivers must call pic_send_eoi() at the end of every IRQ handler,
 * otherwise the PIC will not deliver further interrupts on that line.
 */

#ifndef PIC_H
#define PIC_H

#include "types.h"

/*
 * pic_remap - Reinitialise both PICs with new vector offsets.
 *
 * @master_offset: Vector base for IRQs 0-7  (standard: 0x20).
 * @slave_offset:  Vector base for IRQs 8-15 (standard: 0x28).
 *
 * Must be called before enabling interrupts to avoid spurious CPU exceptions
 * caused by the default BIOS PIC mapping (vectors 0x08-0x0F).
 */
void pic_remap(u8 master_offset, u8 slave_offset);

/*
 * pic_set_mask - Mask (disable) an IRQ line.
 *
 * @irq_line: IRQ number 0-15.  Masked lines will not generate CPU interrupts.
 */
void pic_set_mask(u8 irq_line);

/*
 * pic_clear_mask - Unmask (enable) an IRQ line.
 *
 * @irq_line: IRQ number 0-15.
 */
void pic_clear_mask(u8 irq_line);

/*
 * pic_send_eoi - Acknowledge a processed IRQ.
 *
 * @irq_line: The IRQ number that was just handled.
 *
 * Must be called at the end of every hardware IRQ handler.  Failure to do
 * so will permanently block all further interrupts on the same (and higher)
 * priority lines.
 */
void pic_send_eoi(u8 irq_line);

#endif /* PIC_H */
