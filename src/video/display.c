/*
 * Dual-mode console renderer: VGA text by default, framebuffer text in graphics mode.
 */

#include "display.h"
#include "framebuffer.h"
#include "gui.h"
#include "io.h"
#include "mouse.h"
#include "pit.h"
#include "scheduler.h"
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
#define GFX_WINDOW_TITLE_H 14u
#define GFX_WINDOW_BORDER 1u
#define GFX_WINDOW_MIN_W 72u
#define GFX_WINDOW_MIN_H 48u
#define GFX_RESIZE_HANDLE 8u
#define GFX_DESKTOP_BG 1u

#define WIN_DESKTOP 0
#define WIN_TERMINAL_FIRST 1
#define WIN_TERMINAL_SLOTS 3
#define WIN_TERMINAL_LAST (WIN_TERMINAL_FIRST + WIN_TERMINAL_SLOTS - 1)
#define WIN_SETTINGS (WIN_TERMINAL_LAST + 1)
#define WIN_PROCESS_MANAGER (WIN_TERMINAL_LAST + 2)
#define WIN_COUNT (WIN_PROCESS_MANAGER + 1)

#define START_BTN_X 4u
#define START_BTN_W 48u
#define START_MENU_W 128u
#define START_MENU_ITEM_H 14u
#define START_MENU_ITEMS 4u

#define THEME_COUNT 3u
#define SETTINGS_SWATCH_W 18u
#define SETTINGS_SWATCH_H 12u
#define UI_SCALE_HALF_MIN 2u
#define UI_SCALE_HALF_MAX 4u

typedef gui_window gfx_window;

typedef struct {
    u16 magic;
    u8 mode;
    u8 charsize;
} __attribute__((packed)) psf1_header;

typedef struct {
    u32 magic;
    u32 version;
    u32 header_size;
    u32 flags;
    u32 glyph_count;
    u32 bytes_per_glyph;
    u32 height;
    u32 width;
} __attribute__((packed)) psf2_header;

typedef struct {
    const u8* data;
    u32 header_size;
    u32 glyph_count;
    u32 glyph_size;
    u32 glyph_width;
    u32 glyph_height;
    u32 bytes_per_row;
    u8 valid;
} psf_font;

extern const u8 _binary_assets_fonts_terminus_psf_start[];
extern const u8 _binary_assets_fonts_terminus_psf_end[];

static psf_font g_font = {0};

static volatile u16* const VGA = (u16*)0xB8000;
static u32 cursor_row = 0;
static u32 cursor_col = 0;
static char text_cells[GFX_MAX_COLS * GFX_MAX_ROWS];
static u8 text_cells_dirty[GFX_MAX_COLS * GFX_MAX_ROWS];
static u8 terminal_force_full_redraw = 0u;
static u8 g_graphics_test_overlay = 0;
static u8 g_gfx_fg = GFX_FG;
static u8 g_gfx_bg = GFX_BG;
static u8 g_gfx_disable_overlay = 0;
static u8 g_button_pressed = 0;
static gfx_window g_windows[WIN_COUNT];
static u8 g_prev_cursor_valid = 0u;
static u32 g_prev_cursor_row = 0u;
static u32 g_prev_cursor_col = 0u;
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
static u8 g_ui_scale_level = 3u; /* default 1.5x; 0: auto, 2..4 => 1.0x, 1.5x, 2.0x */
static u32 g_process_cpu_ticks[WIN_COUNT];
static s32 g_pm_selected_index = -1;

static gfx_window* terminal_window(void);
static gfx_window* settings_window(void);
static gfx_window* process_manager_window(void);

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

static const u8 g_cga_palette[16][3] = {
    {  0u,   0u,   0u},
    {  0u,   0u, 170u},
    {  0u, 170u,   0u},
    {  0u, 170u, 170u},
    {170u,   0u,   0u},
    {170u,   0u, 170u},
    {170u,  85u,   0u},
    {170u, 170u, 170u},
    { 85u,  85u,  85u},
    { 85u,  85u, 255u},
    { 85u, 255u,  85u},
    { 85u, 255u, 255u},
    {255u,  85u,  85u},
    {255u,  85u, 255u},
    {255u, 255u,  85u},
    {255u, 255u, 255u}
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
    (void)tw;
    {
        s32 i;
        for (i = 0; i < WIN_COUNT; i++) {
            if (g_windows[i].role == GUI_WINDOW_ROLE_TERMINAL) {
                g_windows[i].title_bg = t->term_title;
                g_windows[i].body_bg = t->term_body;
            }
        }
    }

    sw = 0;
    {
        s32 i;
        for (i = 0; i < WIN_COUNT; i++) {
            if (g_windows[i].role == GUI_WINDOW_ROLE_SETTINGS) {
                sw = &g_windows[i];
                break;
            }
        }
    }
    if (sw) {
        sw->title_bg = t->term_title;
        sw->body_bg = t->settings_body;
    }

    {
        gfx_window* pw = process_manager_window();
        if (pw) {
            pw->title_bg = t->term_title;
            pw->body_bg = t->settings_body;
        }
    }

    g_taskbar_bg = t->taskbar_bg;
    g_taskbar_text = t->desktop_bg;
    g_gfx_fg = t->text_fg;
    g_gfx_bg = t->text_bg;
}

static s32 window_index_from_ptr(gfx_window* w) {
    return gui_window_index_from_ptr(g_windows, WIN_COUNT, w);
}

static void bring_window_to_front(s32 index) {
    gui_bring_window_to_front(g_windows, WIN_COUNT, WIN_TERMINAL_FIRST, index);
}

static u32 gfx_ui_scale_half(void);
static u32 gfx_ui_scale(void);
static u32 gfx_scale_px(u32 base);
static u32 gfx_cell_width(void);
static u32 gfx_cell_height(void);
static u32 gfx_taskbar_height(void);
static u32 gfx_window_title_height(void);
static u32 gfx_window_border_width(void);
static u32 gfx_window_min_width(void);
static u32 gfx_window_min_height(void);
static u32 gfx_resize_handle_size(void);
static u32 gfx_start_button_x(void);
static u32 gfx_start_button_width(void);
static u32 gfx_start_menu_width(void);
static u32 gfx_start_menu_item_height(void);
static u32 gfx_settings_swatch_width(void);
static u32 gfx_settings_swatch_height(void);
static u32 gfx_window_role_min_width(gui_window_role role);
static u32 gfx_window_role_min_height(gui_window_role role);
static void gfx_layout_windows(u8 reset_positions);
static void gfx_draw_scaled_glyph(u32 x_start, u32 y_start, const u8* glyph, u8 color, u8 bg_color);
static void settings_scale_slider_rect(const gfx_window* window,
                                       s32* out_x,
                                       s32* out_y,
                                       u32* out_w,
                                       u32* out_h);

static s32 start_menu_y(void) {
    return (s32)framebuffer_height() - (s32)gfx_taskbar_height()
           - (s32)(gfx_start_menu_item_height() * START_MENU_ITEMS)
           - (s32)gfx_scale_px(2u);
}

static s32 settings_swatch_x(s32 i) {
    s32 widx;
    for (widx = 0; widx < WIN_COUNT; widx++) {
        if (g_windows[widx].role == GUI_WINDOW_ROLE_SETTINGS) {
             return g_windows[widx].x + (s32)gfx_scale_px(10u)
                 + i * ((s32)gfx_settings_swatch_width() + (s32)gfx_scale_px(8u));
        }
    }
        return (s32)gfx_scale_px(10u)
            + i * ((s32)gfx_settings_swatch_width() + (s32)gfx_scale_px(8u));
}

static s32 settings_swatch_y(void) {
    s32 widx;
    for (widx = 0; widx < WIN_COUNT; widx++) {
        if (g_windows[widx].role == GUI_WINDOW_ROLE_SETTINGS) {
            return g_windows[widx].y + (s32)gfx_scale_px(30u);
        }
    }
    return (s32)gfx_scale_px(30u);
}

static gfx_window* terminal_window(void) {
    s32 i;
    for (i = WIN_COUNT - 1; i >= WIN_TERMINAL_FIRST; i--) {
        if (g_windows[i].role == GUI_WINDOW_ROLE_TERMINAL && g_windows[i].visible) {
            return &g_windows[i];
        }
    }
    return gui_find_window_by_role(g_windows, WIN_COUNT, GUI_WINDOW_ROLE_TERMINAL, WIN_TERMINAL_FIRST);
}

