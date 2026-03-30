#include "framebuffer.h"
#include "video.h"
#include "serial.h"

static volatile u8* g_fb = (u8*)0xA0000;
static u32 g_width = 320u;
static u32 g_height = 200u;
static u32 g_pitch = 320u;

void framebuffer_init(void) {
    const video_fb_info* info = video_get_fb_info();
    if (!info) {
        serial_write("[DBG fb] video_get_fb_info returned NULL\n");
        return;
    }

    if (info->fb_phys) {
        g_fb = (volatile u8*)(u32)info->fb_phys;
    }

    if (info->width) {
        g_width = info->width;
    }
    if (info->height) {
        g_height = info->height;
    }
    if (info->pitch) {
        g_pitch = info->pitch;
    }

    serial_write("[DBG fb] init fb_phys=");
    serial_write_hex32((u32)g_fb);
    serial_write(" w=");
    serial_write_dec(g_width);
    serial_write(" h=");
    serial_write_dec(g_height);
    serial_write(" pitch=");
    serial_write_dec(g_pitch);
    serial_write_char('\n');
}

volatile u8* framebuffer_base(void) {
    return g_fb;
}

u32 framebuffer_width(void) {
    return g_width;
}

u32 framebuffer_height(void) {
    return g_height;
}

u32 framebuffer_pitch(void) {
    return g_pitch;
}

void framebuffer_clear(u8 color) {
    u32 y;
    for (y = 0; y < g_height; y++) {
        u32 x;
        u32 row = y * g_pitch;
        for (x = 0; x < g_width; x++) {
            g_fb[row + x] = color;
        }
    }
}

void framebuffer_put_pixel(u32 x, u32 y, u8 color) {
    if (x >= g_width || y >= g_height) {
        return;
    }
    g_fb[y * g_pitch + x] = color;
}

void framebuffer_fill_rect(u32 x, u32 y, u32 w, u32 h, u8 color) {
    u32 yy;
    u32 xx;

    for (yy = 0; yy < h; yy++) {
        if (y + yy >= g_height) {
            break;
        }
        for (xx = 0; xx < w; xx++) {
            if (x + xx >= g_width) {
                break;
            }
            framebuffer_put_pixel(x + xx, y + yy, color);
        }
    }
}

u8 framebuffer_get_pixel(u32 x, u32 y) {
    if (x >= g_width || y >= g_height) {
        return 0u;
    }
    return g_fb[y * g_pitch + x];
}

void framebuffer_set_pixel(u32 x, u32 y, u8 color) {
    framebuffer_put_pixel(x, y, color);
}
