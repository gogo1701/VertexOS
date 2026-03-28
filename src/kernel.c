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
#include "cli.h"
#include "interrupts.h"

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
    /* Initialize subsystems */
    display_init();
    keyboard_init();
    interrupts_init();
    commands_init();

    /* Display welcome message */
    display_print("VertexOS - Simple Console\n");
    display_print("Type 'help' for available commands.\n\n");

    /* Enter the command-line interface */
    cli_run();
}
