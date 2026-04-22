#include "framebuffer.h"
#include "video.h"
#include "serial.h"
#include "io.h"
#include "heap.h"

static volatile u8* g_fb = (u8*)0xA0000;
static u8* g_back_buf = (u8*)0;
static u32 g_width = 320u;
static u32 g_height = 200u;
static u32 g_pitch = 320u;
static u8 g_dirty_valid = 0u;
static u32 g_dirty_min_x = 0u;
static u32 g_dirty_min_y = 0u;
static u32 g_dirty_max_x = 0u;
static u32 g_dirty_max_y = 0u;

static void framebuffer_expand_dirty_rect(u32 x1, u32 y1, u32 x2, u32 y2) {
    if (x1 >= x2 || y1 >= y2) {
        return;
    }

    if (!g_dirty_valid) {
        g_dirty_min_x = x1;
        g_dirty_min_y = y1;
        g_dirty_max_x = x2;
        g_dirty_max_y = y2;
        g_dirty_valid = 1u;
        return;
    }

    if (x1 < g_dirty_min_x) g_dirty_min_x = x1;
    if (y1 < g_dirty_min_y) g_dirty_min_y = y1;
    if (x2 > g_dirty_max_x) g_dirty_max_x = x2;
    if (y2 > g_dirty_max_y) g_dirty_max_y = y2;
}

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

    /* Allocate back buffer for double buffering */
    g_back_buf = (u8*)kmalloc(g_height * g_pitch);
    if (g_back_buf) {
        u32 i;
        for (i = 0; i < g_height * g_pitch; i++) {
            g_back_buf[i] = 0u;
        }
    }
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
    u8* dst = g_back_buf ? g_back_buf : (u8*)g_fb;
    for (y = 0; y < g_height; y++) {
        u32 x;
        u32 row = y * g_pitch;
        for (x = 0; x < g_width; x++) {
            dst[row + x] = color;
        }
    }
    framebuffer_expand_dirty_rect(0u, 0u, g_width, g_height);
}

void framebuffer_put_pixel(u32 x, u32 y, u8 color) {
    if (x >= g_width || y >= g_height) {
        return;
    }
    if (g_back_buf) {
        g_back_buf[y * g_pitch + x] = color;
    } else {
        g_fb[y * g_pitch + x] = color;
    }
    framebuffer_expand_dirty_rect(x, y, x + 1u, y + 1u);
}

void framebuffer_fill_rect(u32 x, u32 y, u32 w, u32 h, u8 color) {
    u32 yy;
    u32 x2;
    u32 y2;
    u8* dst = g_back_buf ? g_back_buf : (u8*)g_fb;

    if (x >= g_width || y >= g_height || w == 0u || h == 0u) {
        return;
    }

    x2 = x + w;
    y2 = y + h;
    if (x2 > g_width) {
        x2 = g_width;
    }
    if (y2 > g_height) {
        y2 = g_height;
    }

    for (yy = y; yy < y2; yy++) {
        u32 xx;
        u32 row = yy * g_pitch;
        for (xx = x; xx < x2; xx++) {
            dst[row + xx] = color;
        }
    }

    framebuffer_expand_dirty_rect(x, y, x2, y2);
}

void framebuffer_mark_dirty_rect(u32 x, u32 y, u32 w, u32 h) {
    u32 x2;
    u32 y2;

    if (x >= g_width || y >= g_height || w == 0u || h == 0u) {
        return;
    }

    x2 = x + w;
    y2 = y + h;
    if (x2 > g_width) {
        x2 = g_width;
    }
    if (y2 > g_height) {
        y2 = g_height;
    }

    framebuffer_expand_dirty_rect(x, y, x2, y2);
}

u8 framebuffer_get_pixel(u32 x, u32 y) {
    if (x >= g_width || y >= g_height) {
        return 0u;
    }
    if (g_back_buf) {
        return g_back_buf[y * g_pitch + x];
    }
    return g_fb[y * g_pitch + x];
}

void framebuffer_set_pixel(u32 x, u32 y, u8 color) {
    framebuffer_put_pixel(x, y, color);
}

void framebuffer_flush(void) {
    if (!g_back_buf) {
        return;
    }
    if (!g_dirty_valid) {
        return;
    }
    framebuffer_wait_vsync();
    {
        u32 y;
        u32 copy_w = g_dirty_max_x - g_dirty_min_x;
        volatile u8* dst = g_fb;
        const u8* src = g_back_buf;
        for (y = g_dirty_min_y; y < g_dirty_max_y; y++) {
            u32 x;
            u32 row = y * g_pitch;
            u32 start = row + g_dirty_min_x;
            for (x = 0; x < copy_w; x++) {
                dst[start + x] = src[start + x];
            }
        }
    }
    g_dirty_valid = 0u;
}

void framebuffer_wait_vsync(void) {
    /* Wait for vertical blank by polling VGA Input Status Register (port 0x3DA) */
    /* Bit 3 indicates vertical retrace (vblank) */
    u8 status;
    
    /* Wait until NOT in vblank */
    while (1) {
        status = io_inb(0x3DAu);
        if ((status & 0x08u) == 0u) {
            break;
        }
    }
    
    /* Wait until IN vblank */
    while (1) {
        status = io_inb(0x3DAu);
        if ((status & 0x08u) != 0u) {
            break;
        }
    }
}