static gfx_window* settings_window(void) {
    return gui_find_window_by_role(g_windows, WIN_COUNT, GUI_WINDOW_ROLE_SETTINGS, -1);
}

static gfx_window* process_manager_window(void) {
    return gui_find_window_by_role(g_windows, WIN_COUNT, GUI_WINDOW_ROLE_PROCESS_MANAGER, -1);
}

static u32 gfx_ui_scale_half(void);
static u32 gfx_ui_scale(void);
static u32 gfx_scale_px(u32 base);
static u32 gfx_cell_width(void);
static u32 gfx_cell_height(void);
static u32 gfx_taskbar_height(void);
static u32 gfx_window_title_height(void);
static u32 gfx_window_border_width(void);
static u32 gfx_window_min_width(void);
static u32 gfx_window_min_height(void);
static u32 gfx_resize_handle_size(void);
static u32 gfx_start_button_x(void);
static u32 gfx_start_button_width(void);
static u32 gfx_start_menu_width(void);
static u32 gfx_start_menu_item_height(void);
static u32 gfx_settings_swatch_width(void);
static u32 gfx_settings_swatch_height(void);
static void gfx_draw_scaled_glyph(u32 x_start, u32 y_start, const u8* glyph, u8 color, u8 bg_color);

static u32 gfx_ui_scale_half(void) {
    if (g_ui_scale_level >= UI_SCALE_HALF_MIN && g_ui_scale_level <= UI_SCALE_HALF_MAX) {
        return (u32)g_ui_scale_level;
    }
    if (framebuffer_width() >= 1152u && framebuffer_height() >= 864u) {
        return 4u;
    }
    if (framebuffer_width() >= 800u && framebuffer_height() >= 600u) {
        return 3u;
    }
    return 2u;
}

static u32 gfx_ui_scale(void) {
    return gfx_scale_px(1u);
}

static u32 gfx_scale_px(u32 base) {
    return (base * gfx_ui_scale_half()) / 2u;
}

static void settings_scale_slider_rect(const gfx_window* window,
                                       s32* out_x,
                                       s32* out_y,
                                       u32* out_w,
                                       u32* out_h) {
    u32 title_h;
    s32 x;
    s32 y;
    u32 w;
    u32 h;

    if (!window) {
        return;
    }

    title_h = gfx_window_title_height();
    x = window->x + (s32)gfx_scale_px(10u);
    y = window->y + (s32)title_h + (s32)gfx_scale_px(56u);
    w = gfx_scale_px(84u);
    h = gfx_scale_px(8u);

    if (out_x) {
        *out_x = x;
    }
    if (out_y) {
        *out_y = y;
    }
    if (out_w) {
        *out_w = w;
    }
    if (out_h) {
        *out_h = h;
    }
}

static u32 gfx_cell_width(void) {
    u32 width = g_font.valid && g_font.glyph_width ? g_font.glyph_width : GFX_CELL_W;
    return gfx_scale_px(width);
}

static u32 gfx_cell_height(void) {
    u32 height = g_font.valid && g_font.glyph_height ? g_font.glyph_height : GFX_CELL_H;
    return gfx_scale_px(height);
}

static u32 gfx_text_vertical_pad(u32 box_height) {
    u32 font_height = gfx_cell_height();

    if (box_height > font_height) {
        return (box_height - font_height) / 2u;
    }
    return 0u;
}

static u32 gfx_taskbar_height(void) {
    return gfx_scale_px(GFX_TASKBAR_HEIGHT);
}

static u32 gfx_window_title_height(void) {
    return gfx_scale_px(GFX_WINDOW_TITLE_H);
}

static u32 gfx_window_border_width(void) {
    return gfx_scale_px(GFX_WINDOW_BORDER);
}

static u32 gfx_window_min_width(void) {
    return gfx_scale_px(GFX_WINDOW_MIN_W);
}

static u32 gfx_window_min_height(void) {
    return gfx_scale_px(GFX_WINDOW_MIN_H);
}

static u32 gfx_resize_handle_size(void) {
    return gfx_scale_px(GFX_RESIZE_HANDLE);
}

static u32 gfx_start_button_x(void) {
    return gfx_scale_px(START_BTN_X);
}

static u32 gfx_start_button_width(void) {
    return gfx_scale_px(START_BTN_W);
}

static u32 gfx_start_menu_width(void) {
    return gfx_scale_px(START_MENU_W);
}

static u32 gfx_start_menu_item_height(void) {
    return gfx_scale_px(START_MENU_ITEM_H);
}

static u32 gfx_settings_swatch_width(void) {
    return gfx_scale_px(SETTINGS_SWATCH_W);
}

static u32 gfx_settings_swatch_height(void) {
    return gfx_scale_px(SETTINGS_SWATCH_H);
}

static u32 gfx_window_role_min_width(gui_window_role role) {
    if (role == GUI_WINDOW_ROLE_SETTINGS) {
        return gfx_scale_px(118u);
    }
    if (role == GUI_WINDOW_ROLE_PROCESS_MANAGER) {
        return gfx_scale_px(164u);
    }
    if (role == GUI_WINDOW_ROLE_TERMINAL) {
        return gfx_scale_px(160u);
    }
    return gfx_window_min_width();
}

static u32 gfx_window_role_min_height(gui_window_role role) {
    if (role == GUI_WINDOW_ROLE_SETTINGS) {
        return gfx_scale_px(88u);
    }
    if (role == GUI_WINDOW_ROLE_PROCESS_MANAGER) {
        return gfx_scale_px(112u);
    }
    if (role == GUI_WINDOW_ROLE_TERMINAL) {
        return gfx_scale_px(120u);
    }
    return gfx_window_min_height();
}

static void gfx_layout_windows(u8 reset_positions) {
    u32 fb_w = framebuffer_width();
    u32 fb_h = framebuffer_height();
    u32 taskbar_h = gfx_taskbar_height();
    s32 i;

    g_windows[WIN_DESKTOP].x = 0;
    g_windows[WIN_DESKTOP].y = 0;
    g_windows[WIN_DESKTOP].w = fb_w;
    g_windows[WIN_DESKTOP].h = fb_h - taskbar_h;

    for (i = 0; i < WIN_TERMINAL_SLOTS; i++) {
        s32 wi = WIN_TERMINAL_FIRST + i;
        gfx_window* window = &g_windows[wi];
        s32 new_w = (s32)(fb_w > gfx_scale_px(128u) ? fb_w - gfx_scale_px(128u) : gfx_scale_px(160u));
        s32 new_h = (s32)(fb_h > (gfx_scale_px(96u) + taskbar_h)
                          ? fb_h - (gfx_scale_px(96u) + taskbar_h)
                          : gfx_scale_px(120u));

        if (reset_positions) {
            window->x = (s32)gfx_scale_px(56u + (u32)(i * 16));
            window->y = (s32)gfx_scale_px(28u + (u32)(i * 12));
            window->w = (u32)new_w;
            window->h = (u32)new_h;
        } else {
            if ((s32)window->w < new_w) {
                window->w = (u32)new_w;
            }
            if ((s32)window->h < new_h) {
                window->h = (u32)new_h;
            }
        }
        {
            s32 clamped_w = (s32)window->w;
            s32 clamped_h = (s32)window->h;
            s32 clamped_x = window->x;
            s32 clamped_y = window->y;
            gui_clamp_window_size(window,
                                  &clamped_w,
                                  &clamped_h,
                                  gfx_window_role_min_width(window->role),
                                  gfx_window_role_min_height(window->role),
                                  fb_w,
                                  fb_h,
                                  taskbar_h);
            window->w = (u32)clamped_w;
            window->h = (u32)clamped_h;
            gui_clamp_window_position(window,
                                      &clamped_x,
                                      &clamped_y,
                                      fb_w,
                                      fb_h,
                                      taskbar_h);
            window->x = clamped_x;
            window->y = clamped_y;
        }
    }

    {
        gfx_window* window = &g_windows[WIN_SETTINGS];
        if (reset_positions) {
            window->x = (s32)gfx_scale_px(18u);
            window->y = (s32)gfx_scale_px(26u);
            window->w = gfx_scale_px(118u);
            window->h = gfx_scale_px(88u);
        }
        {
            s32 clamped_w = (s32)window->w;
            s32 clamped_h = (s32)window->h;
            s32 clamped_x = window->x;
            s32 clamped_y = window->y;
            gui_clamp_window_size(window,
                                  &clamped_w,
                                  &clamped_h,
                                  gfx_window_role_min_width(window->role),
                                  gfx_window_role_min_height(window->role),
                                  fb_w,
                                  fb_h,
                                  taskbar_h);
            window->w = (u32)clamped_w;
            window->h = (u32)clamped_h;
            gui_clamp_window_position(window,
                                      &clamped_x,
                                      &clamped_y,
                                      fb_w,
                                      fb_h,
                                      taskbar_h);
            window->x = clamped_x;
            window->y = clamped_y;
        }
    }

    {
        gfx_window* window = &g_windows[WIN_PROCESS_MANAGER];
        if (reset_positions) {
            window->x = (s32)gfx_scale_px(34u);
            window->y = (s32)gfx_scale_px(34u);
            window->w = gfx_scale_px(164u);
            window->h = gfx_scale_px(112u);
        }
        {
            s32 clamped_w = (s32)window->w;
            s32 clamped_h = (s32)window->h;
            s32 clamped_x = window->x;
            s32 clamped_y = window->y;
            gui_clamp_window_size(window,
                                  &clamped_w,
                                  &clamped_h,
                                  gfx_window_role_min_width(window->role),
                                  gfx_window_role_min_height(window->role),
                                  fb_w,
                                  fb_h,
                                  taskbar_h);
            window->w = (u32)clamped_w;
            window->h = (u32)clamped_h;
            gui_clamp_window_position(window,
                                      &clamped_x,
                                      &clamped_y,
                                      fb_w,
                                      fb_h,
                                      taskbar_h);
            window->x = clamped_x;
            window->y = clamped_y;
        }
    }
}

