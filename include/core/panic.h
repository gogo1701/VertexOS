/*
 * Kernel Panic and Assertion Utilities
 *
 * Provides hard-stop error reporting for unrecoverable conditions.  All
 * paths disable interrupts, print a diagnostic message to both the VGA
 * console and the serial port, then halt the CPU permanently.
 */

#ifndef PANIC_H
#define PANIC_H

#include "types.h"

/*
 * panic - Halt the system with a fatal error message.
 *
 * @message: Human-readable description of the failure.
 *
 * Disables interrupts, prints the message on screen, and spins forever.
 * Never returns.
 */
void panic(const char* message);

/*
 * panic_assert_fail - Called automatically by KASSERT on failure.
 *
 * @expr: Stringified failed expression.
 * @file: Source file name (__FILE__).
 * @line: Source line number (__LINE__).
 *
 * Do not call directly; use the KASSERT macro instead.
 */
void panic_assert_fail(const char* expr, const char* file, u32 line);

/*
 * KASSERT - Kernel assertion macro.
 *
 * Evaluates expr at runtime.  If it is false, calls panic_assert_fail
 * with the source location and halts the system.
 *
 * Example:
 *   KASSERT(ptr != 0);
 *   KASSERT(size <= PAGE_SIZE);
 */
#define KASSERT(expr) \
    do { \
        if (!(expr)) { \
            panic_assert_fail(#expr, __FILE__, __LINE__); \
        } \
    } while (0)

#endif /* PANIC_H */
