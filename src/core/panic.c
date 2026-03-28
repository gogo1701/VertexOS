#include "panic.h"
#include "display.h"
#include "video.h"
#include "interrupts.h"

void panic(const char* message) {
    interrupts_disable();

    if (video_get_mode() == VIDEO_MODE_GRAPHICS) {
        display_set_gfx_colors(15u, 1u, 1u);
        display_clear();
    }

    display_print("\n\nKERNEL PANIC: ");
    display_print(message);
    display_print("\nSystem halted.\n");

    for (;;) {
        interrupts_halt();
    }
}

void panic_assert_fail(const char* expr, const char* file, u32 line) {
    interrupts_disable();

    if (video_get_mode() == VIDEO_MODE_GRAPHICS) {
        display_set_gfx_colors(15u, 1u, 1u);
        display_clear();
    }

    display_print("\n\nASSERT FAILED: ");
    display_print(expr);
    display_print("\nAt: ");
    display_print(file);
    display_print(":");
    display_print_num(line, 10);
    display_put_char('\n');
    display_print("System halted.\n");

    for (;;) {
        interrupts_halt();
    }
}
