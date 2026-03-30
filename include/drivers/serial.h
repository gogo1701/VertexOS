/*
 * Serial Port (COM1) Driver
 *
 * Provides polled output to COM1 (I/O base 0x3F8, 115200 baud, 8-N-1).
 * All display output is automatically mirrored here so that kernel
 * messages are visible in QEMU's "-serial stdio" output and on real
 * hardware via a null-modem cable or USB-serial adapter.
 *
 * Serial input is not implemented; this driver is output-only.
 */

#ifndef SERIAL_H
#define SERIAL_H

#include "types.h"

/*
 * serial_init - Initialise COM1 at 115200 baud, 8-N-1, no interrupts.
 *
 * Must be called early in kmain(), before any display_print() calls,
 * so that all boot messages are captured on the serial line.
 */
void serial_init(void);

/*
 * serial_is_ready - Check whether the transmit FIFO can accept a byte.
 *
 * @return: 1 if the transmit-holding-register-empty bit is set, else 0.
 *
 * Normally not needed by callers; serial_write_char() polls internally.
 */
u8 serial_is_ready(void);

/*
 * serial_write_char - Write a single character to COM1 (blocking poll).
 *
 * Spins until the transmit register is empty, then writes the byte.
 *
 * @c: Character to send.
 */
void serial_write_char(char c);

/*
 * serial_write - Write a null-terminated string to COM1.
 *
 * @s: String to send.  Each character is written with serial_write_char().
 */
void serial_write(const char* s);

/*
 * serial_write_hex32 - Write a 32-bit value as "0xXXXXXXXX\n" to COM1.
 */
void serial_write_hex32(u32 val);

/*
 * serial_write_dec - Write a 32-bit value as a decimal number to COM1.
 */
void serial_write_dec(u32 val);

#endif /* SERIAL_H */