static void font_load_from_memory(const u8* start, const u8* end) {
    u32 size;

    g_font.data = 0;
    g_font.header_size = 0u;
    g_font.glyph_count = 256u;
    g_font.glyph_size = 8u;
    g_font.glyph_width = GFX_CELL_W;
    g_font.glyph_height = GFX_CELL_H;
    g_font.bytes_per_row = 1u;
    g_font.valid = 0u;

    if (!start || !end || end <= start) {
        return;
    }

    size = (u32)(end - start);
    if (size < sizeof(psf1_header)) {
        return;
    }

    if (size >= sizeof(psf2_header)) {
        const psf2_header* psf2 = (const psf2_header*)start;

        if (psf2->magic == 0x864ab572u && psf2->version == 0u &&
            psf2->header_size >= sizeof(psf2_header) &&
            psf2->header_size < size && psf2->glyph_count > 0u &&
            psf2->bytes_per_glyph > 0u && psf2->height > 0u && psf2->width > 0u) {
            u64 glyph_bytes = (u64)psf2->glyph_count * (u64)psf2->bytes_per_glyph;
            if ((u64)psf2->header_size + glyph_bytes <= (u64)size) {
                g_font.data = start;
                g_font.header_size = psf2->header_size;
                g_font.glyph_count = psf2->glyph_count;
                g_font.glyph_size = psf2->bytes_per_glyph;
                g_font.glyph_width = psf2->width;
                g_font.glyph_height = psf2->height;
                g_font.bytes_per_row = (psf2->width + 7u) / 8u;
                if (g_font.bytes_per_row == 0u) {
                    g_font.bytes_per_row = 1u;
                }
                g_font.valid = 1u;
                return;
            }
        }
    }

    {
        const psf1_header* psf1 = (const psf1_header*)start;
        u32 glyph_count;

        if (psf1->magic != 0x0436u && psf1->magic != 0x0437u) {
            return;
        }

        glyph_count = (psf1->mode & 0x01u) ? 512u : 256u;
        if ((u32)psf1->charsize == 0u || sizeof(psf1_header) + (u64)glyph_count * psf1->charsize > (u64)size) {
            return;
        }

        g_font.data = start;
        g_font.header_size = sizeof(psf1_header);
        g_font.glyph_count = glyph_count;
        g_font.glyph_size = psf1->charsize;
        g_font.glyph_width = 8u;
        g_font.glyph_height = psf1->charsize;
        g_font.bytes_per_row = 1u;
        g_font.valid = 1u;
    }
}

static const u8* font_glyph_at(u32 codepoint) {
    if (!g_font.valid || !g_font.data || g_font.glyph_size == 0u) {
        return 0;
    }

    if (codepoint >= g_font.glyph_count) {
        codepoint = (u32)'?';
        if (codepoint >= g_font.glyph_count) {
            codepoint = 0u;
        }
    }

    return g_font.data + g_font.header_size + codepoint * g_font.glyph_size;
}

static u8 glyph_sample_covered(const u8* glyph,
                               u32 sample_x_num,
                               u32 sample_x_den,
                               u32 sample_y_num,
                               u32 sample_y_den) {
    u32 sx;
    u32 sy;
    u32 byte_index;
    u8 mask;

    if (!glyph || g_font.glyph_width == 0u || g_font.glyph_height == 0u ||
        sample_x_den == 0u || sample_y_den == 0u) {
        return 0u;
    }

    sx = (sample_x_num * g_font.glyph_width) / sample_x_den;
    sy = (sample_y_num * g_font.glyph_height) / sample_y_den;
    if (sx >= g_font.glyph_width) {
        sx = g_font.glyph_width - 1u;
    }
    if (sy >= g_font.glyph_height) {
        sy = g_font.glyph_height - 1u;
    }

    byte_index = sy * g_font.bytes_per_row + (sx >> 3);
    mask = (u8)(0x80u >> (sx & 7u));
    return (glyph[byte_index] & mask) ? 1u : 0u;
}

static u8 gfx_blend_palette_color(u8 bg, u8 fg, u32 covered, u32 total) {
    u32 r;
    u32 g;
    u32 b;
    u32 i;
    u32 best_i = fg;
    u32 best_dist = 0xFFFFFFFFu;

    if (covered == 0u || bg == fg) {
        return bg;
    }
    if (covered >= total) {
        return fg;
    }

    r = (g_cga_palette[bg & 0x0Fu][0] * (total - covered)
         + g_cga_palette[fg & 0x0Fu][0] * covered
         + (total / 2u)) / total;
    g = (g_cga_palette[bg & 0x0Fu][1] * (total - covered)
         + g_cga_palette[fg & 0x0Fu][1] * covered
         + (total / 2u)) / total;
    b = (g_cga_palette[bg & 0x0Fu][2] * (total - covered)
         + g_cga_palette[fg & 0x0Fu][2] * covered
         + (total / 2u)) / total;

    for (i = 0u; i < 16u; i++) {
        s32 dr = (s32)g_cga_palette[i][0] - (s32)r;
        s32 dg = (s32)g_cga_palette[i][1] - (s32)g;
        s32 db = (s32)g_cga_palette[i][2] - (s32)b;
        u32 dist = (u32)(dr * dr + dg * dg + db * db);
        if (dist < best_dist) {
            best_dist = dist;
            best_i = i;
        }
    }

    return (u8)best_i;
}

static void gfx_draw_scaled_glyph(u32 x_start, u32 y_start, const u8* glyph, u8 color, u8 bg_color) {
    u32 scaled_w = gfx_scale_px(g_font.glyph_width);
    u32 scaled_h = gfx_scale_px(g_font.glyph_height);
    u32 dy;

    if (!glyph || g_font.glyph_width == 0u || g_font.glyph_height == 0u) {
        return;
    }

    if (scaled_w == 0u) {
        scaled_w = 1u;
    }
    if (scaled_h == 0u) {
        scaled_h = 1u;
    }

    for (dy = 0; dy < scaled_h; dy++) {
        u32 dx;

        for (dx = 0; dx < scaled_w; dx++) {
            u32 coverage = 0u;
            u8 out_color;

            coverage += glyph_sample_covered(glyph,
                                             dx * 4u + 1u,
                                             scaled_w * 4u,
                                             dy * 4u + 1u,
                                             scaled_h * 4u);
            coverage += glyph_sample_covered(glyph,
                                             dx * 4u + 3u,
                                             scaled_w * 4u,
                                             dy * 4u + 1u,
                                             scaled_h * 4u);
            coverage += glyph_sample_covered(glyph,
                                             dx * 4u + 1u,
                                             scaled_w * 4u,
                                             dy * 4u + 3u,
                                             scaled_h * 4u);
            coverage += glyph_sample_covered(glyph,
                                             dx * 4u + 3u,
                                             scaled_w * 4u,
                                             dy * 4u + 3u,
                                             scaled_h * 4u);

            out_color = gfx_blend_palette_color(bg_color, color, coverage, 4u);
            if (out_color != bg_color) {
                framebuffer_put_pixel(x_start + dx, y_start + dy, out_color);
            }
        }
    }
}

