#include "mouse.h"
#include "display.h"
#include "framebuffer.h"
#include "io.h"
#include "pic.h"
#include "video.h"

#define KBC_CMD_PORT  0x64
#define KBC_DATA_PORT 0x60

#define KBC_STATUS_OBF 0x01
#define KBC_STATUS_IBF 0x02

#define MOUSE_CURSOR_SIZE 5
#define GFX_BUTTON_X 32
#define GFX_BUTTON_Y 48
#define GFX_BUTTON_W 80
#define GFX_BUTTON_H 16

static volatile s32 g_mouse_x = 160;
static volatile s32 g_mouse_y = 100;
static volatile u8 g_mouse_buttons = 0;
static volatile u8 g_mouse_enabled = 0;
static s32 g_mouse_prev_x = -1;
static s32 g_mouse_prev_y = -1;
static u8 g_mouse_prev_buffer[MOUSE_CURSOR_SIZE * MOUSE_CURSOR_SIZE];
static u8 g_mouse_prev_valid = 0;
static u8 g_mouse_packet[3];
static u8 g_mouse_phase = 0;

static u8 mouse_wait_input(void) {
    u32 timeout = 100000;
    while (timeout--) {
        if (!(io_inb(KBC_CMD_PORT) & KBC_STATUS_IBF)) {
            return 1;
        }
    }
    return 0;
}

static u8 mouse_wait_output(void) {
    u32 timeout = 100000;
    while (timeout--) {
        if (io_inb(KBC_CMD_PORT) & KBC_STATUS_OBF) {
            return 1;
        }
    }
    return 0;
}

static void mouse_flush_output(void) {
    while (io_inb(KBC_CMD_PORT) & KBC_STATUS_OBF) {
        (void)io_inb(KBC_DATA_PORT);
    }
}

static void mouse_write_cmd(u8 cmd) {
    if (!mouse_wait_input()) {
        return;
    }
    io_outb(KBC_CMD_PORT, cmd);
}

static void mouse_write_data(u8 data) {
    if (!mouse_wait_input()) {
        return;
    }
    io_outb(KBC_DATA_PORT, data);
}

static void mouse_write_mouse(u8 data) {
    if (!mouse_wait_input()) {
        return;
    }
    io_outb(KBC_CMD_PORT, 0xD4);
    if (!mouse_wait_input()) {
        return;
    }
    io_outb(KBC_DATA_PORT, data);
}

static u8 mouse_read_data(void) {
    if (!mouse_wait_output()) {
        return 0;
    }
    return io_inb(KBC_DATA_PORT);
}

static u8 mouse_get_framebuffer_pixel(s32 x, s32 y) {
    return framebuffer_get_pixel((u32)x, (u32)y);
}

static void mouse_set_framebuffer_pixel(s32 x, s32 y, u8 color) {
    framebuffer_set_pixel((u32)x, (u32)y, color);
}

static void mouse_restore_pointer(void) {
    if (!g_mouse_prev_valid) {
        return;
    }

    u32 row;
    for (row = 0; row < MOUSE_CURSOR_SIZE; row++) {
        u32 col;
        for (col = 0; col < MOUSE_CURSOR_SIZE; col++) {
            s32 x = g_mouse_prev_x + (s32)col;
            s32 y = g_mouse_prev_y + (s32)row;
            if (x < 0 || x >= (s32)framebuffer_width() || y < 0 || y >= (s32)framebuffer_height()) {
                continue;
            }
            mouse_set_framebuffer_pixel(x, y,
                g_mouse_prev_buffer[row * MOUSE_CURSOR_SIZE + col]);
        }
    }

    g_mouse_prev_valid = 0;
}

static void mouse_draw_pointer(void);

static void mouse_save_region(s32 x_start, s32 y_start) {
    u32 row;
    for (row = 0; row < MOUSE_CURSOR_SIZE; row++) {
        u32 col;
        for (col = 0; col < MOUSE_CURSOR_SIZE; col++) {
            s32 x = x_start + (s32)col;
            s32 y = y_start + (s32)row;
            if (x < 0 || x >= (s32)framebuffer_width() || y < 0 || y >= (s32)framebuffer_height()) {
                g_mouse_prev_buffer[row * MOUSE_CURSOR_SIZE + col] = 0;
            } else {
                g_mouse_prev_buffer[row * MOUSE_CURSOR_SIZE + col] =
                    mouse_get_framebuffer_pixel(x, y);
            }
        }
    }
}

void mouse_hide_cursor(void) {
    mouse_restore_pointer();
    g_mouse_prev_valid = 0;
}

void mouse_refresh_cursor(void) {
    if (g_mouse_prev_valid) {
        mouse_restore_pointer();
    }
    mouse_draw_pointer();
}

