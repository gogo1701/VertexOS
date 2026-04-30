#include "panic.h"
#include "display.h"
#include "video.h"
#include "pit.h"
#include "scheduler.h"
#include "interrupts.h"

#define PANIC_RULE_WIDTH 78u

static void panic_print_rule(void) {
    u32 cols = 0u;
    u32 width;
    u32 i;

    display_get_viewport(0, &cols);
    if (cols == 0u) {
        cols = PANIC_RULE_WIDTH;
    }

    width = cols < PANIC_RULE_WIDTH ? cols : PANIC_RULE_WIDTH;
    for (i = 0u; i < width; i++) {
        display_put_char('=');
    }
    display_put_char('\n');
}

static void panic_print_hex32(u32 value) {
    u32 shift;

    display_print("0x");
    for (shift = 28u;; shift -= 4u) {
        u32 nibble = (value >> shift) & 0xFu;
        display_put_char((char)(nibble < 10u ? ('0' + nibble) : ('A' + (nibble - 10u))));
        if (shift == 0u) {
            break;
        }
    }
}

static void panic_print_common_debug(const char* stop_code) {
    const task* current_task = scheduler_current_task();
    u32 current_tid = scheduler_current_tid();
    const video_fb_info* fb = video_get_fb_info();
    u32 row = 0u;
    u32 col = 0u;

    display_get_cursor(&row, &col);

    panic_print_rule();
    display_print("DEBUG DETAILS\n");
    panic_print_rule();

    display_print("Stop code   : ");
    display_print(stop_code);
    display_put_char('\n');

    display_print("Mode        : ");
    display_print(video_mode_name(video_get_mode()));
    display_put_char('\n');

    display_print("Resolution  : ");
    display_print(video_resolution_name(video_get_resolution()));
    display_put_char('\n');

    if (fb) {
        display_print("Framebuffer : ");
        display_print_num(fb->width, 10);
        display_put_char('x');
        display_print_num(fb->height, 10);
        display_print(" @ ");
        display_print_num((u32)fb->bpp, 10);
        display_print("bpp, pitch ");
        display_print_num(fb->pitch, 10);
        display_put_char('\n');

        display_print("FB phys     : ");
        panic_print_hex32(fb->fb_phys);
        display_put_char('\n');
    }

    display_print("Tick count  : ");
    display_print_num(pit_get_ticks(), 10);
    display_print(" (hz=");
    display_print_num(pit_get_frequency(), 10);
    display_print(")\n");

    display_print("Task ID     : ");
    if (current_tid == 0xFFFFFFFFu) {
        display_print("none");
    } else {
        display_print_num(current_tid, 10);
    }
    display_put_char('\n');

    display_print("Task name   : ");
    if (current_task && current_task->name) {
        display_print(current_task->name);
    } else {
        display_print("(unknown)");
    }
    display_put_char('\n');

    display_print("Cursor rc   : ");
    display_print_num(row, 10);
    display_put_char(',');
    display_print_num(col, 10);
    display_put_char('\n');

    display_print("Build       : ");
    display_print(__DATE__);
    display_put_char(' ');
    display_print(__TIME__);
    display_put_char('\n');
}

static void panic_setup_screen(void) {
    if (video_get_mode() == VIDEO_MODE_GRAPHICS) {
        display_set_panic_mode(1u);
        display_set_gfx_colors(15u, 1u, 1u);
    }
    display_clear();
}

static void panic_print_intro(const char* headline) {
    display_print(":(\n\n");
    display_print("Your VertexOS device ran into a problem and needs to stop.\n");
    display_print("The system has been halted to prevent further damage.\n\n");
    display_print("Error       : ");
    display_print(headline);
    display_put_char('\n');
}

void panic(const char* message) {
    interrupts_disable();
    panic_setup_screen();

    panic_print_intro("KERNEL PANIC");

    display_print("Reason      : ");
    display_print(message ? message : "(no message)");
    display_put_char('\n');

    panic_print_common_debug("KERNEL_PANIC");
    panic_print_rule();
    display_print("System halted. Reboot required.\n");

    for (;;) {
        interrupts_halt();
    }
}

void panic_assert_fail(const char* expr, const char* file, u32 line) {
    interrupts_disable();
    panic_setup_screen();

    panic_print_intro("ASSERTION FAILURE");

    display_print("Expression  : ");
    display_print(expr ? expr : "(unknown)");
    display_put_char('\n');

    display_print("Source      : ");
    display_print(file ? file : "(unknown)");
    display_put_char(':');
    display_print_num(line, 10);
    display_put_char('\n');

    panic_print_common_debug("ASSERT_FAILED");
    panic_print_rule();
    display_print("System halted. Reboot required.\n");

    for (;;) {
        interrupts_halt();
    }
}