static u32 visible_cols(void) {
    if (video_get_mode() == VIDEO_MODE_GRAPHICS) {
        gfx_window* tw = terminal_window();
        u32 inner_w = 0u;
        u32 cols;
        u32 border = gfx_window_border_width();
        u32 cell_w = gfx_cell_width();

        if (tw->w > 2u * border) {
            inner_w = tw->w - 2u * border;
        }
        cols = inner_w / cell_w;
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
        u32 title_h = gfx_window_title_height();
        u32 border = gfx_window_border_width();
        u32 cell_h = gfx_cell_height();

        if (tw->h > (title_h + border)) {
            inner_h = tw->h - title_h - border;
        }
        rows = inner_h / cell_h;
        if (rows == 0u) {
            rows = 1u;
        }
        return rows > GFX_MAX_ROWS ? GFX_MAX_ROWS : rows;
    }
    return VGA_HEIGHT;
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
    u32 border = gfx_window_border_width();
    u32 title_h = gfx_window_title_height();
    u32 cell_w = gfx_cell_width();
    u32 cell_h = gfx_cell_height();
    u32 scale = gfx_ui_scale();
    u32 px = (u32)(tw->x + (s32)border) + cursor_col * cell_w;
    u32 py = (u32)(tw->y + (s32)title_h) + cursor_row * cell_h + (cell_h - scale);
    u32 underline_w = cell_w > (2u * scale) ? (cell_w - (2u * scale)) : cell_w;

    framebuffer_fill_rect(px + scale, py, underline_w, scale, GFX_CURSOR);
}

static void gfx_render_label(u32 x_start, u32 y_start, const char* text, u8 color, u8 bg_color);
static void gfx_render_taskbar(void);
static void gfx_render_window(gfx_window* window);
static void gfx_render_terminal_content(gfx_window* window);
static void gfx_render_settings_content(gfx_window* window);
static void gfx_render_process_manager_content(gfx_window* window);
static void gfx_render_start_menu(void);
static void gfx_render_windows(void);
static void gfx_render_scene_no_cursor(void);
static void gfx_render_terminal_cell(gfx_window* window, u32 row, u32 col);
static void gfx_refresh_terminal_incremental(void);
static void clear_visible_text_dirty(void);
static void u32_to_dec_string(u32 value, char* out, u32 out_size);
static s32 find_hidden_terminal_slot(void);
static u32 visible_terminal_count(void);
static void update_process_usage_sample(void);
static u32 build_running_process_list(s32* out_indices, u32 max_indices);
static u8 window_close_button_hit(const gfx_window* window, s32 x, s32 y);
static u8 process_manager_kill_button_hit(const gfx_window* window, s32 x, s32 y);
static u8 process_manager_handle_click(gfx_window* window, s32 x, s32 y);
static void process_manager_kill_selected(void);

static void gfx_render_label(u32 x_start, u32 y_start, const char* text, u8 color, u8 bg_color) {
    const char* p = text;
    u32 advance = gfx_cell_width();
    while (*p) {
        const u8* glyph = font_glyph_at((u8)*p);
        if (glyph) {
            gfx_draw_scaled_glyph(x_start, y_start, glyph, color, bg_color);
        }
        x_start += advance;
        p++;
    }
}

static void u32_to_dec_string(u32 value, char* out, u32 out_size) {
    char tmp[16];
    u32 n = 0;
    u32 i;

    if (!out || out_size == 0u) {
        return;
    }

    if (value == 0u) {
        if (out_size > 1u) {
            out[0] = '0';
            out[1] = '\0';
        } else {
            out[0] = '\0';
        }
        return;
    }

    while (value > 0u && n < sizeof(tmp)) {
        tmp[n++] = (char)('0' + (value % 10u));
        value /= 10u;
    }

    i = 0u;
    while (n > 0u && i + 1u < out_size) {
        out[i++] = tmp[--n];
    }
    out[i] = '\0';
}

static s32 find_hidden_terminal_slot(void) {
    s32 i;
    for (i = WIN_TERMINAL_FIRST; i <= WIN_TERMINAL_LAST; i++) {
        if (g_windows[i].role == GUI_WINDOW_ROLE_TERMINAL && !g_windows[i].visible) {
            return i;
        }
    }
    return -1;
}

static u32 visible_terminal_count(void) {
    u32 count = 0u;
    s32 i;
    for (i = WIN_TERMINAL_FIRST; i <= WIN_TERMINAL_LAST; i++) {
        if (g_windows[i].role == GUI_WINDOW_ROLE_TERMINAL && g_windows[i].visible) {
            count++;
        }
    }
    return count;
}

static void update_process_usage_sample(void) {
    s32 i;
    for (i = WIN_COUNT - 1; i >= WIN_TERMINAL_FIRST; i--) {
        if (!g_windows[i].visible) {
            continue;
        }
        if (g_windows[i].role == GUI_WINDOW_ROLE_PROCESS_MANAGER) {
            continue;
        }
        if (g_windows[i].role == GUI_WINDOW_ROLE_DESKTOP) {
            continue;
        }
        g_process_cpu_ticks[i]++;
        return;
    }
}

static u32 build_running_process_list(s32* out_indices, u32 max_indices) {
    u32 count = 0u;
    s32 i;

    for (i = WIN_TERMINAL_FIRST; i < WIN_COUNT; i++) {
        if (!g_windows[i].visible) {
            continue;
        }
        if (g_windows[i].role == GUI_WINDOW_ROLE_PROCESS_MANAGER) {
            continue;
        }
        if (count < max_indices) {
            out_indices[count] = i;
            count++;
        }
    }

    return count;
}

static u8 window_close_button_hit(const gfx_window* window, s32 x, s32 y) {
    u32 title_h;
    u32 close_w;
    s32 close_x;
    s32 close_y;

    if (!window || window->role == GUI_WINDOW_ROLE_DESKTOP) {
        return 0u;
    }

    title_h = gfx_window_title_height();
    close_w = title_h > gfx_scale_px(4u) ? (title_h - gfx_scale_px(4u)) : title_h;
    close_x = window->x + (s32)window->w - (s32)close_w - (s32)gfx_scale_px(2u);
    close_y = window->y + (s32)gfx_scale_px(2u);

    return (x >= close_x && x < close_x + (s32)close_w &&
            y >= close_y && y < close_y + (s32)close_w) ? 1u : 0u;
}

static u8 process_manager_kill_button_hit(const gfx_window* window, s32 x, s32 y) {
    u32 btn_w;
    u32 btn_h;
    s32 btn_x;
    s32 btn_y;

    if (!window || window->role != GUI_WINDOW_ROLE_PROCESS_MANAGER) {
        return 0u;
    }

    btn_w = gfx_scale_px(46u);
    btn_h = gfx_cell_height();
    btn_x = window->x + (s32)gfx_scale_px(10u);
    btn_y = window->y + (s32)window->h - (s32)btn_h - (s32)gfx_scale_px(8u);

    return (x >= btn_x && x < btn_x + (s32)btn_w &&
            y >= btn_y && y < btn_y + (s32)btn_h) ? 1u : 0u;
}

static void process_manager_kill_selected(void) {
    if (g_pm_selected_index < WIN_TERMINAL_FIRST || g_pm_selected_index >= WIN_COUNT) {
        return;
    }

    if (g_windows[g_pm_selected_index].visible) {
        g_windows[g_pm_selected_index].visible = 0u;
        if (g_drag_window == g_pm_selected_index) {
            g_drag_window = -1;
        }
        if (g_resize_window == g_pm_selected_index) {
            g_resize_window = -1;
        }
    }
    g_pm_selected_index = -1;
}

static u8 process_manager_handle_click(gfx_window* window, s32 x, s32 y) {
    s32 indices[WIN_COUNT];
    u32 count;
    s32 list_x;
    s32 list_y;
    s32 list_w;
    s32 row_h;
    u32 i;

    if (!window || window->role != GUI_WINDOW_ROLE_PROCESS_MANAGER) {
        return 0u;
    }

    if (process_manager_kill_button_hit(window, x, y)) {
        process_manager_kill_selected();
        return 1u;
    }

    count = build_running_process_list(indices, WIN_COUNT);
    list_x = window->x + (s32)gfx_scale_px(8u);
    list_y = window->y + (s32)gfx_scale_px(48u);
    list_w = (s32)window->w - (s32)gfx_scale_px(16u);
    row_h = (s32)gfx_cell_height();

    if (x < list_x || x >= list_x + list_w || y < list_y) {
        return 0u;
    }

    for (i = 0u; i < count; i++) {
        s32 ry = list_y + (s32)i * row_h;
        if (y >= ry && y < ry + row_h) {
            g_pm_selected_index = indices[i];
            return 1u;
        }
    }

    return 0u;
}

