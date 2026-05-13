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
#include "net.h"
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
#define IDENTITY_MAP_LIMIT 0x02000000u

static void map_graphics_framebuffer_if_needed(void) {
    const video_fb_info* fb = video_get_fb_info();

    if (video_get_mode() != VIDEO_MODE_GRAPHICS || !fb || !fb->fb_phys ||
        !fb->pitch || !fb->height || fb->bpp != 8u) {
        serial_write("[DBG kern] fb map skipped (text mode or no fb)\n");
        return;
    }

    {
        u32 start = fb->fb_phys & 0xFFFFF000u;
        u32 end = fb->fb_phys + (fb->pitch * fb->height);
        u32 addr;

        end = (end + 0xFFFu) & 0xFFFFF000u;
        if (end <= IDENTITY_MAP_LIMIT) {
            serial_write("[DBG kern] fb within identity map, no extra pages needed\n");
            return;
        }

        serial_write("[DBG kern] mapping fb pages ");
        serial_write_hex32(start);
        serial_write(" - ");
        serial_write_hex32(end);
        serial_write_char('\n');

        for (addr = start; addr < end; addr += 0x1000u) {
            paging_map_page(addr, addr, 0x002u);
        }
    }

    paging_reload_directory();
    serial_write("[DBG kern] fb page mapping done\n");
}

static void idle_task(void* arg) {
    (void)arg;
    for (;;) {
        net_poll();
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
    video_init(boot_video_mode);
    region_count = bootinfo_get_usable_regions(
        e820_map,
        e820_count,
        regions,
        E820_MAX_ENTRIES
    );

    pmm_init(regions, region_count, (u32)&_kernel_start, (u32)&_kernel_end);
    paging_init_identity(IDENTITY_MAP_LIMIT);
    map_graphics_framebuffer_if_needed();
    heap_init(HEAP_START, HEAP_SIZE);

    display_init();
    keyboard_init();
    mouse_init();
    interrupts_init();
    net_init();
    vfs_init();
    userland_seed_programs();
    commands_init();
    scheduler_init();

    /* Start scheduler with CLI and idle tasks. */
    {
        (void)display_create_terminal_session(1u);
        (void)display_create_terminal_session(0u);
        (void)display_create_terminal_session(0u);

        /* Display welcome message */
        display_print("VertexOS - Simple Console\n");
        display_print("Type 'help' for available commands.\n\n");

        scheduler_create_task(idle_task, 0, "idle", TASK_MODE_KERNEL);
    }
    scheduler_start();
}
