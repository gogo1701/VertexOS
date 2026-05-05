/*
 * VGA Display Management
 * 
 * Handles all screen rendering, cursor positioning, and text scrolling
 */

#ifndef DISPLAY_H
#define DISPLAY_H

#include "types.h"

/* VGA Display Constants */
#define VGA_WIDTH 80           /* Characters per row */
#define VGA_HEIGHT 25          /* Total rows of text */

/*
 * Initialize the display
 * 
 * Clears the screen and positions the cursor at the top
 */
void display_init(void);

/*
 * Clear the entire screen and reset cursor
 */
void display_clear(void);
void display_refresh(void);
void display_set_graphics_test_overlay(u8 enabled);
u8 display_get_graphics_test_overlay(void);
void display_set_button_pressed(u8 pressed);
void display_handle_mouse_event(s32 x, s32 y, u8 buttons);
void display_set_gfx_colors(u8 fg, u8 bg, u8 suppress_test_overlay);
void display_begin_update(void);
void display_end_update(void);

/*
 * Display a single character, handling special characters
 * 
 * @c: Character to display (\n for newline, \b for backspace)
 */
void display_put_char(char c);

/*
 * Print a null-terminated string
 * 
 * @s: String to print
 */
void display_print(const char* s);

/*
 * Print an integer value
 * 
 * @num: Number to print
 * @base: Base for conversion (10 for decimal, 16 for hex, etc.)
 */
void display_print_num(u32 num, u32 base);

/* Set cursor position explicitly (clamped to screen bounds). */
void display_set_cursor(u32 row, u32 col);

/*
 * Get current cursor position
 * 
 * @row: Output parameter for row position
 * @col: Output parameter for column position
 */
void display_get_cursor(u32* row, u32* col);

#endif /* DISPLAY_H */