static void gfx_render_taskbar(void) {
    u32 fb_w = framebuffer_width();
    u32 fb_h = framebuffer_height();
    u32 taskbar_h = gfx_taskbar_height();
    u32 start_x = gfx_start_button_x();
    u32 start_w = gfx_start_button_width();

    framebuffer_fill_rect(0, fb_h - taskbar_h, fb_w, taskbar_h, g_taskbar_bg);
    framebuffer_fill_rect(0, fb_h - taskbar_h, fb_w, 1u, 15u);
    framebuffer_fill_rect(0, fb_h - 1u, fb_w, 1u, 0u);

    framebuffer_fill_rect(start_x, fb_h - taskbar_h + gfx_scale_px(3u), start_w,
                          taskbar_h - gfx_scale_px(6u), 7u);
    framebuffer_fill_rect(start_x, fb_h - taskbar_h + gfx_scale_px(3u), start_w, 1u, 15u);
    framebuffer_fill_rect(start_x, fb_h - taskbar_h + gfx_scale_px(3u), 1u,
                          taskbar_h - gfx_scale_px(6u), 15u);
    framebuffer_fill_rect(start_x, fb_h - gfx_scale_px(3u), start_w, 1u, 8u);
    framebuffer_fill_rect(start_x + start_w - 1u, fb_h - taskbar_h + gfx_scale_px(3u), 1u,
                          taskbar_h - gfx_scale_px(6u), 8u);
    gfx_render_label(gfx_scale_px(12u), fb_h - taskbar_h + gfx_text_vertical_pad(taskbar_h), "START", g_taskbar_text, g_taskbar_bg);
    gfx_render_label(fb_w - gfx_scale_px(40u), fb_h - taskbar_h + gfx_text_vertical_pad(taskbar_h),
                     "12:34", g_taskbar_text, g_taskbar_bg);
}

static void gfx_render_start_menu(void) {
    s32 x = (s32)gfx_start_button_x();
    s32 y = start_menu_y();
    s32 i;
    static const char* items[START_MENU_ITEMS] = {"TERMINAL", "SETTINGS", "TASKMGR", "ABOUT"};
    u32 menu_w = gfx_start_menu_width();
    u32 item_h = gfx_start_menu_item_height();

    if (!g_start_menu_open) {
        return;
    }

    framebuffer_fill_rect((u32)x, (u32)y, menu_w, item_h * START_MENU_ITEMS, 7u);
    framebuffer_fill_rect((u32)x, (u32)y, menu_w, 1u, 15u);
    framebuffer_fill_rect((u32)x, (u32)y, 1u, item_h * START_MENU_ITEMS, 15u);
    framebuffer_fill_rect((u32)(x + (s32)(item_h * START_MENU_ITEMS - 1u)), (u32)y,
                          menu_w, 1u, 8u);
    framebuffer_fill_rect((u32)(x + (s32)menu_w - 1), (u32)y,
                          1u, item_h * START_MENU_ITEMS, 8u);

    for (i = 0; i < (s32)START_MENU_ITEMS; i++) {
        s32 iy = y + i * (s32)item_h;
        if (i != 0) {
            framebuffer_fill_rect((u32)(x + (s32)gfx_scale_px(2u)), (u32)iy,
                                  menu_w - gfx_scale_px(4u), 1u, 8u);
        }
        gfx_render_label((u32)(x + (s32)gfx_scale_px(8u)), (u32)(iy + (s32)gfx_text_vertical_pad(item_h)), items[i], 0u, 7u);
    }
}

static void gfx_render_window(gfx_window* window) {
    if (!window->visible) {
        return;
    }

    {
        u32 border = gfx_window_border_width();
        u32 title_h = gfx_window_title_height();
        u32 resize = gfx_resize_handle_size();
        u32 line_w = gfx_ui_scale();
        u32 inset = gfx_ui_scale();
        u32 close_w = title_h > gfx_scale_px(4u) ? (title_h - gfx_scale_px(4u)) : title_h;
        u32 close_x = (u32)(window->x + (s32)window->w - (s32)close_w - (s32)gfx_scale_px(2u));
        u32 close_y = (u32)(window->y + (s32)gfx_scale_px(2u));
        u32 i;

        if (window->role == GUI_WINDOW_ROLE_DESKTOP) {
            framebuffer_fill_rect(window->x, window->y, window->w, window->h, window->body_bg);
            return;
        }

        framebuffer_fill_rect(window->x, window->y, window->w, window->h,
                              window->body_bg);
        framebuffer_fill_rect(window->x, window->y, window->w,
                              title_h, window->title_bg);

        framebuffer_fill_rect(window->x, window->y, window->w, border, 15u);
        framebuffer_fill_rect(window->x, window->y, border, window->h, 15u);
        framebuffer_fill_rect(window->x, window->y + window->h - border,
                              window->w, border, 8u);
        framebuffer_fill_rect(window->x + window->w - border, window->y,
                              border, window->h, 8u);

        if (window->w > (resize + 2u) && window->h > (resize + 2u)) {
            u32 rx = (u32)(window->x + (s32)window->w - (s32)resize - 1);
            u32 ry = (u32)(window->y + (s32)window->h - (s32)resize - 1);
            framebuffer_fill_rect(rx, ry, resize, resize, 7u);
            framebuffer_fill_rect(rx, ry, resize, 1u, 15u);
            framebuffer_fill_rect(rx, ry, 1u, resize, 15u);
            framebuffer_fill_rect(rx, ry + resize - 1u, resize, 1u, 8u);
            framebuffer_fill_rect(rx + resize - 1u, ry, 1u, resize, 8u);
        }

        gfx_render_label(window->x + gfx_scale_px(4u), window->y + gfx_text_vertical_pad(title_h), window->title, 0u, window->title_bg);

        framebuffer_fill_rect(close_x, close_y, close_w, close_w, 12u);
        framebuffer_fill_rect(close_x, close_y, close_w, 1u, 15u);
        framebuffer_fill_rect(close_x, close_y, 1u, close_w, 15u);
        framebuffer_fill_rect(close_x, close_y + close_w - 1u, close_w, 1u, 8u);
        framebuffer_fill_rect(close_x + close_w - 1u, close_y, 1u, close_w, 8u);
        if (close_w > (2u * inset + 1u)) {
            for (i = 0u; i + (2u * inset) < close_w; i++) {
                framebuffer_fill_rect(close_x + inset + i, close_y + inset + i, line_w, line_w, 0u);
                framebuffer_fill_rect(close_x + close_w - inset - 1u - i,
                                      close_y + inset + i,
                                      line_w,
                                      line_w,
                                      0u);
            }
        }

        if (window->role == GUI_WINDOW_ROLE_TERMINAL) {
            gfx_render_terminal_content(window);
        } else if (window->role == GUI_WINDOW_ROLE_SETTINGS) {
            gfx_render_settings_content(window);
        } else if (window->role == GUI_WINDOW_ROLE_PROCESS_MANAGER) {
            gfx_render_process_manager_content(window);
        }
    }
}

