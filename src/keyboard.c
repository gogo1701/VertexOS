/*
 * Keyboard Input Management Implementation
 */

#include "keyboard.h"
#include "io.h"

/*
 * Convert PS/2 keyboard scancode to ASCII character
 */
static char scancode_to_ascii(u8 scancode) {
    /* US QWERTY keyboard scancode to ASCII mapping table */
    static const char map[128] = {
        0,      /* 0x00: Invalid */
        27,     /* 0x01: ESC */
        '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=',
        '\b',   /* 0x0E: Backspace */
        '\t',   /* 0x0F: Tab */
        'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']',
        '\n',   /* 0x1C: Enter */
        0,      /* 0x1D: Left Ctrl */
        'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
        0,      /* 0x2A: Left Shift */
        '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/',
        0,      /* 0x36: Right Shift */
        '*',    /* 0x37: Keypad Multiply */
        0,      /* 0x38: Left Alt */
        ' '     /* 0x39: Spacebar */
    };

    /* Look up scancode in the mapping table */
    if (scancode < 128) {
        return map[scancode];
    }
    return 0;  /* Invalid or key-release scancode */
}

/*
 * Wait for and read a keyboard scancode
 */
static u8 read_scancode_blocking(void) {
    for (;;) {
        /* Poll port 0x64: bit 0 indicates data available */
        if (io_inb(0x64) & 1) {
            /* Data ready: read byte from port 0x60 (keyboard data port) */
            return io_inb(0x60);
        }
    }
}

char keyboard_read_char(void) {
    for (;;) {
        u8 scancode = read_scancode_blocking();
        char c;

        /* Ignore key-release events (scancode with high bit set) */
        if (scancode & 0x80) {
            continue;
        }

        /* Convert scancode to ASCII character */
        c = scancode_to_ascii(scancode);
        
        /* Skip unmapped scancodes (like Ctrl, Shift, Alt) */
        if (c) {
            return c;
        }
    }
}
