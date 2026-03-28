/*
 * Low-level I/O port operations
 */

#ifndef IO_H
#define IO_H

#include "types.h"

/*
 * Read a byte from an I/O port
 * 
 * @port: The I/O port address to read from
 * @return: The byte value read from the port
 */
static inline u8 io_inb(u16 port) {
    u8 value;
    __asm__ __volatile__("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

/*
 * Write a byte to an I/O port
 * 
 * @port: The I/O port address to write to
 * @value: The byte value to write
 */
static inline void io_outb(u16 port, u8 value) {
    __asm__ __volatile__("outb %0, %1" : : "a"(value), "Nd"(port));
}

#endif /* IO_H */
