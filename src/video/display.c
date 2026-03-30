/*
 * Dual-mode console renderer: VGA text by default, framebuffer text in graphics mode.
 */

#include "display.h"
#include "framebuffer.h"
#include "io.h"
#include "mouse.h"
#include "serial.h"
#include "video.h"

#define TEXT_COLOR 0x0Fu
#define GFX_CELL_W 8u
#define GFX_CELL_H 8u
#define GFX_COLS 40u   /* legacy: used only for 320x200 default */
#define GFX_ROWS 25u   /* legacy: used only for 320x200 default */
#define GFX_MAX_COLS VGA_WIDTH   /* max cols: limited by buffer stride */
#define GFX_MAX_ROWS 75u         /* max rows: covers 800x600 @ 8px cell */
#define GFX_FG 15u
#define GFX_BG 0u
#define GFX_CURSOR 12u
#define GFX_BUTTON_X 32u
#define GFX_BUTTON_Y 48u
#define GFX_BUTTON_W 80u
#define GFX_BUTTON_H 16u
#define GFX_TASKBAR_HEIGHT 18u
#define GFX_WINDOW_TITLE_H 10u
#define GFX_WINDOW_BORDER 1u
#define GFX_WINDOW_MIN_W 72u
#define GFX_WINDOW_MIN_H 48u
#define GFX_RESIZE_HANDLE 8u
#define GFX_DESKTOP_BG 1u

#define WIN_DESKTOP 0
#define WIN_TERMINAL 1
#define WIN_SETTINGS 2
#define WIN_COUNT 3

#define START_BTN_X 4u
#define START_BTN_W 48u
#define START_MENU_W 128u
#define START_MENU_ITEM_H 14u
#define START_MENU_ITEMS 3u

#define THEME_COUNT 3u
#define SETTINGS_SWATCH_W 18u
#define SETTINGS_SWATCH_H 12u

typedef struct {
    s32 x;
    s32 y;
    u32 w;
    u32 h;
    u8 title_bg;
    u8 body_bg;
    u8 border;
    const char* title;
    u8 visible;
    u8 is_desktop;
    u8 is_terminal;
    u8 is_settings;
} gfx_window;

static volatile u16* const VGA = (u16*)0xB8000;
static u32 cursor_row = 0;
static u32 cursor_col = 0;
static char text_cells[GFX_MAX_COLS * GFX_MAX_ROWS];
static u8 g_graphics_test_overlay = 0;
static u8 g_gfx_fg = GFX_FG;
static u8 g_gfx_bg = GFX_BG;
static u8 g_gfx_disable_overlay = 0;
static u8 g_button_pressed = 0;
static gfx_window g_windows[WIN_COUNT];
static s32 g_drag_window = -1;
static s32 g_drag_offset_x = 0;
static s32 g_drag_offset_y = 0;
static s32 g_resize_window = -1;
static s32 g_resize_offset_x = 0;
static s32 g_resize_offset_y = 0;
static u8 g_start_menu_open = 0;
static u8 g_active_theme = 0;
static u8 g_taskbar_bg = 8u;
static u8 g_taskbar_text = 0u;

static gfx_window* terminal_window(void);
static gfx_window* settings_window(void);

typedef struct {
    u8 desktop_bg;
    u8 taskbar_bg;
    u8 term_title;
    u8 term_body;
    u8 settings_title;
    u8 settings_body;
    u8 text_fg;
    u8 text_bg;
} ui_theme;

static const ui_theme g_themes[THEME_COUNT] = {
    {1u, 8u, 2u, 0u, 5u, 7u, 15u, 0u},
    {3u, 1u, 4u, 1u, 6u, 3u, 15u, 1u},
    {0u, 7u, 9u, 0u, 2u, 8u, 14u, 0u}
};

static void apply_theme(u8 theme_id) {
    const ui_theme* t;
    gfx_window* tw;
    gfx_window* sw;
    if (theme_id >= THEME_COUNT) {
        theme_id = 0u;
    }
    g_active_theme = theme_id;
    t = &g_themes[theme_id];

    g_windows[WIN_DESKTOP].body_bg = t->desktop_bg;
    tw = terminal_window();
    if (tw) {
        tw->title_bg = t->term_title;
        tw->body_bg = t->term_body;
    }

    sw = 0;
    {
        s32 i;
        for (i = 0; i < WIN_COUNT; i++) {
            if (g_windows[i].is_settings) {
                sw = &g_windows[i];
                break;
            }
        }
    }
    if (sw) {
        sw->title_bg = t->settings_title;
        sw->body_bg = t->settings_body;
    }

    g_taskbar_bg = t->taskbar_bg;
    g_taskbar_text = t->desktop_bg;
    g_gfx_fg = t->text_fg;
    g_gfx_bg = t->text_bg;
}

