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
#include "mouse.h"
#include "commands.h"
#include "cli.h"
#include "bootinfo.h"
#include "heap.h"
#include "interrupts.h"
#include "paging.h"
#include "pmm.h"
#include "scheduler.h"
#include "serial.h"
#include "userland.h"
#include "video.h"
#include "vfs.h"

extern u8 _kernel_start;
extern u8 _kernel_end;

#define HEAP_START 0x01000000u
#define HEAP_SIZE  (1024u * 1024u)

static void cli_task(void* arg) {
    (void)arg;
    cli_run();
}

static void idle_task(void* arg) {
    (void)arg;
    for (;;) {
        /* Let timer-driven preemption switch back to runnable tasks. */
        scheduler_maybe_preempt();
        interrupts_halt();
    }
}

/* ============================
 * Main Kernel Entry Point
 * ============================ */

/*
 * Main kernel function - entry point after bootloader
 * 
 * Initializes subsystems and runs the command-line interface.
 * This function never returns in normal operation.
 */
void kmain(u32 boot_video_mode, const e820_entry* e820_map, u32 e820_count) {
    memory_region regions[E820_MAX_ENTRIES];
    u32 region_count;

    /* Initialize subsystems */
    serial_init();
    video_init((video_mode)boot_video_mode);
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
    mouse_init();
    interrupts_init();
    vfs_init();
    userland_seed_programs();
    commands_init();
    scheduler_init();

    /* Display welcome message */
    display_print("VertexOS - Simple Console\n");
    display_print("Type 'help' for available commands.\n\n");

    /* Start scheduler with CLI and idle tasks. */
    scheduler_create_task(cli_task, 0, "cli", TASK_MODE_KERNEL);
    scheduler_create_task(idle_task, 0, "idle", TASK_MODE_KERNEL);
    scheduler_start();
}
