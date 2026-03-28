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

#define FB_WIDTH  320  /* Framebuffer width in pixels  */
#define FB_HEIGHT 200  /* Framebuffer height in pixels */

/*
 * framebuffer_init - Prepare the framebuffer module.
 *
 * Stores the base address (0xA0000) and clears the screen to black.
 * Must be called after entering mode 13h.
 */
void framebuffer_init(void);

/*
 * framebuffer_clear - Fill the entire screen with a single colour.
 *
 * @color: 8-bit VGA palette index.
 */
void framebuffer_clear(u8 color);

/*
 * framebuffer_put_pixel - Set a single pixel.
 *
 * Coordinates outside the FB_WIDTH x FB_HEIGHT bounds are silently
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

#endif /* FRAMEBUFFER_H */