static s32 window_index_from_ptr(gfx_window* w) {
    s32 i;
    for (i = 0; i < WIN_COUNT; i++) {
        if (&g_windows[i] == w) {
            return i;
        }
    }
    return -1;
}

static void bring_window_to_front(s32 index) {
    while (index >= WIN_TERMINAL && index < (WIN_COUNT - 1)) {
        gfx_window tmp = g_windows[index];
        g_windows[index] = g_windows[index + 1];
        g_windows[index + 1] = tmp;
        index++;
    }
}

static s32 start_menu_y(void) {
    return (s32)framebuffer_height() - (s32)GFX_TASKBAR_HEIGHT - (s32)(START_MENU_ITEM_H * START_MENU_ITEMS) - 2;
}

static s32 settings_swatch_x(s32 i) {
    s32 widx;
    for (widx = 0; widx < WIN_COUNT; widx++) {
        if (g_windows[widx].is_settings) {
            return g_windows[widx].x + 10 + i * ((s32)SETTINGS_SWATCH_W + 8);
        }
    }
    return 10 + i * ((s32)SETTINGS_SWATCH_W + 8);
}

static s32 settings_swatch_y(void) {
    s32 widx;
    for (widx = 0; widx < WIN_COUNT; widx++) {
        if (g_windows[widx].is_settings) {
            return g_windows[widx].y + 30;
        }
    }
    return 30;
}

static gfx_window* terminal_window(void) {
    s32 i;
    for (i = 0; i < WIN_COUNT; i++) {
        if (g_windows[i].is_terminal) {
            return &g_windows[i];
        }
    }
    return &g_windows[WIN_TERMINAL];
}

static gfx_window* settings_window(void) {
    s32 i;
    for (i = 0; i < WIN_COUNT; i++) {
        if (g_windows[i].is_settings) {
            return &g_windows[i];
        }
    }
    return 0;
}

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
    if (video_get_mode() == VIDEO_MODE_GRAPHICS) {
        gfx_window* tw = terminal_window();
        u32 inner_w = 0u;
        u32 cols;
        if (tw->w > 2u * GFX_WINDOW_BORDER) {
            inner_w = tw->w - 2u * GFX_WINDOW_BORDER;
        }
        cols = inner_w / GFX_CELL_W;
        if (cols == 0u) {
            cols = 1u;
        }
        return cols > GFX_MAX_COLS ? GFX_MAX_COLS : cols;
    }
    return VGA_WIDTH;
}