static void gfx_render_settings_content(gfx_window* window) {
    s32 i;
    s32 sy = settings_swatch_y();
    s32 slider_x;
    s32 slider_y;
    u32 slider_w;
    u32 slider_h;
    u32 knob_size;
    u32 knob_x;
    u32 knob_y;
    u32 scale_value;
    const char* scale_text;
    u32 scale = gfx_ui_scale();
    u32 swatch_w = gfx_settings_swatch_width();
    u32 swatch_h = gfx_settings_swatch_height();
    u32 title_h = gfx_window_title_height();

    gfx_render_label((u32)(window->x + (s32)gfx_scale_px(10u)), (u32)(window->y + (s32)title_h + (s32)gfx_scale_px(4u)), "THEME", 0u, window->body_bg);

    for (i = 0; i < (s32)THEME_COUNT; i++) {
        const ui_theme* t = &g_themes[i];
        s32 sx = settings_swatch_x(i);
        framebuffer_fill_rect((u32)sx, (u32)sy, swatch_w, swatch_h, t->desktop_bg);
        framebuffer_fill_rect((u32)sx, (u32)sy, swatch_w, 1u, (g_active_theme == (u8)i) ? 15u : 8u);
        framebuffer_fill_rect((u32)sx, (u32)sy, 1u, swatch_h, (g_active_theme == (u8)i) ? 15u : 8u);
        framebuffer_fill_rect((u32)sx, (u32)(sy + (s32)swatch_h - 1), swatch_w, 1u, 0u);
        framebuffer_fill_rect((u32)(sx + (s32)swatch_w - 1), (u32)sy, 1u, swatch_h, 0u);
    }

    gfx_render_label((u32)(window->x + (s32)gfx_scale_px(10u)), (u32)(window->y + (s32)title_h + (s32)gfx_scale_px(36u)), "CLICK SWATCH", 0u, window->body_bg);

    settings_scale_slider_rect(window, &slider_x, &slider_y, &slider_w, &slider_h);
    framebuffer_fill_rect((u32)slider_x, (u32)slider_y, slider_w, slider_h, 8u);
    framebuffer_fill_rect((u32)slider_x, (u32)slider_y, slider_w, 1u, 15u);
    framebuffer_fill_rect((u32)slider_x, (u32)slider_y, 1u, slider_h, 15u);
    framebuffer_fill_rect((u32)slider_x, (u32)(slider_y + (s32)slider_h - 1), slider_w, 1u, 0u);
    framebuffer_fill_rect((u32)(slider_x + (s32)slider_w - 1), (u32)slider_y, 1u, slider_h, 0u);

    scale_value = gfx_ui_scale_half();
    if (scale_value < UI_SCALE_HALF_MIN) {
        scale_value = UI_SCALE_HALF_MIN;
    }
    if (scale_value > UI_SCALE_HALF_MAX) {
        scale_value = UI_SCALE_HALF_MAX;
    }

    knob_size = slider_h + gfx_scale_px(2u);
    if (UI_SCALE_HALF_MAX > UI_SCALE_HALF_MIN) {
        knob_x = (u32)slider_x
                 + (((scale_value - UI_SCALE_HALF_MIN)
                     * (slider_w - knob_size))
                    / (UI_SCALE_HALF_MAX - UI_SCALE_HALF_MIN));
    } else {
        knob_x = (u32)slider_x;
    }
    knob_y = (u32)slider_y - scale;
    framebuffer_fill_rect(knob_x, knob_y, knob_size, knob_size, 7u);
    framebuffer_fill_rect(knob_x, knob_y, knob_size, 1u, 15u);
    framebuffer_fill_rect(knob_x, knob_y, 1u, knob_size, 15u);
    framebuffer_fill_rect(knob_x, knob_y + knob_size - 1u, knob_size, 1u, 8u);
    framebuffer_fill_rect(knob_x + knob_size - 1u, knob_y, 1u, knob_size, 8u);

    if (scale_value <= 2u) {
        scale_text = "SCALE: 1X";
    } else if (scale_value == 3u) {
        scale_text = "SCALE: 1.5X";
    } else {
        scale_text = "SCALE: 2X";
    }

    gfx_render_label((u32)(window->x + (s32)gfx_scale_px(10u)), (u32)(slider_y + (s32)slider_h + (s32)gfx_scale_px(6u)), scale_text, 0u, window->body_bg);
}

static void gfx_render_process_manager_content(gfx_window* window) {
    s32 indices[WIN_COUNT];
    u32 count;
    u32 i;
    char line[56];
    char num_buf[16];
    u32 y = (u32)(window->y + (s32)gfx_window_title_height() + (s32)gfx_scale_px(2u));
    u32 total_ticks = 0u;
    u32 btn_w = gfx_scale_px(46u);
    u32 btn_h = gfx_cell_height();
    u32 btn_x = (u32)(window->x + (s32)gfx_scale_px(10u));
    u32 btn_y = (u32)(window->y + (s32)window->h - (s32)btn_h - (s32)gfx_scale_px(8u));
    u32 line_h = gfx_cell_height();

    count = build_running_process_list(indices, WIN_COUNT);
    for (i = 0u; i < count; i++) {
        total_ticks += g_process_cpu_ticks[indices[i]];
    }

    if (g_pm_selected_index >= WIN_TERMINAL_FIRST) {
        u8 still_visible = 0u;
        for (i = 0u; i < count; i++) {
            if (indices[i] == g_pm_selected_index) {
                still_visible = 1u;
                break;
            }
        }
        if (!still_visible) {
            g_pm_selected_index = -1;
        }
    }

    gfx_render_label((u32)(window->x + (s32)gfx_scale_px(10u)), y, "PROCESS MANAGER", 0u, window->body_bg);
    y += line_h;

    u32_to_dec_string(count, num_buf, sizeof(num_buf));
    line[0] = '\0';
    {
        const char* prefix = "RUNNING: ";
        u32 p = 0u;
        while (prefix[p] && p + 1u < sizeof(line)) {
            line[p] = prefix[p];
            p++;
        }
        {
            u32 n = 0u;
            while (num_buf[n] && p + 1u < sizeof(line)) {
                line[p++] = num_buf[n++];
            }
        }
        line[p] = '\0';
    }
    gfx_render_label((u32)(window->x + (s32)gfx_scale_px(10u)), y, line, 0u, window->body_bg);
    y += line_h;

    gfx_render_label((u32)(window->x + (s32)gfx_scale_px(10u)), y, "NAME        CPU%", 0u, window->body_bg);
    y += line_h;

    for (i = 0u; i < count; i++) {
        s32 idx = indices[i];
        u32 usage = 0u;
        u32 row_h = line_h;
        u32 row_x = (u32)(window->x + (s32)gfx_scale_px(8u));
        u32 row_w = window->w > gfx_scale_px(16u) ? (window->w - gfx_scale_px(16u)) : window->w;

        if (total_ticks > 0u) {
            usage = (g_process_cpu_ticks[idx] * 100u) / total_ticks;
        }

        if (idx == g_pm_selected_index) {
            framebuffer_fill_rect(row_x, y - 1u, row_w, row_h, 3u);
        }

        line[0] = '\0';
        {
            const char* name = g_windows[idx].title ? g_windows[idx].title : "Process";
            u32 p = 0u;
            u32 n = 0u;
            while (name[n] && p < 11u && p + 1u < sizeof(line)) {
                line[p++] = name[n++];
            }
            while (p < 12u && p + 1u < sizeof(line)) {
                line[p++] = ' ';
            }
            u32_to_dec_string(usage, num_buf, sizeof(num_buf));
            n = 0u;
            while (num_buf[n] && p + 1u < sizeof(line)) {
                line[p++] = num_buf[n++];
            }
            if (p + 1u < sizeof(line)) {
                line[p++] = '%';
            }
            line[p] = '\0';
        }
        gfx_render_label((u32)(window->x + (s32)gfx_scale_px(10u)), y, line, 0u,
                 (idx == g_pm_selected_index) ? 3u : window->body_bg);
        y += line_h;
        if (y + line_h >= btn_y) {
            break;
        }
    }

    framebuffer_fill_rect(btn_x, btn_y, btn_w, btn_h, (g_pm_selected_index >= 0) ? 12u : 8u);
    framebuffer_fill_rect(btn_x, btn_y, btn_w, 1u, 15u);
    framebuffer_fill_rect(btn_x, btn_y, 1u, btn_h, 15u);
    framebuffer_fill_rect(btn_x, btn_y + btn_h - 1u, btn_w, 1u, 0u);
    framebuffer_fill_rect(btn_x + btn_w - 1u, btn_y, 1u, btn_h, 0u);
    gfx_render_label(btn_x + gfx_scale_px(4u), btn_y + gfx_text_vertical_pad(btn_h), "KILL", 0u,
                     (g_pm_selected_index >= 0) ? 12u : 8u);
}

static void gfx_render_terminal_content(gfx_window* window) {
    u32 row;
    u32 col;
    u32 cols = visible_cols();
    u32 rows = visible_rows();
    u32 border = gfx_window_border_width();
    u32 title_h = gfx_window_title_height();
    u32 cell_w = gfx_cell_width();
    u32 cell_h = gfx_cell_height();
    u32 glyph_pad = 0u;
    u32 ox = (u32)(window->x + (s32)border);
    u32 oy = (u32)(window->y + (s32)title_h);

    for (row = 0; row < rows; row++) {
        for (col = 0; col < cols; col++) {
            const u8* glyph = font_glyph_at((u8)text_cells[row * VGA_WIDTH + col]);
            u32 px = ox + col * cell_w;
            u32 py = oy + row * cell_h;

            framebuffer_fill_rect(px, py, cell_w, cell_h, g_gfx_bg);
            gfx_draw_scaled_glyph(px + glyph_pad, py + glyph_pad, glyph, g_gfx_fg, g_gfx_bg);
        }
    }
}

