#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include "types.h"

#define FB_WIDTH 320
#define FB_HEIGHT 200

void framebuffer_init(void);
void framebuffer_clear(u8 color);
void framebuffer_put_pixel(u32 x, u32 y, u8 color);
void framebuffer_fill_rect(u32 x, u32 y, u32 w, u32 h, u8 color);

#endif /* FRAMEBUFFER_H */
