/*
 * VGA Display Management Implementation
 */

#include "display.h"
#include "io.h"

/* VGA Memory and Display State */
static volatile u16* const VGA = (u16*)0xB8000;  /* VGA text buffer address */
static const u8 COLOR = 0x0F;                     /* White text on black background */
static u32 cursor_row = 0;                        /* Current cursor row position */
static u32 cursor_col = 0;                        /* Current cursor column position */

/*
 * Update the hardware cursor position
 * 
 * The VGA controller maintains a hardware cursor (the blinking line).
 * This function calculates the linear position and sends it to the VGA
 * controller via I/O ports 0x3D4 (command) and 0x3D5 (data).
 */
static void update_cursor(void) {
    u16 pos = cursor_row * VGA_WIDTH + cursor_col;
    
    /* Send high byte of cursor position */
    io_outb(0x3D4, 0x0E);
    io_outb(0x3D5, (u8)(pos >> 8));
    
    /* Send low byte of cursor position */
    io_outb(0x3D4, 0x0F);
    io_outb(0x3D5, (u8)(pos & 0xFF));
}

/*
 * Scroll the display up one line if cursor has reached the bottom
 */
static void scroll_if_needed(void) {
    u32 row;
    u32 col;

    /* Only scroll if cursor is beyond the bottom of the screen */
    if (cursor_row < VGA_HEIGHT) {
        return;
    }

    /* Move each row up one position */
    for (row = 1; row < VGA_HEIGHT; row++) {
        for (col = 0; col < VGA_WIDTH; col++) {
            VGA[(row - 1) * VGA_WIDTH + col] = VGA[row * VGA_WIDTH + col];
        }
    }

    /* Clear the bottom line with blank spaces */
    for (col = 0; col < VGA_WIDTH; col++) {
        VGA[(VGA_HEIGHT - 1) * VGA_WIDTH + col] = (u16)(' ' | (COLOR << 8));
    }

    /* Position cursor at start of bottom line */
    cursor_row = VGA_HEIGHT - 1;
}

void display_init(void) {
    display_clear();
}

void display_clear(void) {
    u32 i;
    /* Fill entire VGA buffer with blank characters */
    for (i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        VGA[i] = (u16)(' ' | (COLOR << 8));
    }
    /* Reset cursor to top-left */
    cursor_row = 0;
    cursor_col = 0;
    update_cursor();
}

void display_put_char(char c) {
    /* Handle newline: move to next line */
    if (c == '\n') {
        cursor_col = 0;
        cursor_row++;
        scroll_if_needed();
        update_cursor();
        return;
    }

    /* Handle backspace: erase previous character if possible */
    if (c == '\b') {
        if (cursor_col > 0) {
            cursor_col--;
            VGA[cursor_row * VGA_WIDTH + cursor_col] = (u16)(' ' | (COLOR << 8));
            update_cursor();
        }
        return;
    }

    /* Write character to VGA buffer with color attribute */
    VGA[cursor_row * VGA_WIDTH + cursor_col] = (u16)((u8)c | (COLOR << 8));
    cursor_col++;

    /* Wrap to next line if cursor reaches the end of screen width */
    if (cursor_col >= VGA_WIDTH) {
        cursor_col = 0;
        cursor_row++;
        scroll_if_needed();
    }
    
    update_cursor();
}

void display_print(const char* s) {
    while (*s) {
        display_put_char(*s);
        s++;
    }
}

void display_print_num(u32 num, u32 base) {
    char buffer[32];
    char* p = buffer + 31;
    *p = '\0';
    
    if (num == 0) {
        display_put_char('0');
        return;
    }
    
    while (num > 0) {
        u32 digit = num % base;
        p--;
        if (digit < 10) {
            *p = '0' + digit;
        } else {
            *p = 'a' + (digit - 10);
        }
        num /= base;
    }
    
    display_print(p);
}

void display_set_cursor(u32 row, u32 col) {
    if (row >= VGA_HEIGHT) {
        row = VGA_HEIGHT - 1;
    }
    if (col >= VGA_WIDTH) {
        col = VGA_WIDTH - 1;
    }

    cursor_row = row;
    cursor_col = col;
    update_cursor();
}

void display_get_cursor(u32* row, u32* col) {
    *row = cursor_row;
    *col = cursor_col;
}
