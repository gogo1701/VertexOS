/*
 * Dual-mode console renderer: VGA text by default, framebuffer text in graphics mode.
 */

#include "display.h"
#include "framebuffer.h"
#include "io.h"
#include "serial.h"
#include "video.h"

#define TEXT_COLOR 0x0Fu
#define GFX_CELL_W 8u
#define GFX_CELL_H 8u
#define GFX_COLS 40u
#define GFX_ROWS 25u
#define GFX_FG 15u
#define GFX_BG 0u
#define GFX_CURSOR 12u

static volatile u16* const VGA = (u16*)0xB8000;
static u32 cursor_row = 0;
static u32 cursor_col = 0;
static char text_cells[VGA_WIDTH * VGA_HEIGHT];
static u8 g_graphics_test_overlay = 0;

static const u8 GLYPH_SPACE[7] = {0, 0, 0, 0, 0, 0, 0};
static const u8 GLYPH_QMARK[7] = {0x0E, 0x11, 0x01, 0x06, 0x04, 0x00, 0x04};
static const u8 GLYPH_DOT[7] = {0, 0, 0, 0, 0, 0x06, 0x06};
static const u8 GLYPH_COMMA[7] = {0, 0, 0, 0, 0x00, 0x06, 0x04};
static const u8 GLYPH_COLON[7] = {0, 0x06, 0x06, 0, 0x06, 0x06, 0};
static const u8 GLYPH_SEMI[7] = {0, 0x06, 0x06, 0, 0x06, 0x04, 0};
static const u8 GLYPH_EXCL[7] = {0x04, 0x04, 0x04, 0x04, 0x04, 0x00, 0x04};
static const u8 GLYPH_MINUS[7] = {0, 0, 0, 0x1F, 0, 0, 0};
static const u8 GLYPH_PLUS[7] = {0, 0x04, 0x04, 0x1F, 0x04, 0x04, 0};
static const u8 GLYPH_SLASH[7] = {0x01, 0x02, 0x04, 0x08, 0x10, 0, 0};
static const u8 GLYPH_BSLASH[7] = {0x10, 0x08, 0x04, 0x02, 0x01, 0, 0};
static const u8 GLYPH_LPAREN[7] = {0x02, 0x04, 0x08, 0x08, 0x08, 0x04, 0x02};
static const u8 GLYPH_RPAREN[7] = {0x08, 0x04, 0x02, 0x02, 0x02, 0x04, 0x08};
static const u8 GLYPH_LBRACK[7] = {0x0E, 0x08, 0x08, 0x08, 0x08, 0x08, 0x0E};
static const u8 GLYPH_RBRACK[7] = {0x0E, 0x02, 0x02, 0x02, 0x02, 0x02, 0x0E};
static const u8 GLYPH_LT[7] = {0x01, 0x02, 0x04, 0x08, 0x04, 0x02, 0x01};
static const u8 GLYPH_GT[7] = {0x10, 0x08, 0x04, 0x02, 0x04, 0x08, 0x10};
static const u8 GLYPH_EQ[7] = {0, 0x1F, 0, 0x1F, 0, 0, 0};
static const u8 GLYPH_QUOTE[7] = {0x0A, 0x0A, 0x04, 0, 0, 0, 0};
static const u8 GLYPH_APOS[7] = {0x04, 0x04, 0x02, 0, 0, 0, 0};
static const u8 GLYPH_USCORE[7] = {0, 0, 0, 0, 0, 0, 0x1F};
static const u8 GLYPH_0[7] = {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E};
static const u8 GLYPH_1[7] = {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E};
static const u8 GLYPH_2[7] = {0x0E, 0x11, 0x01, 0x06, 0x08, 0x10, 0x1F};
static const u8 GLYPH_3[7] = {0x1F, 0x01, 0x02, 0x06, 0x01, 0x11, 0x0E};
static const u8 GLYPH_4[7] = {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02};
static const u8 GLYPH_5[7] = {0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E};
static const u8 GLYPH_6[7] = {0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E};
static const u8 GLYPH_7[7] = {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08};
static const u8 GLYPH_8[7] = {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E};
static const u8 GLYPH_9[7] = {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C};
static const u8 GLYPH_A[7] = {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11};
static const u8 GLYPH_B[7] = {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E};
static const u8 GLYPH_C[7] = {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E};
static const u8 GLYPH_D[7] = {0x1C, 0x12, 0x11, 0x11, 0x11, 0x12, 0x1C};
static const u8 GLYPH_E[7] = {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F};
static const u8 GLYPH_F[7] = {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10};
static const u8 GLYPH_G[7] = {0x0E, 0x11, 0x10, 0x13, 0x11, 0x11, 0x0E};
static const u8 GLYPH_H[7] = {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11};
static const u8 GLYPH_I[7] = {0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E};
static const u8 GLYPH_J[7] = {0x01, 0x01, 0x01, 0x01, 0x11, 0x11, 0x0E};
static const u8 GLYPH_K[7] = {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11};
static const u8 GLYPH_L[7] = {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F};
static const u8 GLYPH_M[7] = {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11};
static const u8 GLYPH_N[7] = {0x11, 0x11, 0x19, 0x15, 0x13, 0x11, 0x11};
static const u8 GLYPH_O[7] = {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E};
static const u8 GLYPH_P[7] = {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10};
static const u8 GLYPH_Q[7] = {0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D};
static const u8 GLYPH_R[7] = {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11};
static const u8 GLYPH_S[7] = {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E};
static const u8 GLYPH_T[7] = {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04};
static const u8 GLYPH_U[7] = {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E};
static const u8 GLYPH_V[7] = {0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04};
static const u8 GLYPH_W[7] = {0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0A};
static const u8 GLYPH_X[7] = {0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11};
static const u8 GLYPH_Y[7] = {0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04};
static const u8 GLYPH_Z[7] = {0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F};

