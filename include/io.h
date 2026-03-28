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
 * Read a 16-bit value from an I/O port.
 */
static inline u16 io_inw(u16 port) {
    u16 value;
    __asm__ __volatile__("inw %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

/*
 * Read a 32-bit value from an I/O port.
 */
static inline u32 io_inl(u16 port) {
    u32 value;
    __asm__ __volatile__("inl %1, %0" : "=a"(value) : "Nd"(port));
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

/*
 * Write a 16-bit value to an I/O port
 *
 * @port: The I/O port address to write to
 * @value: The 16-bit value to write
 */
static inline void io_outw(u16 port, u16 value) {
    __asm__ __volatile__("outw %0, %1" : : "a"(value), "Nd"(port));
}

/*
 * Write a 32-bit value to an I/O port.
 */
static inline void io_outl(u16 port, u32 value) {
    __asm__ __volatile__("outl %0, %1" : : "a"(value), "Nd"(port));
}

#endif /* IO_H */