static void mouse_draw_pointer(void) {
    mouse_restore_pointer();

    if (g_mouse_x < 0 || g_mouse_x >= (s32)framebuffer_width() ||
        g_mouse_y < 0 || g_mouse_y >= (s32)framebuffer_height()) {
        return;
    }

    mouse_save_region(g_mouse_x, g_mouse_y);
    u32 row;
    for (row = 0; row < MOUSE_CURSOR_SIZE; row++) {
        u32 col;
        for (col = 0; col < MOUSE_CURSOR_SIZE; col++) {
            s32 x = g_mouse_x + (s32)col;
            s32 y = g_mouse_y + (s32)row;
            if (x < 0 || x >= (s32)framebuffer_width() || y < 0 || y >= (s32)framebuffer_height()) {
                continue;
            }
            mouse_set_framebuffer_pixel(x, y, 0u);
        }
    }
    g_mouse_prev_x = g_mouse_x;
    g_mouse_prev_y = g_mouse_y;
    g_mouse_prev_valid = 1;
}

static void mouse_enable_device(void) {
    mouse_flush_output();
    mouse_write_cmd(0xA8);

    mouse_flush_output();
    mouse_write_cmd(0x20);
    if (!mouse_wait_output()) {
        return;
    }
    u8 command_byte = mouse_read_data();
    command_byte |= 0x03; /* enable IRQ1 and IRQ12 */
    command_byte &= ~(1u << 5); /* enable auxiliary device */

    mouse_flush_output();
    mouse_write_cmd(0x60);
    mouse_write_data(command_byte);

    mouse_flush_output();
    mouse_write_mouse(0xF4);
    if (!mouse_wait_output()) {
        return;
    }
    mouse_read_data();

    g_mouse_enabled = 1;
}

void mouse_init(void) {
    g_mouse_x = (s32)(framebuffer_width() / 2u);
    g_mouse_y = (s32)(framebuffer_height() / 2u);
    g_mouse_buttons = 0;
    g_mouse_phase = 0;
    g_mouse_prev_valid = 0;
    g_mouse_enabled = 0;
    mouse_enable_device();
}

static void mouse_process_packet(void) {
    u8 status = g_mouse_packet[0];
    if (status & 0xC0) {
        /* Ignore overflow packets. */
        return;
    }

    s8 dx = (s8)g_mouse_packet[1];
    s8 dy = (s8)g_mouse_packet[2];

    g_mouse_x += dx * 4;
    g_mouse_y -= dy * 4;

    if (g_mouse_x < 0) {
        g_mouse_x = 0;
    } else if (g_mouse_x >= (s32)framebuffer_width() - MOUSE_CURSOR_SIZE) {
        g_mouse_x = (s32)framebuffer_width() - MOUSE_CURSOR_SIZE;
    }

    if (g_mouse_y < 0) {
        g_mouse_y = 0;
    } else if (g_mouse_y >= (s32)framebuffer_height() - MOUSE_CURSOR_SIZE) {
        g_mouse_y = (s32)framebuffer_height() - MOUSE_CURSOR_SIZE;
    }

    g_mouse_buttons = status & 0x07;

    u8 pressed = 0;
    if ((g_mouse_buttons & 0x01) &&
        video_get_mode() == VIDEO_MODE_GRAPHICS &&
        g_mouse_x >= (s32)GFX_BUTTON_X && g_mouse_x < (s32)(GFX_BUTTON_X + GFX_BUTTON_W) &&
        g_mouse_y >= (s32)GFX_BUTTON_Y && g_mouse_y < (s32)(GFX_BUTTON_Y + GFX_BUTTON_H)) {
        pressed = 1;
    }

    if (video_get_mode() == VIDEO_MODE_GRAPHICS) {
        display_set_button_pressed(pressed);
        display_handle_mouse_event(g_mouse_x, g_mouse_y, g_mouse_buttons);
        mouse_draw_pointer();
        framebuffer_flush();
    }
}

void mouse_irq_handler(void) {
    u8 data = io_inb(KBC_DATA_PORT);

    if (g_mouse_phase == 0) {
        if (!(data & 0x08)) {
            return;
        }
        g_mouse_packet[0] = data;
        g_mouse_phase = 1;
        return;
    }

    if (g_mouse_phase == 1) {
        g_mouse_packet[1] = data;
        g_mouse_phase = 2;
        return;
    }

    if (g_mouse_phase == 2) {
        g_mouse_packet[2] = data;
        g_mouse_phase = 0;
        mouse_process_packet();
    }
}

s32 mouse_get_x(void) {
    return g_mouse_x;
}

s32 mouse_get_y(void) {
    return g_mouse_y;
}

u8 mouse_get_buttons(void) {
    return g_mouse_buttons;
}