static u32 visible_rows(void) {
    if (video_get_mode() == VIDEO_MODE_GRAPHICS) {
        gfx_window* tw = terminal_window();
        u32 inner_h = 0u;
        u32 rows;
        if (tw->h > (GFX_WINDOW_TITLE_H + GFX_WINDOW_BORDER)) {
            inner_h = tw->h - GFX_WINDOW_TITLE_H - GFX_WINDOW_BORDER;
        }
        rows = inner_h / GFX_CELL_H;
        if (rows == 0u) {
            rows = 1u;
        }
        return rows > GFX_MAX_ROWS ? GFX_MAX_ROWS : rows;
    }
    return VGA_HEIGHT;
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

static void gfx_render_cursor(void) {
    gfx_window* tw = terminal_window();
    u32 px = (u32)(tw->x + (s32)GFX_WINDOW_BORDER) + cursor_col * GFX_CELL_W;
    u32 py = (u32)(tw->y + (s32)GFX_WINDOW_TITLE_H) + cursor_row * GFX_CELL_H + (GFX_CELL_H - 1u);
    framebuffer_fill_rect(px + 1u, py, 5u, 1u, GFX_CURSOR);
}

static void gfx_render_label(u32 x_start, u32 y_start, const char* text, u8 color);
static void gfx_render_taskbar(void);
static void gfx_render_window(gfx_window* window);
static void gfx_render_terminal_content(gfx_window* window);
static void gfx_render_settings_content(gfx_window* window);
static void gfx_render_start_menu(void);
static void gfx_render_windows(void);
static void gfx_render_scene_no_cursor(void);

static void gfx_render_label(u32 x_start, u32 y_start, const char* text, u8 color) {
    const char* p = text;
    while (*p) {
        const u8* glyph = glyph_for(*p);
        u32 row;
        for (row = 0; row < 7u; row++) {
            u8 bits = glyph[row];
            u32 col;
            for (col = 0; col < 5u; col++) {
                if (bits & (1u << (4u - col))) {
                    framebuffer_put_pixel(x_start + col, y_start + row, color);
                }
            }
        }
        x_start += 6u;
        p++;
    }
}

static void gfx_render_taskbar(void) {
    u8 bar_color = g_taskbar_bg;
    u32 fb_w = framebuffer_width();
    u32 fb_h = framebuffer_height();

    framebuffer_fill_rect(0, fb_h - GFX_TASKBAR_HEIGHT, fb_w,
                          GFX_TASKBAR_HEIGHT, bar_color);
    framebuffer_fill_rect(0, fb_h - GFX_TASKBAR_HEIGHT, fb_w, 1u, 15u);
    framebuffer_fill_rect(0, fb_h - 1u, fb_w, 1u, 0u);

    /* Start button */
    framebuffer_fill_rect(START_BTN_X, fb_h - GFX_TASKBAR_HEIGHT + 3u, START_BTN_W,
                          GFX_TASKBAR_HEIGHT - 6u, 7u);
    framebuffer_fill_rect(START_BTN_X, fb_h - GFX_TASKBAR_HEIGHT + 3u, START_BTN_W, 1u, 15u);
    framebuffer_fill_rect(START_BTN_X, fb_h - GFX_TASKBAR_HEIGHT + 3u, 1u,
                          GFX_TASKBAR_HEIGHT - 6u, 15u);
    framebuffer_fill_rect(START_BTN_X, fb_h - 3u, START_BTN_W, 1u, 8u);
    framebuffer_fill_rect(START_BTN_X + START_BTN_W - 1u, fb_h - GFX_TASKBAR_HEIGHT + 3u, 1u,
                          GFX_TASKBAR_HEIGHT - 6u, 8u);
    gfx_render_label(12u, fb_h - GFX_TASKBAR_HEIGHT + 6u, "START", g_taskbar_text);

    /* Clock and status on right side */
    gfx_render_label(fb_w - 40u, fb_h - GFX_TASKBAR_HEIGHT + 6u,
                     "12:34", g_taskbar_text);
}

static void gfx_render_start_menu(void) {
    s32 x = (s32)START_BTN_X;
    s32 y = start_menu_y();
    s32 i;
    static const char* items[START_MENU_ITEMS] = {"TERMINAL", "SETTINGS", "ABOUT"};

    if (!g_start_menu_open) {
        return;
    }

    framebuffer_fill_rect((u32)x, (u32)y, START_MENU_W, START_MENU_ITEM_H * START_MENU_ITEMS, 7u);
    framebuffer_fill_rect((u32)x, (u32)y, START_MENU_W, 1u, 15u);
    framebuffer_fill_rect((u32)x, (u32)y, 1u, START_MENU_ITEM_H * START_MENU_ITEMS, 15u);
    framebuffer_fill_rect((u32)x, (u32)(y + (s32)(START_MENU_ITEM_H * START_MENU_ITEMS - 1u)),
                          START_MENU_W, 1u, 8u);
    framebuffer_fill_rect((u32)(x + (s32)START_MENU_W - 1), (u32)y,
                          1u, START_MENU_ITEM_H * START_MENU_ITEMS, 8u);

    for (i = 0; i < (s32)START_MENU_ITEMS; i++) {
        s32 iy = y + i * (s32)START_MENU_ITEM_H;
        if (i != 0) {
            framebuffer_fill_rect((u32)(x + 2), (u32)iy, START_MENU_W - 4u, 1u, 8u);
        }
        gfx_render_label((u32)(x + 8), (u32)(iy + 4), items[i], 0u);
    }
}

static void gfx_render_window(gfx_window* window) {
    if (!window->visible) {
        return;
    }

    if (window->is_desktop) {
        framebuffer_fill_rect(window->x, window->y, window->w, window->h, window->body_bg);
        return;
    }

    framebuffer_fill_rect(window->x, window->y, window->w, window->h,
                          window->body_bg);
    framebuffer_fill_rect(window->x, window->y, window->w,
                          GFX_WINDOW_TITLE_H, window->title_bg);

    framebuffer_fill_rect(window->x, window->y, window->w, GFX_WINDOW_BORDER, 15u);
    framebuffer_fill_rect(window->x, window->y, GFX_WINDOW_BORDER, window->h, 15u);
    framebuffer_fill_rect(window->x, window->y + window->h - GFX_WINDOW_BORDER,
                          window->w, GFX_WINDOW_BORDER, 8u);
    framebuffer_fill_rect(window->x + window->w - GFX_WINDOW_BORDER, window->y,
                          GFX_WINDOW_BORDER, window->h, 8u);

    if (window->w > (GFX_RESIZE_HANDLE + 2u) && window->h > (GFX_RESIZE_HANDLE + 2u)) {
        u32 rx = (u32)(window->x + (s32)window->w - (s32)GFX_RESIZE_HANDLE - 1);
        u32 ry = (u32)(window->y + (s32)window->h - (s32)GFX_RESIZE_HANDLE - 1);
        framebuffer_fill_rect(rx, ry, GFX_RESIZE_HANDLE, GFX_RESIZE_HANDLE, 7u);
        framebuffer_fill_rect(rx, ry, GFX_RESIZE_HANDLE, 1u, 15u);
        framebuffer_fill_rect(rx, ry, 1u, GFX_RESIZE_HANDLE, 15u);
        framebuffer_fill_rect(rx, ry + GFX_RESIZE_HANDLE - 1u, GFX_RESIZE_HANDLE, 1u, 8u);
        framebuffer_fill_rect(rx + GFX_RESIZE_HANDLE - 1u, ry, 1u, GFX_RESIZE_HANDLE, 8u);
    }

    gfx_render_label(window->x + 4u, window->y + 2u, window->title, 0u);

    if (window->is_terminal) {
        gfx_render_terminal_content(window);
    } else if (window->is_settings) {
        gfx_render_settings_content(window);
    }
}

static void gfx_render_settings_content(gfx_window* window) {
    s32 i;
    s32 sy = settings_swatch_y();
    gfx_render_label((u32)(window->x + 10), (u32)(window->y + 15), "THEME", 0u);

    for (i = 0; i < (s32)THEME_COUNT; i++) {
        const ui_theme* t = &g_themes[i];
        s32 sx = settings_swatch_x(i);
        framebuffer_fill_rect((u32)sx, (u32)sy, SETTINGS_SWATCH_W, SETTINGS_SWATCH_H, t->desktop_bg);
        framebuffer_fill_rect((u32)sx, (u32)sy, SETTINGS_SWATCH_W, 1u, (g_active_theme == (u8)i) ? 15u : 8u);
        framebuffer_fill_rect((u32)sx, (u32)sy, 1u, SETTINGS_SWATCH_H, (g_active_theme == (u8)i) ? 15u : 8u);
        framebuffer_fill_rect((u32)sx, (u32)(sy + (s32)SETTINGS_SWATCH_H - 1), SETTINGS_SWATCH_W, 1u, 0u);
        framebuffer_fill_rect((u32)(sx + (s32)SETTINGS_SWATCH_W - 1), (u32)sy, 1u, SETTINGS_SWATCH_H, 0u);
    }

    gfx_render_label((u32)(window->x + 10), (u32)(window->y + 52), "CLICK SWATCH", 0u);
}

static void gfx_render_terminal_content(gfx_window* window) {
    u32 row;
    u32 col;
    u32 cols = visible_cols();
    u32 rows = visible_rows();
    u32 ox = (u32)(window->x + (s32)GFX_WINDOW_BORDER);
    u32 oy = (u32)(window->y + (s32)GFX_WINDOW_TITLE_H);

    for (row = 0; row < rows; row++) {
        for (col = 0; col < cols; col++) {
            const u8* glyph = glyph_for(text_cells[row * VGA_WIDTH + col]);
            u32 px = ox + col * GFX_CELL_W;
            u32 py = oy + row * GFX_CELL_H;
            u32 y;

            framebuffer_fill_rect(px, py, GFX_CELL_W, GFX_CELL_H, g_gfx_bg);
            for (y = 0; y < 7u; y++) {
                u8 bits = glyph[y];
                u32 x;
                for (x = 0; x < 5u; x++) {
                    if (bits & (1u << (4u - x))) {
                        framebuffer_put_pixel(px + 1u + x, py + y, g_gfx_fg);
                    }
                }
            }
        }
    }
}

static void gfx_render_windows(void) {
    s32 i;
    for (i = 0; i < WIN_COUNT; i++) {
        gfx_render_window(&g_windows[i]);
    }
}

/*
 * Repaint only a dirty rectangle without a full-screen clear: fill with
 * background, re-render text cells that overlap it, then re-render the
 * background window if it overlaps.  Used during window drag/resize so the
 * screen never goes fully black between frames.
 */
static void gfx_repair_region(s32 x1, s32 y1, s32 x2, s32 y2) {
    s32 i;
    s32 fb_w = (s32)framebuffer_width();
    s32 fb_h = (s32)framebuffer_height();

    if (x1 < 0) x1 = 0;
    if (y1 < 0) y1 = 0;
    if (x2 > fb_w) x2 = fb_w;
    if (y2 > fb_h) y2 = fb_h;
    if (x2 <= x1 || y2 <= y1) return;

    framebuffer_fill_rect((u32)x1, (u32)y1, (u32)(x2 - x1), (u32)(y2 - y1), g_gfx_bg);

    for (i = 0; i < WIN_COUNT; i++) {
        gfx_window* w = &g_windows[i];
        if (w->visible &&
            w->x < x2 && w->x + (s32)w->w > x1 &&
            w->y < y2 && w->y + (s32)w->h > y1) {
            gfx_render_window(w);
        }
    }
}

static void gfx_render_color_spectrum(void) {
    u32 x;
    u32 y;

    for (x = 0; x < 256u; x++) {
        u8 color = (u8)x;
        for (y = 0; y < 12u; y++) {
            framebuffer_put_pixel(32u + x, 32u + y, color);
        }
    }
}

static void gfx_render_test_overlay(void) {
    framebuffer_clear(15);
    framebuffer_fill_rect(0, framebuffer_height() - 4u, framebuffer_width(), 4u, 0u);
    gfx_render_color_spectrum();
}

static void gfx_render_full(void) {
    if (g_graphics_test_overlay && !g_gfx_disable_overlay) {
        gfx_render_test_overlay();
    } else {
        framebuffer_clear(g_gfx_bg);
    }

    if (cursor_col >= visible_cols()) {
        cursor_col = visible_cols() - 1u;
    }

    gfx_render_windows();
    gfx_render_taskbar();
    gfx_render_start_menu();
    gfx_render_cursor();
}

static void gfx_render_scene_no_cursor(void) {
    if (g_graphics_test_overlay && !g_gfx_disable_overlay) {
        gfx_render_test_overlay();
    } else {
        framebuffer_clear(g_gfx_bg);
    }

    gfx_render_windows();
    gfx_render_taskbar();
    gfx_render_start_menu();
    gfx_render_cursor();
}

/*
 * Load the standard VGA 16-color CGA palette into the DAC.
 * Values use the full 8-bit (0-255) scale so the palette is correct in both
 * 6-bit DAC mode (hardware masks to bits [5:0]: 255->63, 170->42, 85->21)
 * and 8-bit DAC mode (used after VBE mode set: 255->white, 170->2/3, 85->1/3).
 */
static void load_cga_palette(void) {
    static const u8 pal[16][3] = {
        {  0,   0,   0},  /*  0 black          */
        {  0,   0, 170},  /*  1 dark blue       */
        {  0, 170,   0},  /*  2 dark green      */
        {  0, 170, 170},  /*  3 dark cyan       */
        {170,   0,   0},  /*  4 dark red        */
        {170,   0, 170},  /*  5 dark magenta    */
        {170,  85,   0},  /*  6 brown           */
        {170, 170, 170},  /*  7 light gray      */
        { 85,  85,  85},  /*  8 dark gray       */
        { 85,  85, 255},  /*  9 bright blue     */
        { 85, 255,  85},  /* 10 bright green    */
        { 85, 255, 255},  /* 11 bright cyan     */
        {255,  85,  85},  /* 12 bright red      */
        {255,  85, 255},  /* 13 bright magenta  */
        {255, 255,  85},  /* 14 bright yellow   */
        {255, 255, 255},  /* 15 bright white    */
    };
    u32 i;
    u8 r, g, b;

    io_outb(0x3C8, 0);  /* start writing from DAC index 0 */
    for (i = 0; i < 16u; i++) {
        io_outb(0x3C9, pal[i][0]);
        io_outb(0x3C9, pal[i][1]);
        io_outb(0x3C9, pal[i][2]);
    }

    /* Read back indices 0, 12, 15 to confirm writes took effect */
    io_outb(0x3C7, 0);
    r = io_inb(0x3C9); g = io_inb(0x3C9); b = io_inb(0x3C9);
    serial_write("[DBG disp] DAC[0]  R="); serial_write_dec(r);
    serial_write(" G="); serial_write_dec(g);
    serial_write(" B="); serial_write_dec(b); serial_write_char('\n');

    io_outb(0x3C7, 12);
    r = io_inb(0x3C9); g = io_inb(0x3C9); b = io_inb(0x3C9);
    serial_write("[DBG disp] DAC[12] R="); serial_write_dec(r);
    serial_write(" G="); serial_write_dec(g);
    serial_write(" B="); serial_write_dec(b); serial_write_char('\n');

    io_outb(0x3C7, 15);
    r = io_inb(0x3C9); g = io_inb(0x3C9); b = io_inb(0x3C9);
    serial_write("[DBG disp] DAC[15] R="); serial_write_dec(r);
    serial_write(" G="); serial_write_dec(g);
    serial_write(" B="); serial_write_dec(b); serial_write_char('\n');
}

void display_init(void) {
    u32 i;
    serial_write("[DBG disp] display_init mode=");
    serial_write(video_get_mode() == VIDEO_MODE_GRAPHICS ? "graphics" : "text");
    serial_write_char('\n');
    framebuffer_init();

    if (video_get_mode() == VIDEO_MODE_GRAPHICS) {
        u8 overlay_enabled = 0u;
        load_cga_palette();
        if (video_get_boot_overlay_preference(&overlay_enabled) && overlay_enabled) {
            g_graphics_test_overlay = 1u;
        }
    }

    for (i = 0; i < GFX_MAX_COLS * GFX_MAX_ROWS; i++) {
        text_cells[i] = ' ';
    }

    g_windows[WIN_DESKTOP].x = 0;
    g_windows[WIN_DESKTOP].y = 0;
    g_windows[WIN_DESKTOP].w = framebuffer_width();
    g_windows[WIN_DESKTOP].h = framebuffer_height() - GFX_TASKBAR_HEIGHT;
    g_windows[WIN_DESKTOP].title_bg = 1u;
    g_windows[WIN_DESKTOP].body_bg = GFX_DESKTOP_BG;
    g_windows[WIN_DESKTOP].border = 0u;
    g_windows[WIN_DESKTOP].title = "Desktop";
    g_windows[WIN_DESKTOP].visible = 1u;
    g_windows[WIN_DESKTOP].is_desktop = 1u;
    g_windows[WIN_DESKTOP].is_terminal = 0u;
    g_windows[WIN_DESKTOP].is_settings = 0u;

    g_windows[WIN_TERMINAL].x = 56;
    g_windows[WIN_TERMINAL].y = 28;
    g_windows[WIN_TERMINAL].w = framebuffer_width() > 128u ? framebuffer_width() - 128u : 160u;
    g_windows[WIN_TERMINAL].h = framebuffer_height() > 96u + GFX_TASKBAR_HEIGHT
                                ? framebuffer_height() - (96u + GFX_TASKBAR_HEIGHT) : 120u;
    g_windows[WIN_TERMINAL].title_bg = 2u;
    g_windows[WIN_TERMINAL].body_bg = 0u;
    g_windows[WIN_TERMINAL].border = 15u;
    g_windows[WIN_TERMINAL].title = "Terminal";
    g_windows[WIN_TERMINAL].visible = 1u;
    g_windows[WIN_TERMINAL].is_desktop = 0u;
    g_windows[WIN_TERMINAL].is_terminal = 1u;
    g_windows[WIN_TERMINAL].is_settings = 0u;

    g_windows[WIN_SETTINGS].x = 18;
    g_windows[WIN_SETTINGS].y = 26;
    g_windows[WIN_SETTINGS].w = 118u;
    g_windows[WIN_SETTINGS].h = 88u;
    g_windows[WIN_SETTINGS].title_bg = 5u;
    g_windows[WIN_SETTINGS].body_bg = 7u;
    g_windows[WIN_SETTINGS].border = 15u;
    g_windows[WIN_SETTINGS].title = "Settings";
    g_windows[WIN_SETTINGS].visible = 0u;
    g_windows[WIN_SETTINGS].is_desktop = 0u;
    g_windows[WIN_SETTINGS].is_terminal = 0u;
    g_windows[WIN_SETTINGS].is_settings = 1u;

    apply_theme(0u);
    g_start_menu_open = 0u;

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
        mouse_refresh_cursor();
    } else {
        text_render_full();
    }
}