static void gfx_render_terminal_cell(gfx_window* window, u32 row, u32 col) {
    u32 cols = visible_cols();
    u32 rows = visible_rows();
    u32 border = gfx_window_border_width();
    u32 title_h = gfx_window_title_height();
    u32 cell_w = gfx_cell_width();
    u32 cell_h = gfx_cell_height();
    u32 glyph_pad = 0u;
    u32 px;
    u32 py;
    const u8* glyph;

    if (row >= rows || col >= cols) {
        return;
    }

    px = (u32)(window->x + (s32)border) + col * cell_w;
    py = (u32)(window->y + (s32)title_h) + row * cell_h;
    glyph = font_glyph_at((u8)text_cells[row * VGA_WIDTH + col]);

    framebuffer_fill_rect(px, py, cell_w, cell_h, g_gfx_bg);
    gfx_draw_scaled_glyph(px + glyph_pad, py + glyph_pad, glyph, g_gfx_fg, g_gfx_bg);
    text_cells_dirty[row * VGA_WIDTH + col] = 0u;
}

static void clear_visible_text_dirty(void) {
    u32 row;
    u32 col;
    u32 cols = visible_cols();
    u32 rows = visible_rows();

    for (row = 0; row < rows; row++) {
        for (col = 0; col < cols; col++) {
            text_cells_dirty[row * VGA_WIDTH + col] = 0u;
        }
    }
}

static void gfx_refresh_terminal_incremental(void) {
    gfx_window* tw = terminal_window();
    u32 row;
    u32 col;
    u32 cols = visible_cols();
    u32 rows = visible_rows();

    mouse_hide_cursor();

    if (g_prev_cursor_valid && (g_prev_cursor_row != cursor_row || g_prev_cursor_col != cursor_col)) {
        gfx_render_terminal_cell(tw, g_prev_cursor_row, g_prev_cursor_col);
    }

    for (row = 0; row < rows; row++) {
        for (col = 0; col < cols; col++) {
            if (text_cells_dirty[row * VGA_WIDTH + col]) {
                gfx_render_terminal_cell(tw, row, col);
            }
        }
    }

    gfx_render_cursor();
    mouse_refresh_cursor();
    framebuffer_flush();

    g_prev_cursor_row = cursor_row;
    g_prev_cursor_col = cursor_col;
    g_prev_cursor_valid = 1u;
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
    framebuffer_fill_rect(0, framebuffer_height() - gfx_scale_px(4u), framebuffer_width(), gfx_scale_px(4u), 0u);
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
    u32 i;
    u8 r, g, b;

    io_outb(0x3C8, 0);  /* start writing from DAC index 0 */
    for (i = 0; i < 16u; i++) {
        io_outb(0x3C9, g_cga_palette[i][0]);
        io_outb(0x3C9, g_cga_palette[i][1]);
        io_outb(0x3C9, g_cga_palette[i][2]);
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
    font_load_from_memory(_binary_assets_fonts_terminus_psf_start,
                          _binary_assets_fonts_terminus_psf_end);

    if (video_get_mode() == VIDEO_MODE_GRAPHICS) {
        u8 overlay_enabled = 0u;
        load_cga_palette();
        if (video_get_boot_overlay_preference(&overlay_enabled) && overlay_enabled) {
            g_graphics_test_overlay = 1u;
        }
    }

    for (i = 0; i < GFX_MAX_COLS * GFX_MAX_ROWS; i++) {
        text_cells[i] = ' ';
        text_cells_dirty[i] = 1u;
    }
    for (i = 0; i < WIN_COUNT; i++) {
        g_process_cpu_ticks[i] = 0u;
    }
    g_pm_selected_index = -1;

    g_windows[WIN_DESKTOP].title_bg = 1u;
    g_windows[WIN_DESKTOP].body_bg = GFX_DESKTOP_BG;
    g_windows[WIN_DESKTOP].border = 0u;
    g_windows[WIN_DESKTOP].title = "Desktop";
    g_windows[WIN_DESKTOP].visible = 1u;
    g_windows[WIN_DESKTOP].role = GUI_WINDOW_ROLE_DESKTOP;

    {
        static const char* terminal_titles[WIN_TERMINAL_SLOTS] = {
            "Terminal",
            "Terminal 2",
            "Terminal 3"
        };
        s32 i;
        for (i = 0; i < WIN_TERMINAL_SLOTS; i++) {
            s32 wi = WIN_TERMINAL_FIRST + i;
            g_windows[wi].title_bg = 2u;
            g_windows[wi].body_bg = 0u;
            g_windows[wi].border = 15u;
            g_windows[wi].title = terminal_titles[i];
            g_windows[wi].visible = (i == 0) ? 1u : 0u;
            g_windows[wi].role = GUI_WINDOW_ROLE_TERMINAL;
        }
    }

    g_windows[WIN_SETTINGS].title_bg = 5u;
    g_windows[WIN_SETTINGS].body_bg = 7u;
    g_windows[WIN_SETTINGS].border = 15u;
    g_windows[WIN_SETTINGS].title = "Settings";
    g_windows[WIN_SETTINGS].visible = 0u;
    g_windows[WIN_SETTINGS].role = GUI_WINDOW_ROLE_SETTINGS;

    g_windows[WIN_PROCESS_MANAGER].title_bg = 4u;
    g_windows[WIN_PROCESS_MANAGER].body_bg = 7u;
    g_windows[WIN_PROCESS_MANAGER].border = 15u;
    g_windows[WIN_PROCESS_MANAGER].title = "Process Manager";
    g_windows[WIN_PROCESS_MANAGER].visible = 0u;
    g_windows[WIN_PROCESS_MANAGER].role = GUI_WINDOW_ROLE_PROCESS_MANAGER;

    apply_theme(0u);
    g_start_menu_open = 0u;
    gfx_layout_windows(1u);

    cursor_row = 0;
    cursor_col = 0;
    terminal_force_full_redraw = 1u;
    g_prev_cursor_valid = 0u;
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
        update_process_usage_sample();
        if (visible_terminal_count() > 1u) {
            terminal_force_full_redraw = 1u;
        }
        if (terminal_force_full_redraw) {
            gfx_render_full();
            mouse_refresh_cursor();
            framebuffer_flush();
            clear_visible_text_dirty();
            terminal_force_full_redraw = 0u;
            g_prev_cursor_row = cursor_row;
            g_prev_cursor_col = cursor_col;
            g_prev_cursor_valid = 1u;
        } else {
            gfx_refresh_terminal_incremental();
        }
    } else {
        text_render_full();
    }
}

void display_clear(void) {
    u32 i;
    for (i = 0; i < GFX_MAX_COLS * GFX_MAX_ROWS; i++) {
        text_cells[i] = ' ';
        text_cells_dirty[i] = 1u;
    }
    cursor_row = 0;
    cursor_col = 0;
    terminal_force_full_redraw = 1u;
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
            u32 dst_idx = (row - 1u) * VGA_WIDTH + col;
            u32 src_idx = row * VGA_WIDTH + col;
            text_cells[dst_idx] = text_cells[src_idx];
            text_cells_dirty[dst_idx] = 1u;
        }
    }

    for (col = 0; col < cols; col++) {
        text_cells[(rows - 1u) * VGA_WIDTH + col] = ' ';
        text_cells_dirty[(rows - 1u) * VGA_WIDTH + col] = 1u;
    }

    cursor_row = rows - 1u;
}

static void display_put_char_internal(char c, u8 do_refresh) {
    u32 idx;
    
    if (serial_is_ready()) {
        serial_write_char(c);
    }

    if (c == '\n') {
        cursor_col = 0;
        cursor_row++;
        scroll_if_needed();
        terminal_force_full_redraw = 1u;
        if (do_refresh) {
            display_refresh();
        }
        return;
    }

    if (c == '\b') {
        if (cursor_col > 0) {
            cursor_col--;
            idx = cursor_row * VGA_WIDTH + cursor_col;
            text_cells[idx] = ' ';
            text_cells_dirty[idx] = 1u;
        }
        if (do_refresh) {
            display_refresh();
        }
        return;
    }

    idx = cursor_row * VGA_WIDTH + cursor_col;
    text_cells[idx] = c;
    text_cells_dirty[idx] = 1u;
    
    cursor_col++;

    if (cursor_col >= visible_cols()) {
        cursor_col = 0;
        cursor_row++;
        scroll_if_needed();
        terminal_force_full_redraw = 1u;
    }

    if (do_refresh) {
        display_refresh();
    }
}

