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
#include "bootinfo.h"
#include "heap.h"
#include "interrupts.h"
#include "paging.h"
#include "pmm.h"

extern u8 _kernel_start;
extern u8 _kernel_end;

#define HEAP_START 0x01000000u
#define HEAP_SIZE  (1024u * 1024u)

/* ============================
 * Main Kernel Entry Point
 * ============================ */

/*
 * Main kernel function - entry point after bootloader
 * 
 * Initializes subsystems and runs the command-line interface.
 * This function never returns in normal operation.
 */
void kmain(const e820_entry* e820_map, u32 e820_count) {
    memory_region regions[E820_MAX_ENTRIES];
    u32 region_count;

    /* Initialize subsystems */
    display_init();
    region_count = bootinfo_get_usable_regions(
        e820_map,
        e820_count,
        regions,
        E820_MAX_ENTRIES
    );

    pmm_init(regions, region_count, (u32)&_kernel_start, (u32)&_kernel_end);
    paging_init_identity(0x02000000u);
    heap_init(HEAP_START, HEAP_SIZE);

    keyboard_init();
    interrupts_init();
    commands_init();

    /* Display welcome message */
    display_print("VertexOS - Simple Console\n");
    display_print("Type 'help' for available commands.\n\n");

    /* Enter the command-line interface */
    cli_run();
}
