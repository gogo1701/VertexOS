/*
 * VertexOS Kernel
 * 
 * A minimal 32-bit kernel that manages VGA text mode display and handles
 * keyboard input. This kernel demonstrates basic OS concepts including:
 * - Memory mapping and I/O
 * - Display buffer management
 * - Keyboard interrupt handling
 * - Basic command-line interface
 */

/* Type definitions for convenience */
typedef unsigned char u8;      /* 8-bit unsigned integer  */
typedef unsigned short u16;    /* 16-bit unsigned integer */
typedef unsigned int u32;      /* 32-bit unsigned integer */

/* VGA Display Constants */
#define VGA_WIDTH 80           /* Characters per row */
#define VGA_HEIGHT 25          /* Total rows of text */
#define INPUT_MAX 128          /* Maximum input buffer size */

/* VGA Memory and Display State */
static volatile u16* const VGA = (u16*)0xB8000;  /* VGA text buffer address */
static const u8 COLOR = 0x0F;                     /* White text on black background */
static u32 cursor_row = 0;                        /* Current cursor row position */
static u32 cursor_col = 0;                        /* Current cursor column position */

/* 
 * Read a byte from an I/O port
 * 
 * This is a low-level x86 instruction that reads a single byte from
 * the specified I/O port. Used for hardware communication like keyboard input.
 * 
 * @port: The I/O port address to read from
 * @return: The byte value read from the port
 */
static inline u8 inb(u16 port) {
    u8 value;
    __asm__ __volatile__("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

/* ============================
 * Display Management Functions
 * ============================ */

/*
 * Clear the entire screen and reset the cursor position
 * 
 * Fills all VGA memory locations with spaces to create a blank display.
 * The color byte (0x0F) represents white text on black background.
 */
static void clear_screen(void) {
    u32 i;
    /* Fill entire VGA buffer with blank characters */
    for (i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        VGA[i] = (u16)(' ' | (COLOR << 8));
    }
    /* Reset cursor to top-left */
    cursor_row = 0;
    cursor_col = 0;
}

/*
 * Scroll the display up one line if cursor has reached the bottom
 * 
 * When the cursor moves below the bottom of the screen, this function:
 * - Moves all visible lines up by one row
 * - Clears the bottom line
 * - Positions the cursor at the start of the new bottom line
 */
static void scroll_if_needed(void) {
    u32 row;
    u32 col;

    /* Only scroll if cursor is beyond the bottom of the screen */
    if (cursor_row < VGA_HEIGHT) {
        return;
    }

    /* Move each row up one position */
    for (row = 1; row < VGA_HEIGHT; row++) {
        for (col = 0; col < VGA_WIDTH; col++) {
            VGA[(row - 1) * VGA_WIDTH + col] = VGA[row * VGA_WIDTH + col];
        }
    }

    /* Clear the bottom line with blank spaces */
    for (col = 0; col < VGA_WIDTH; col++) {
        VGA[(VGA_HEIGHT - 1) * VGA_WIDTH + col] = (u16)(' ' | (COLOR << 8));
    }

    /* Position cursor at start of bottom line */
    cursor_row = VGA_HEIGHT - 1;
}

/*
 * Display a single character at the current cursor position
 * 
 * Handles special characters:
 * - '\n': Move to next line
 * - '\b': Backspace (move cursor back and erase)
 * - Other: Print normally and advance cursor
 * 
 * @c: The character to display
 */
static void put_char(char c) {
    /* Handle newline: move to next line */
    if (c == '\n') {
        cursor_col = 0;
        cursor_row++;
        scroll_if_needed();
        return;
    }

    /* Handle backspace: erase previous character if possible */
    if (c == '\b') {
        if (cursor_col > 0) {
            cursor_col--;
            VGA[cursor_row * VGA_WIDTH + cursor_col] = (u16)(' ' | (COLOR << 8));
        }
        return;
    }

    /* Write character to VGA buffer with color attribute */
    VGA[cursor_row * VGA_WIDTH + cursor_col] = (u16)((u8)c | (COLOR << 8));
    cursor_col++;

    /* Wrap to next line if cursor reaches the end of screen width */
    if (cursor_col >= VGA_WIDTH) {
        cursor_col = 0;
        cursor_row++;
        scroll_if_needed();
    }
}

/*
 * Print a null-terminated string
 * 
 * Display entire string by calling put_char for each character.
 * 
 * @s: Pointer to the string to print
 */
static void print(const char* s) {
    while (*s) {
        put_char(*s);
        s++;
    }
}

/* ============================
 * Keyboard Input Functions
 * ============================ */

/*
 * Convert PS/2 keyboard scancode to ASCII character
 * 
 * Maps hardware keyboard scancodes to their ASCII character equivalents.
 * Uses a lookup table that covers the US keyboard layout.
 * 
 * Scancode 0x80 and above represent key release events (ignored).
 * See PS/2 Keyboard Scancode Set 1 documentation for details.
 * 
 * @scancode: The PS/2 scancode to convert
 * @return: ASCII character, or 0 if scancode doesn't map to a character
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
 * 
 * Polls the keyboard controller status port (0x64) waiting for data.
 * When data is available (bit 0 set), reads the scancode from port 0x60.
 * 
 * This is a busy-wait (polling) approach, not interrupt-driven.
 * 
 * @return: The scancode read from the keyboard
 */
static u8 read_scancode_blocking(void) {
    for (;;) {
        /* Poll port 0x64: bit 0 indicates data available */
        if (inb(0x64) & 1) {
            /* Data ready: read byte from port 0x60 (keyboard data port) */
            return inb(0x60);
        }
    }
}

/* ============================
 * Main Kernel Entry Point
 * ============================ */

/*
 * Main kernel function - entry point after bootloader
 * 
 * Initializes the display and implements a simple command-line interface:
 * 1. Clears the screen and shows welcome message
 * 2. Enters an infinite loop reading keyboard input
 * 3. Echoes characters as the user types
 * 4. On Enter, displays the complete input and waits for next command
 * 
 * This function never returns in normal operation.
 */
void kmain(void) {
    char input[INPUT_MAX];  /* Buffer for user input */
    u32 len = 0;            /* Current length of input buffer */

    /* Initialize display */
    clear_screen();
    print("Simple C console\n");
    print("Type and press Enter.\n\n");
    print("> ");

    /* Main event loop - process keyboard input forever */
    for (;;) {
        /* Read raw keyboard scancode from controller */
        u8 scancode = read_scancode_blocking();
        char c;

        /* Ignore key-release events (scancode with high bit set) */
        if (scancode & 0x80) {
            continue;
        }

        /* Convert scancode to ASCII character */
        c = scancode_to_ascii(scancode);
        /* Skip unmapped scancodes (like Ctrl, Shift, Alt) */
        if (!c) {
            continue;
        }

        /* Handle backspace: remove last character from buffer and display */
        if (c == '\b') {
            if (len > 0) {
                len--;
                put_char('\b');  /* Erase on screen */
            }
            continue;
        }

        /* Handle Enter: process the complete input line */
        if (c == '\n') {
            input[len] = '\0';   /* Null-terminate the string */
            put_char('\n');
            print("You said: ");
            print(input);
            put_char('\n');
            put_char('\n');
            print("> ");         /* Prompt for next input */
            len = 0;             /* Reset buffer for next input */
            continue;
        }

        /* Handle regular characters: add to buffer and display */
        if (len < INPUT_MAX - 1) {
            input[len] = c;      /* Store in buffer */
            len++;
            put_char(c);         /* Echo to screen */
        }
        /* If buffer is full, silently ignore additional input */
    }
}
