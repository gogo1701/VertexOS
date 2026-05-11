/*
 * Keyboard Input Management Implementation
 */

#include "keyboard.h"
#include "display.h"
#include "interrupts.h"
#include "io.h"
#include "scheduler.h"

#define KB_BUFFER_SIZE 128
#define TERMINAL_SESSION_COUNT DISPLAY_MAX_TERMINALS

static volatile s32 kb_buffer[KB_BUFFER_SIZE];
static volatile u32 kb_head = 0;
static volatile u32 kb_tail = 0;
static volatile s32 term_kb_buffer[TERMINAL_SESSION_COUNT][KB_BUFFER_SIZE];
static volatile u32 term_kb_head[TERMINAL_SESSION_COUNT];
static volatile u32 term_kb_tail[TERMINAL_SESSION_COUNT];
static volatile u8 kb_extended = 0;
static volatile u8 kb_shift_down = 0;

static u8 kb_buffer_is_empty(void) {
    return kb_head == kb_tail;
}

static u8 kb_buffer_is_full(void) {
    return ((kb_head + 1) % KB_BUFFER_SIZE) == kb_tail;
}

static void kb_buffer_push(s32 key) {
    if (kb_buffer_is_full()) {
        return;
    }

    kb_buffer[kb_head] = key;
    kb_head = (kb_head + 1) % KB_BUFFER_SIZE;
}

static s32 kb_buffer_pop(void) {
    s32 key = kb_buffer[kb_tail];
    kb_tail = (kb_tail + 1) % KB_BUFFER_SIZE;
    return key;
}

static u8 term_kb_buffer_is_empty(u32 terminal_session) {
    return term_kb_head[terminal_session] == term_kb_tail[terminal_session];
}

static u8 term_kb_buffer_is_full(u32 terminal_session) {
    return ((term_kb_head[terminal_session] + 1u) % KB_BUFFER_SIZE) == term_kb_tail[terminal_session];
}

static void term_kb_buffer_push(u32 terminal_session, s32 key) {
    if (terminal_session >= TERMINAL_SESSION_COUNT || term_kb_buffer_is_full(terminal_session)) {
        return;
    }

    term_kb_buffer[terminal_session][term_kb_head[terminal_session]] = key;
    term_kb_head[terminal_session] = (term_kb_head[terminal_session] + 1u) % KB_BUFFER_SIZE;
}

static s32 term_kb_buffer_pop(u32 terminal_session) {
    s32 key = term_kb_buffer[terminal_session][term_kb_tail[terminal_session]];
    term_kb_tail[terminal_session] = (term_kb_tail[terminal_session] + 1u) % KB_BUFFER_SIZE;
    return key;
}

static void keyboard_route_key(s32 key) {
    s32 focused_terminal = display_get_focused_terminal_session();
    if (focused_terminal >= 0 && focused_terminal < (s32)TERMINAL_SESSION_COUNT) {
        term_kb_buffer_push((u32)focused_terminal, key);
    } else {
        kb_buffer_push(key);
    }
}

/*
 * Convert PS/2 keyboard scancode to ASCII character
 */
static char scancode_to_ascii(u8 scancode, u8 shift_down) {
    /* US QWERTY keyboard scancode to ASCII mapping tables. */
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

    static const char shift_map[128] = {
        0,      /* 0x00: Invalid */
        27,     /* 0x01: ESC */
        '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+',
        '\b',   /* 0x0E: Backspace */
        '\t',   /* 0x0F: Tab */
        'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}',
        '\n',   /* 0x1C: Enter */
        0,      /* 0x1D: Left Ctrl */
        'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
        0,      /* 0x2A: Left Shift */
        '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?',
        0,      /* 0x36: Right Shift */
        '*',    /* 0x37: Keypad Multiply */
        0,      /* 0x38: Left Alt */
        ' '     /* 0x39: Spacebar */
    };

    /* Look up scancode in the mapping table */
    if (scancode < 128) {
        return shift_down ? shift_map[scancode] : map[scancode];
    }
    return 0;  /* Invalid or key-release scancode */
}

void keyboard_init(void) {
    u32 i;

    kb_head = 0;
    kb_tail = 0;
    kb_extended = 0;
    kb_shift_down = 0;
    for (i = 0u; i < TERMINAL_SESSION_COUNT; i++) {
        term_kb_head[i] = 0u;
        term_kb_tail[i] = 0u;
    }
}

void keyboard_irq_handler(void) {
    u8 scancode;
    s32 key = 0;

    /* Read keyboard scancode from data port. */
    scancode = io_inb(0x60);

    if (scancode == 0xE0) {
        kb_extended = 1;
        return;
    }

    if (kb_extended) {
        if (!(scancode & 0x80)) {
            if (scancode == 0x4B) {
                key = KEY_LEFT;
            } else if (scancode == 0x4D) {
                key = KEY_RIGHT;
            } else if (scancode == 0x53) {
                key = KEY_DELETE;
            } else if (scancode == 0x48) {
                key = KEY_UP;
            } else if (scancode == 0x50) {
                key = KEY_DOWN;
            }
        }

        kb_extended = 0;
        if (key) {
            keyboard_route_key(key);
        }
        return;
    }

    if (scancode & 0x80) {
        u8 released = (u8)(scancode & 0x7Fu);
        if (released == 0x2Au || released == 0x36u) {
            kb_shift_down = 0u;
        }
        return;
    }

    if (scancode == 0x2Au || scancode == 0x36u) {
        kb_shift_down = 1u;
        return;
    }

    if (scancode == 0x3C) {
        keyboard_route_key(KEY_F2);
        return;
    }

    if (scancode == 0x44) {
        keyboard_route_key(KEY_F10);
        return;
    }

    key = (s32)scancode_to_ascii(scancode, kb_shift_down);
    if (key) {
        keyboard_route_key(key);
    }
}

s32 keyboard_read_key(void) {
    s32 terminal_session = display_terminal_session_for_task(scheduler_current_tid());

    if (terminal_session >= 0 && terminal_session < (s32)TERMINAL_SESSION_COUNT) {
        return keyboard_read_key_for_terminal((u32)terminal_session);
    }

    for (;;) {
        interrupts_disable();
        if (!kb_buffer_is_empty()) {
            s32 key = kb_buffer_pop();
            interrupts_enable();
            return key;
        }
        interrupts_enable();

        scheduler_maybe_preempt();

        /* Sleep until an IRQ wakes the CPU. */
        interrupts_halt();
    }
}

s32 keyboard_read_key_for_terminal(u32 terminal_session) {
    if (terminal_session >= TERMINAL_SESSION_COUNT) {
        return keyboard_read_key();
    }

    for (;;) {
        interrupts_disable();
        if (!term_kb_buffer_is_empty(terminal_session)) {
            s32 key = term_kb_buffer_pop(terminal_session);
            interrupts_enable();
            return key;
        }
        interrupts_enable();

        scheduler_maybe_preempt();
        interrupts_halt();
    }
}

char keyboard_read_char(void) {
    for (;;) {
        s32 key = keyboard_read_key();
        if (key >= 0 && key <= 0xFF) {
            return (char)key;
        }
    }
}