static u32 visible_cols(void) {
    return video_get_mode() == VIDEO_MODE_GRAPHICS ? GFX_COLS : VGA_WIDTH;
}

static u32 visible_rows(void) {
    return video_get_mode() == VIDEO_MODE_GRAPHICS ? GFX_ROWS : VGA_HEIGHT;
}

static char upcase(char c) {
    if (c >= 'a' && c <= 'z') {
        return (char)(c - ('a' - 'A'));
    }
    return c;
}

static const u8* glyph_for(char c) {
    c = upcase(c);
    switch (c) {
        case ' ': return GLYPH_SPACE;
        case '.': return GLYPH_DOT;
        case ',': return GLYPH_COMMA;
        case ':': return GLYPH_COLON;
        case ';': return GLYPH_SEMI;
        case '!': return GLYPH_EXCL;
        case '-': return GLYPH_MINUS;
        case '+': return GLYPH_PLUS;
        case '/': return GLYPH_SLASH;
        case '\\': return GLYPH_BSLASH;
        case '(': return GLYPH_LPAREN;
        case ')': return GLYPH_RPAREN;
        case '[': return GLYPH_LBRACK;
        case ']': return GLYPH_RBRACK;
        case '<': return GLYPH_LT;
        case '>': return GLYPH_GT;
        case '=': return GLYPH_EQ;
        case '"': return GLYPH_QUOTE;
        case '\'': return GLYPH_APOS;
        case '_': return GLYPH_USCORE;
        case '?': return GLYPH_QMARK;
        case '0': return GLYPH_0;
        case '1': return GLYPH_1;
        case '2': return GLYPH_2;
        case '3': return GLYPH_3;
        case '4': return GLYPH_4;
        case '5': return GLYPH_5;
        case '6': return GLYPH_6;
        case '7': return GLYPH_7;
        case '8': return GLYPH_8;
        case '9': return GLYPH_9;
        case 'A': return GLYPH_A;
        case 'B': return GLYPH_B;
        case 'C': return GLYPH_C;
        case 'D': return GLYPH_D;
        case 'E': return GLYPH_E;
        case 'F': return GLYPH_F;
        case 'G': return GLYPH_G;
        case 'H': return GLYPH_H;
        case 'I': return GLYPH_I;
        case 'J': return GLYPH_J;
        case 'K': return GLYPH_K;
        case 'L': return GLYPH_L;
        case 'M': return GLYPH_M;
        case 'N': return GLYPH_N;
        case 'O': return GLYPH_O;
        case 'P': return GLYPH_P;
        case 'Q': return GLYPH_Q;
        case 'R': return GLYPH_R;
        case 'S': return GLYPH_S;
        case 'T': return GLYPH_T;
        case 'U': return GLYPH_U;
        case 'V': return GLYPH_V;
        case 'W': return GLYPH_W;
        case 'X': return GLYPH_X;
        case 'Y': return GLYPH_Y;
        case 'Z': return GLYPH_Z;
        default: return GLYPH_QMARK;
    }
}

static void update_hw_cursor(void) {
    u16 pos = (u16)(cursor_row * VGA_WIDTH + cursor_col);
    io_outb(0x3D4, 0x0E);
    io_outb(0x3D5, (u8)(pos >> 8));
    io_outb(0x3D4, 0x0F);
    io_outb(0x3D5, (u8)(pos & 0xFF));
}

static void text_render_cell(u32 row, u32 col) {
    VGA[row * VGA_WIDTH + col] = (u16)((u8)text_cells[row * VGA_WIDTH + col] | (TEXT_COLOR << 8));
}

static void text_render_full(void) {
    u32 row;
    u32 col;
    for (row = 0; row < VGA_HEIGHT; row++) {
        for (col = 0; col < VGA_WIDTH; col++) {
            text_render_cell(row, col);
        }
    }
    update_hw_cursor();
}

