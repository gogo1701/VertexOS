/*
 * GUI window manager primitives.
 *
 * This layer provides shared window operations so display code can stop
 * hardcoding per-window list manipulation logic.
 */

#ifndef GUI_H
#define GUI_H

#include "types.h"

typedef enum {
    GUI_WINDOW_ROLE_DESKTOP = 0,
    GUI_WINDOW_ROLE_TERMINAL,
    GUI_WINDOW_ROLE_SETTINGS,
    GUI_WINDOW_ROLE_PROCESS_MANAGER,
    GUI_WINDOW_ROLE_GENERIC
} gui_window_role;

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
    gui_window_role role;
    u8 slot_id; /* for terminal windows: index into per-terminal buffer array */
} gui_window;

s32 gui_window_index_from_ptr(gui_window* windows, s32 count, gui_window* target);
void gui_bring_window_to_front(gui_window* windows, s32 count, s32 first_movable_index, s32 index);

gui_window* gui_find_window_by_role(gui_window* windows,
                                    s32 count,
                                    gui_window_role role,
                                    s32 fallback_index);
s32 gui_find_topmost_visible_at(gui_window* windows,
                                s32 count,
                                s32 first_index,
                                s32 x,
                                s32 y,
                                u8 include_desktop);

void gui_clamp_window_position(const gui_window* window,
                               s32* x,
                               s32* y,
                               u32 bounds_w,
                               u32 bounds_h,
                               u32 reserved_bottom);
void gui_clamp_window_size(const gui_window* window,
                           s32* w,
                           s32* h,
                           u32 min_w,
                           u32 min_h,
                           u32 bounds_w,
                           u32 bounds_h,
                           u32 reserved_bottom);

#endif /* GUI_H */
