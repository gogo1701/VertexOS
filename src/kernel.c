/*
 * VertexOS Kernel
 * 
 * A minimal 32-bit kernel that manages VGA text mode display and handles
 * keyboard input. This kernel demonstrates basic OS concepts including:
 * - Memory mapping and I/O
 * - Display buffer management
 * - Keyboard input handling
 * - Modular command system
 */

#include "display.h"
#include "keyboard.h"
#include "commands.h"

#define INPUT_MAX 128

/* ============================
 * Main Kernel Entry Point
 * ============================ */

/*
 * Main kernel function - entry point after bootloader
 * 
 * Initializes subsystems and runs the command-line interface.
 * This function never returns in normal operation.
 */
void kmain(void) {
    char input[INPUT_MAX];  /* Buffer for user input */
    u32 len = 0;            /* Current length of input buffer */

    /* Initialize subsystems */
    display_init();
    commands_init();
    
    /* Display welcome message */
    display_print("VertexOS - Simple Console\n");
    display_print("Type 'help' for available commands.\n\n");
    display_print("> ");

    /* Main event loop - process commands forever */
    for (;;) {
        /* Read character from keyboard */
        char c = keyboard_read_char();

        /* Handle backspace: remove last character from buffer and display */
        if (c == '\b') {
            if (len > 0) {
                len--;
                display_put_char('\b');  /* Erase on screen */
            }
            continue;
        }

        /* Handle Enter: process the command */
        if (c == '\n') {
            input[len] = '\0';   /* Null-terminate the string */
            display_put_char('\n');
            
            /* Try to execute the command */
            if (!command_execute(input)) {
                /* Command not found */
                display_print("Unknown command: ");
                display_print(input);
                display_put_char('\n');
            }
            
            display_print("> ");  /* Prompt for next input */
            len = 0;              /* Reset buffer for next input */
            continue;
        }

        /* Handle regular characters: add to buffer and display */
        if (len < INPUT_MAX - 1) {
            input[len] = c;      /* Store in buffer */
            len++;
            display_put_char(c); /* Echo to screen */
        }
        /* If buffer is full, silently ignore additional input */
    }
}
