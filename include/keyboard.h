/*
 * Keyboard Input Management
 * 
 * Handles PS/2 keyboard input and scancode-to-ASCII conversion
 */

#ifndef KEYBOARD_H
#define KEYBOARD_H

#include "types.h"

/* Initialize keyboard state used by IRQ-driven buffering. */
void keyboard_init(void);

/* IRQ1 handler entrypoint called from interrupt dispatcher. */
void keyboard_irq_handler(void);

/*
 * Read a character from the keyboard (blocking)
 * 
 * Waits for a key press and converts the scancode to ASCII
 * 
 * @return: ASCII character of the key pressed
 */
char keyboard_read_char(void);

#endif /* KEYBOARD_H */
