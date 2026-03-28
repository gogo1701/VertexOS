/*
 * Keyboard Input Management Implementation
 */

#include "keyboard.h"
#include "interrupts.h"
#include "io.h"

#define KB_BUFFER_SIZE 128

static volatile char kb_buffer[KB_BUFFER_SIZE];
static volatile u32 kb_head = 0;
static volatile u32 kb_tail = 0;

static u8 kb_buffer_is_empty(void) {
    return kb_head == kb_tail;
}

static u8 kb_buffer_is_full(void) {
    return ((kb_head + 1) % KB_BUFFER_SIZE) == kb_tail;
}

static void kb_buffer_push(char c) {
    if (kb_buffer_is_full()) {
        return;
    }

    kb_buffer[kb_head] = c;
    kb_head = (kb_head + 1) % KB_BUFFER_SIZE;
}

static char kb_buffer_pop(void) {
    char c = kb_buffer[kb_tail];
    kb_tail = (kb_tail + 1) % KB_BUFFER_SIZE;
    return c;
}

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

void keyboard_init(void) {
    kb_head = 0;
    kb_tail = 0;
}

void keyboard_irq_handler(void) {
    u8 scancode;
    char c;

    /* Read keyboard scancode from data port. */
    scancode = io_inb(0x60);

    /* Ignore key-release events. */
    if (scancode & 0x80) {
        return;
    }

    c = scancode_to_ascii(scancode);
    if (c) {
        kb_buffer_push(c);
    }
}

char keyboard_read_char(void) {
    for (;;) {
        interrupts_disable();
        if (!kb_buffer_is_empty()) {
            char c = kb_buffer_pop();
            interrupts_enable();
            return c;
        }
        interrupts_enable();

        /* Sleep until an IRQ wakes the CPU. */
        interrupts_halt();
    }
}
