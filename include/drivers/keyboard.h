/*
 * Keyboard Input Management
 * 
 * Handles PS/2 keyboard input and scancode-to-ASCII conversion
 */

#ifndef KEYBOARD_H
#define KEYBOARD_H

#include "types.h"

#define KEY_LEFT   0x100
#define KEY_RIGHT  0x101
#define KEY_DELETE 0x102
#define KEY_UP     0x103
#define KEY_DOWN   0x104
#define KEY_F2     0x105
#define KEY_F10    0x106

/* Initialize keyboard state used by IRQ-driven buffering. */
void keyboard_init(void);

/* IRQ1 handler entrypoint called from interrupt dispatcher. */
void keyboard_irq_handler(void);

/* Read a key event (ASCII or KEY_* constant), blocking. */
s32 keyboard_read_key(void);

/* Read a key event for a specific terminal session, blocking. */
s32 keyboard_read_key_for_terminal(u32 terminal_session);

/*
 * Read a character from the keyboard (blocking)
 * 
 * Waits for a key press and converts the scancode to ASCII
 * 
 * @return: ASCII character of the key pressed
 */
char keyboard_read_char(void);

#endif /* KEYBOARD_H */
