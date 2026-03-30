/*
 * Linear Framebuffer (Mode 13h, 320x200 @ 8bpp)
 *
 * Provides pixel-level access to VGA mode 13h, where each byte at
 * 0xA0000 is one 8-bit palette index covering a 320x200 grid.
 *
 * Palette (standard VGA 256-colour):
 *   Index 0  = black
 *   Index 1  = blue
 *   Index 2  = green
 *   Index 15 = white
 *   ...  (full 256-colour VGA palette; colours 0-15 match the standard
 *         CGA/EGA/BIOS palette)
 *
 * This module is only active when the kernel was booted into graphics
 * mode.  In text mode the framebuffer base at 0xA0000 is unused.
 */

#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include "types.h"

/*
 * framebuffer_init - Prepare the framebuffer module.
 *
 * Reads current mode metadata and stores runtime framebuffer geometry.
 */
void framebuffer_init(void);

volatile u8* framebuffer_base(void);
u32 framebuffer_width(void);
u32 framebuffer_height(void);
u32 framebuffer_pitch(void);

/*
 * framebuffer_clear - Fill the entire screen with a single colour.
 *
 * @color: 8-bit VGA palette index.
 */
void framebuffer_clear(u8 color);

/*
 * framebuffer_put_pixel - Set a single pixel.
 *
 * Coordinates outside the current framebuffer bounds are silently
 * ignored.
 *
 * @x:     Horizontal position (0 = left).
 * @y:     Vertical position   (0 = top).
 * @color: 8-bit VGA palette index.
 */
void framebuffer_put_pixel(u32 x, u32 y, u8 color);

/*
 * framebuffer_fill_rect - Fill a rectangular region with a colour.
 *
 * The rectangle is clipped to the screen boundaries.
 *
 * @x:     Left edge (inclusive).
 * @y:     Top edge  (inclusive).
 * @w:     Width in pixels.
 * @h:     Height in pixels.
 * @color: 8-bit VGA palette index.
 */
void framebuffer_fill_rect(u32 x, u32 y, u32 w, u32 h, u8 color);

u8 framebuffer_get_pixel(u32 x, u32 y);
void framebuffer_set_pixel(u32 x, u32 y, u8 color);

#endif /* FRAMEBUFFER_H */