void display_clear(void) {
    u32 i;
    for (i = 0; i < GFX_MAX_COLS * GFX_MAX_ROWS; i++) {
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
    (void)video_set_boot_overlay_preference(enabled ? 1u : 0u);
    g_gfx_disable_overlay = 0;
    display_refresh();
}

void display_set_button_pressed(u8 pressed) {
    u8 normalized = pressed ? 1u : 0u;
    if (g_button_pressed != normalized) {
        g_button_pressed = normalized;
        if (video_get_mode() != VIDEO_MODE_GRAPHICS) {
            display_refresh();
        }
    }
}

void display_handle_mouse_event(s32 x, s32 y, u8 buttons) {
    static u8 prev_buttons = 0;

    if ((buttons & 0x01) && !(prev_buttons & 0x01)) {
        s32 menu_y = start_menu_y();

        /* Start button toggles menu first. */
        if (y >= (s32)framebuffer_height() - (s32)GFX_TASKBAR_HEIGHT + 3 &&
            y < (s32)framebuffer_height() - 2 &&
            x >= (s32)START_BTN_X && x < (s32)(START_BTN_X + START_BTN_W)) {
            g_start_menu_open = g_start_menu_open ? 0u : 1u;
            mouse_hide_cursor();
            gfx_render_scene_no_cursor();
            mouse_refresh_cursor();
            prev_buttons = buttons;
            return;
        }

        /* Menu item selection has priority while menu is open. */
        if (g_start_menu_open) {
            if (x >= (s32)START_BTN_X && x < (s32)(START_BTN_X + START_MENU_W) &&
                y >= menu_y && y < menu_y + (s32)(START_MENU_ITEM_H * START_MENU_ITEMS)) {
                s32 rel = y - menu_y;
                s32 item = rel / (s32)START_MENU_ITEM_H;

                if (item == 0) {
                    s32 ti = window_index_from_ptr(terminal_window());
                    if (ti >= 0) {
                        g_windows[ti].visible = 1u;
                        bring_window_to_front(ti);
                    }
                } else if (item == 1) {
                    gfx_window* sw = settings_window();
                    s32 si = window_index_from_ptr(sw);
                    if (si >= 0) {
                        g_windows[si].visible = 1u;
                        bring_window_to_front(si);
                    }
                }

                g_start_menu_open = 0u;
                mouse_hide_cursor();
                gfx_render_scene_no_cursor();
                mouse_refresh_cursor();
                prev_buttons = buttons;
                return;
            }

            g_start_menu_open = 0u;
            mouse_hide_cursor();
            gfx_render_scene_no_cursor();
            mouse_refresh_cursor();
        }

        {
            s32 i;
            for (i = WIN_COUNT - 1; i >= WIN_TERMINAL; i--) {
                gfx_window* window = &g_windows[i];
                if (!window->visible || window->is_desktop) {
                    continue;
                }
                if (x >= window->x && x < window->x + (s32)window->w &&
                    y >= window->y && y < window->y + (s32)window->h) {
                    if (window->is_settings) {
                        s32 si;
                        s32 sy = settings_swatch_y();
                        for (si = 0; si < (s32)THEME_COUNT; si++) {
                            s32 sx = settings_swatch_x(si);
                            if (x >= sx && x < sx + (s32)SETTINGS_SWATCH_W &&
                                y >= sy && y < sy + (s32)SETTINGS_SWATCH_H) {
                                apply_theme((u8)si);
                                mouse_hide_cursor();
                                gfx_render_scene_no_cursor();
                                mouse_refresh_cursor();
                                prev_buttons = buttons;
                                return;
                            }
                        }
                    }

                    if (i != WIN_COUNT - 1) {
                        bring_window_to_front(i);
                        window = &g_windows[WIN_COUNT - 1];
                    }

                    g_drag_window = -1;
                    g_resize_window = -1;

                    {
                        gfx_window* selected = &g_windows[WIN_COUNT - 1];
                        s32 resize_x = selected->x + (s32)selected->w - (s32)GFX_RESIZE_HANDLE;
                        s32 resize_y = selected->y + (s32)selected->h - (s32)GFX_RESIZE_HANDLE;

                        if (x >= resize_x && y >= resize_y) {
                            g_resize_window = WIN_COUNT - 1;
                            g_resize_offset_x = (selected->x + (s32)selected->w) - x;
                            g_resize_offset_y = (selected->y + (s32)selected->h) - y;
                        } else if (y < selected->y + (s32)GFX_WINDOW_TITLE_H) {
                            g_drag_window = WIN_COUNT - 1;
                            g_drag_offset_x = x - selected->x;
                            g_drag_offset_y = y - selected->y;
                        }
                    }

                    mouse_hide_cursor();
                    gfx_render_scene_no_cursor();
                    break;
                }
            }
        }
    }

    if (g_drag_window >= 0 && (buttons & 0x01)) {
        gfx_window* window = &g_windows[g_drag_window];
        s32 new_x = x - g_drag_offset_x;
        s32 new_y = y - g_drag_offset_y;

        if (new_x < 0) {
            new_x = 0;
        }
        if (new_y < 0) {
            new_y = 0;
        }
        if (new_x + (s32)window->w > (s32)framebuffer_width()) {
            new_x = (s32)framebuffer_width() - (s32)window->w;
        }
        if (new_y + (s32)window->h > (s32)framebuffer_height() - (s32)GFX_TASKBAR_HEIGHT) {
            new_y = (s32)framebuffer_height() - (s32)GFX_TASKBAR_HEIGHT - (s32)window->h;
        }

        if (window->x != new_x || window->y != new_y) {
            s32 old_x = window->x;
            s32 old_y = window->y;
            s32 dx1, dy1, dx2, dy2;
            window->x = new_x;
            window->y = new_y;
            dx1 = old_x < new_x ? old_x : new_x;
            dy1 = old_y < new_y ? old_y : new_y;
            dx2 = (old_x + (s32)window->w) > (new_x + (s32)window->w)
                  ? (old_x + (s32)window->w) : (new_x + (s32)window->w);
            dy2 = (old_y + (s32)window->h) > (new_y + (s32)window->h)
                  ? (old_y + (s32)window->h) : (new_y + (s32)window->h);
            mouse_hide_cursor();
            gfx_repair_region(dx1, dy1, dx2, dy2);
            gfx_render_window(window);
            gfx_render_taskbar();
            gfx_render_start_menu();
            mouse_refresh_cursor();
        }
    }

    if (g_resize_window >= 0 && (buttons & 0x01)) {
        gfx_window* window = &g_windows[g_resize_window];
        s32 max_w = (s32)framebuffer_width() - window->x;
        s32 max_h = (s32)framebuffer_height() - (s32)GFX_TASKBAR_HEIGHT - window->y;
        s32 new_w = x - window->x + g_resize_offset_x;
        s32 new_h = y - window->y + g_resize_offset_y;

        if (new_w < (s32)GFX_WINDOW_MIN_W) {
            new_w = (s32)GFX_WINDOW_MIN_W;
        }
        if (new_h < (s32)GFX_WINDOW_MIN_H) {
            new_h = (s32)GFX_WINDOW_MIN_H;
        }
        if (new_w > max_w) {
            new_w = max_w;
        }
        if (new_h > max_h) {
            new_h = max_h;
        }

        if ((s32)window->w != new_w || (s32)window->h != new_h) {
            s32 old_w = (s32)window->w;
            s32 old_h = (s32)window->h;
            s32 dx2, dy2;
            window->w = (u32)new_w;
            window->h = (u32)new_h;
            dx2 = window->x + (old_w > new_w ? old_w : new_w);
            dy2 = window->y + (old_h > new_h ? old_h : new_h);
            mouse_hide_cursor();
            gfx_repair_region(window->x, window->y, dx2, dy2);
            gfx_render_window(window);
            gfx_render_taskbar();
            gfx_render_start_menu();
            mouse_refresh_cursor();
        }
    }

    if (!(buttons & 0x01) && (prev_buttons & 0x01)) {
        if (g_drag_window >= 0 || g_resize_window >= 0) {
            mouse_hide_cursor();
            gfx_render_scene_no_cursor();
        }
        g_drag_window = -1;
        g_resize_window = -1;
    }

    prev_buttons = buttons;
}

void display_set_gfx_colors(u8 fg, u8 bg, u8 suppress_test_overlay) {
    g_gfx_fg = fg;
    g_gfx_bg = bg;
    g_gfx_disable_overlay = suppress_test_overlay ? 1u : 0u;
}

u8 display_get_graphics_test_overlay(void) {
    return g_graphics_test_overlay;
}
