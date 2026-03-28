#include "framebuffer.h"

static volatile u8* const FB = (u8*)0xA0000;

void framebuffer_init(void) {
    (void)0;
}

void framebuffer_clear(u8 color) {
    u32 i;
    for (i = 0; i < FB_WIDTH * FB_HEIGHT; i++) {
        FB[i] = color;
    }
}

void framebuffer_put_pixel(u32 x, u32 y, u8 color) {
    if (x >= FB_WIDTH || y >= FB_HEIGHT) {
        return;
    }
    FB[y * FB_WIDTH + x] = color;
}

void framebuffer_fill_rect(u32 x, u32 y, u32 w, u32 h, u8 color) {
    u32 yy;
    u32 xx;

    for (yy = 0; yy < h; yy++) {
        if (y + yy >= FB_HEIGHT) {
            break;
        }
        for (xx = 0; xx < w; xx++) {
            if (x + xx >= FB_WIDTH) {
                break;
            }
            framebuffer_put_pixel(x + xx, y + yy, color);
        }
    }
}