static void gfx_render_cell(u32 row, u32 col, char ch) {
    const u8* glyph = glyph_for(ch);
    u32 px = col * GFX_CELL_W;
    u32 py = row * GFX_CELL_H;
    u32 y;

    framebuffer_fill_rect(px, py, GFX_CELL_W, GFX_CELL_H, GFX_BG);
    for (y = 0; y < 7; y++) {
        u8 bits = glyph[y];
        u32 x;
        for (x = 0; x < 5; x++) {
            if (bits & (1u << (4u - x))) {
                framebuffer_put_pixel(px + 1u + x, py + y, GFX_FG);
            }
        }
    }
}

static void gfx_render_cursor(void) {
    u32 px = cursor_col * GFX_CELL_W;
    u32 py = cursor_row * GFX_CELL_H + (GFX_CELL_H - 1u);
    framebuffer_fill_rect(px + 1u, py, 5u, 1u, GFX_CURSOR);
}

static void gfx_render_test_overlay(void) {
    framebuffer_fill_rect(0, 0, 320, 4, 15);
    framebuffer_fill_rect(0, 196, 320, 4, 15);
    framebuffer_fill_rect(0, 0, 4, 200, 15);
    framebuffer_fill_rect(316, 0, 4, 200, 15);

    framebuffer_fill_rect(256, 12, 16, 16, 12);
    framebuffer_fill_rect(276, 12, 16, 16, 10);
    framebuffer_fill_rect(296, 12, 16, 16, 14);
    framebuffer_fill_rect(256, 32, 56, 12, 15);
}

static void gfx_render_full(void) {
    u32 row;
    u32 col;
    framebuffer_clear(GFX_BG);

    for (row = 0; row < GFX_ROWS; row++) {
        for (col = 0; col < GFX_COLS; col++) {
            gfx_render_cell(row, col, text_cells[row * VGA_WIDTH + col]);
        }
    }
    if (cursor_col >= GFX_COLS) {
        cursor_col = GFX_COLS - 1u;
    }

    if (g_graphics_test_overlay) {
        gfx_render_test_overlay();
    }

    gfx_render_cursor();
}

void display_init(void) {
    u32 i;
    framebuffer_init();
    for (i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        text_cells[i] = ' ';
    }
    cursor_row = 0;
    cursor_col = 0;
    display_refresh();
}

void display_refresh(void) {
    if (cursor_row >= visible_rows()) {
        cursor_row = visible_rows() - 1u;
    }
    if (cursor_col >= visible_cols()) {
        cursor_col = visible_cols() - 1u;
    }

    if (video_get_mode() == VIDEO_MODE_GRAPHICS) {
        gfx_render_full();
    } else {
        text_render_full();
    }
}

void display_clear(void) {
    u32 i;
    for (i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        text_cells[i] = ' ';
    }
    cursor_row = 0;
    cursor_col = 0;
    display_refresh();
}

static void scroll_if_needed(void) {
    u32 row;
    u32 col;
    u32 cols = visible_cols();
    u32 rows = visible_rows();

    if (cursor_row < rows) {
        return;
    }

    for (row = 1; row < rows; row++) {
        for (col = 0; col < cols; col++) {
            text_cells[(row - 1u) * VGA_WIDTH + col] = text_cells[row * VGA_WIDTH + col];
        }
    }

    for (col = 0; col < cols; col++) {
        text_cells[(rows - 1u) * VGA_WIDTH + col] = ' ';
    }

    cursor_row = rows - 1u;
    display_refresh();
}

void display_put_char(char c) {
    if (serial_is_ready()) {
        serial_write_char(c);
    }

    if (c == '\n') {
        cursor_col = 0;
        cursor_row++;
        scroll_if_needed();
        display_refresh();
        return;
    }

    if (c == '\b') {
        if (cursor_col > 0) {
            cursor_col--;
            text_cells[cursor_row * VGA_WIDTH + cursor_col] = ' ';
        }
        display_refresh();
        return;
    }

    text_cells[cursor_row * VGA_WIDTH + cursor_col] = c;
    cursor_col++;

    if (cursor_col >= visible_cols()) {
        cursor_col = 0;
        cursor_row++;
        scroll_if_needed();
    }

    display_refresh();
}

void display_print(const char* s) {
    while (s && *s) {
        display_put_char(*s++);
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
        *p = (digit < 10) ? (char)('0' + digit) : (char)('a' + (digit - 10));
        num /= base;
    }

    display_print(p);
}

void display_set_cursor(u32 row, u32 col) {
    if (row >= visible_rows()) {
        row = visible_rows() - 1u;
    }
    if (col >= visible_cols()) {
        col = visible_cols() - 1u;
    }
    cursor_row = row;
    cursor_col = col;
    display_refresh();
}

void display_get_cursor(u32* row, u32* col) {
    if (row) {
        *row = cursor_row;
    }
    if (col) {
        *col = cursor_col;
    }
}

void display_set_graphics_test_overlay(u8 enabled) {
    g_graphics_test_overlay = enabled ? 1u : 0u;
    display_refresh();
}

u8 display_get_graphics_test_overlay(void) {
    return g_graphics_test_overlay;
}