void display_put_char(char c) {
    display_put_char_internal(c, 1u);
}

void display_print(const char* s) {
    u8 wrote = 0u;
    while (s && *s) {
        display_put_char_internal(*s++, 0u);
        wrote = 1u;
    }
    if (wrote) {
        display_refresh();
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
    terminal_force_full_redraw = 1u;
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
        u32 scale = gfx_ui_scale();
        u32 taskbar_h = gfx_taskbar_height();
        u32 start_x = gfx_start_button_x();
        u32 start_w = gfx_start_button_width();
        u32 menu_w = gfx_start_menu_width();
        u32 item_h = gfx_start_menu_item_height();
        u32 swatch_w = gfx_settings_swatch_width();
        u32 swatch_h = gfx_settings_swatch_height();
        u32 resize = gfx_resize_handle_size();
        u32 title_h = gfx_window_title_height();
        s32 menu_y = start_menu_y();

        /* Start button toggles menu first. */
        if (y >= (s32)framebuffer_height() - (s32)taskbar_h + (s32)gfx_scale_px(3u) &&
            y < (s32)framebuffer_height() - 2 &&
            x >= (s32)start_x && x < (s32)(start_x + start_w)) {
            g_start_menu_open = g_start_menu_open ? 0u : 1u;
            mouse_hide_cursor();
            gfx_render_scene_no_cursor();
            mouse_refresh_cursor();
            prev_buttons = buttons;
            return;
        }

        /* Menu item selection has priority while menu is open. */
        if (g_start_menu_open) {
            if (x >= (s32)start_x && x < (s32)(start_x + menu_w) &&
                y >= menu_y && y < menu_y + (s32)(item_h * START_MENU_ITEMS)) {
                s32 rel = y - menu_y;
                s32 item = rel / (s32)item_h;

                if (item == 0) {
                    s32 ti = find_hidden_terminal_slot();
                    if (ti < 0) {
                        ti = window_index_from_ptr(terminal_window());
                    }
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
                } else if (item == 2) {
                    gfx_window* pw = process_manager_window();
                    s32 pi = window_index_from_ptr(pw);
                    if (pi >= 0) {
                        g_windows[pi].visible = 1u;
                        bring_window_to_front(pi);
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
            s32 hit = gui_find_topmost_visible_at(g_windows,
                                                  WIN_COUNT,
                                                  WIN_TERMINAL_FIRST,
                                                  x,
                                                  y,
                                                  0u);
            if (hit >= WIN_TERMINAL_FIRST) {
                gfx_window* window = &g_windows[hit];

                if (hit != WIN_COUNT - 1) {
                    bring_window_to_front(hit);
                    window = &g_windows[WIN_COUNT - 1];
                }

                if (window_close_button_hit(window, x, y)) {
                    window->visible = 0u;
                    if (window->role == GUI_WINDOW_ROLE_PROCESS_MANAGER) {
                        g_pm_selected_index = -1;
                    }
                    if (g_pm_selected_index == WIN_COUNT - 1) {
                        g_pm_selected_index = -1;
                    }
                    if (g_drag_window == WIN_COUNT - 1) {
                        g_drag_window = -1;
                    }
                    if (g_resize_window == WIN_COUNT - 1) {
                        g_resize_window = -1;
                    }
                    mouse_hide_cursor();
                    gfx_render_scene_no_cursor();
                    mouse_refresh_cursor();
                    prev_buttons = buttons;
                    return;
                }

                if (window->role == GUI_WINDOW_ROLE_SETTINGS) {
                    s32 si;
                    s32 sy = settings_swatch_y();
                    s32 slider_x;
                    s32 slider_y;
                    u32 slider_w;
                    u32 slider_h;
                    for (si = 0; si < (s32)THEME_COUNT; si++) {
                        s32 sx = settings_swatch_x(si);
                        if (x >= sx && x < sx + (s32)swatch_w &&
                            y >= sy && y < sy + (s32)swatch_h) {
                            apply_theme((u8)si);
                            mouse_hide_cursor();
                            gfx_render_scene_no_cursor();
                            mouse_refresh_cursor();
                            prev_buttons = buttons;
                            return;
                        }
                    }

                    settings_scale_slider_rect(window, &slider_x, &slider_y, &slider_w, &slider_h);
                    if (x >= slider_x && x < slider_x + (s32)slider_w &&
                        y >= slider_y - (s32)scale && y < slider_y + (s32)slider_h + (s32)scale) {
                        u32 scale_level;
                        s32 rel_x = x - slider_x;
                        if (slider_w <= 1u || UI_SCALE_HALF_MAX == UI_SCALE_HALF_MIN) {
                            scale_level = UI_SCALE_HALF_MIN;
                        } else {
                            scale_level = UI_SCALE_HALF_MIN
                                          + (u32)((rel_x * (s32)(UI_SCALE_HALF_MAX - UI_SCALE_HALF_MIN)
                                                   + (s32)(slider_w / 2u))
                                                  / (s32)(slider_w - 1u));
                        }
                        u8 new_level = (u8)scale_level;
                        if (new_level < UI_SCALE_HALF_MIN) {
                            new_level = UI_SCALE_HALF_MIN;
                        }
                        if (new_level > UI_SCALE_HALF_MAX) {
                            new_level = UI_SCALE_HALF_MAX;
                        }
                        if (g_ui_scale_level != new_level) {
                            g_ui_scale_level = new_level;
                            gfx_layout_windows(1u);
                            terminal_force_full_redraw = 1u;
                            g_prev_cursor_valid = 0u;
                        }
                        mouse_hide_cursor();
                        gfx_render_scene_no_cursor();
                        mouse_refresh_cursor();
                        prev_buttons = buttons;
                        return;
                    }
                }

                if (process_manager_handle_click(window, x, y)) {
                    mouse_hide_cursor();
                    gfx_render_scene_no_cursor();
                    mouse_refresh_cursor();
                    prev_buttons = buttons;
                    return;
                }

                g_drag_window = -1;
                g_resize_window = -1;

                {
                    gfx_window* selected = &g_windows[WIN_COUNT - 1];
                    s32 resize_x = selected->x + (s32)selected->w - (s32)resize;
                    s32 resize_y = selected->y + (s32)selected->h - (s32)resize;

                    if (x >= resize_x && y >= resize_y) {
                        g_resize_window = WIN_COUNT - 1;
                        g_resize_offset_x = (selected->x + (s32)selected->w) - x;
                        g_resize_offset_y = (selected->y + (s32)selected->h) - y;
                    } else if (y < selected->y + (s32)title_h) {
                        g_drag_window = WIN_COUNT - 1;
                        g_drag_offset_x = x - selected->x;
                        g_drag_offset_y = y - selected->y;
                    }
                }

                mouse_hide_cursor();
                gfx_render_scene_no_cursor();
            }
        }
    }

    if (g_drag_window >= 0 && (buttons & 0x01)) {
        gfx_window* window = &g_windows[g_drag_window];
        s32 new_x = x - g_drag_offset_x;
        s32 new_y = y - g_drag_offset_y;

        gui_clamp_window_position(window,
                                  &new_x,
                                  &new_y,
                                  framebuffer_width(),
                                  framebuffer_height(),
                                  gfx_taskbar_height());

        if (window->x != new_x || window->y != new_y) {
            window->x = new_x;
            window->y = new_y;
            mouse_hide_cursor();
            gfx_render_scene_no_cursor();
            mouse_refresh_cursor();
        }
    }

    if (g_resize_window >= 0 && (buttons & 0x01)) {
        gfx_window* window = &g_windows[g_resize_window];
        s32 new_w = x - window->x + g_resize_offset_x;
        s32 new_h = y - window->y + g_resize_offset_y;

        gui_clamp_window_size(window,
                              &new_w,
                              &new_h,
                              gfx_window_role_min_width(window->role),
                              gfx_window_role_min_height(window->role),
                              framebuffer_width(),
                              framebuffer_height(),
                              gfx_taskbar_height());

        if ((s32)window->w != new_w || (s32)window->h != new_h) {
            window->w = (u32)new_w;
            window->h = (u32)new_h;
            mouse_hide_cursor();
            gfx_render_scene_no_cursor();
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
    terminal_force_full_redraw = 1u;
}

u8 display_get_graphics_test_overlay(void) {
    return g_graphics_test_overlay;
}
