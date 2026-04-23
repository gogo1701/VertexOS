#include "gui.h"

s32 gui_window_index_from_ptr(gui_window* windows, s32 count, gui_window* target) {
    s32 i;

    if (!windows || !target || count <= 0) {
        return -1;
    }

    for (i = 0; i < count; i++) {
        if (&windows[i] == target) {
            return i;
        }
    }

    return -1;
}

void gui_bring_window_to_front(gui_window* windows, s32 count, s32 first_movable_index, s32 index) {
    if (!windows || count <= 0) {
        return;
    }

    while (index >= first_movable_index && index < (count - 1)) {
        gui_window tmp = windows[index];
        windows[index] = windows[index + 1];
        windows[index + 1] = tmp;
        index++;
    }
}

gui_window* gui_find_window_by_role(gui_window* windows,
                                    s32 count,
                                    gui_window_role role,
                                    s32 fallback_index) {
    s32 i;

    if (!windows || count <= 0) {
        return 0;
    }

    for (i = 0; i < count; i++) {
        if (windows[i].role == role) {
            return &windows[i];
        }
    }

    if (fallback_index >= 0 && fallback_index < count) {
        return &windows[fallback_index];
    }

    return 0;
}

s32 gui_find_topmost_visible_at(gui_window* windows,
                                s32 count,
                                s32 first_index,
                                s32 x,
                                s32 y,
                                u8 include_desktop) {
    s32 i;

    if (!windows || count <= 0) {
        return -1;
    }

    for (i = count - 1; i >= first_index; i--) {
        gui_window* window = &windows[i];
        if (!window->visible) {
            continue;
        }
        if (!include_desktop && window->role == GUI_WINDOW_ROLE_DESKTOP) {
            continue;
        }
        if (x >= window->x && x < window->x + (s32)window->w &&
            y >= window->y && y < window->y + (s32)window->h) {
            return i;
        }
    }

    return -1;
}

void gui_clamp_window_position(const gui_window* window,
                               s32* x,
                               s32* y,
                               u32 bounds_w,
                               u32 bounds_h,
                               u32 reserved_bottom) {
    s32 max_x;
    s32 max_y;

    if (!window || !x || !y) {
        return;
    }

    max_x = (s32)bounds_w - (s32)window->w;
    max_y = (s32)bounds_h - (s32)reserved_bottom - (s32)window->h;

    if (*x < 0) {
        *x = 0;
    }
    if (*y < 0) {
        *y = 0;
    }
    if (*x > max_x) {
        *x = max_x;
    }
    if (*y > max_y) {
        *y = max_y;
    }
}

void gui_clamp_window_size(const gui_window* window,
                           s32* w,
                           s32* h,
                           u32 min_w,
                           u32 min_h,
                           u32 bounds_w,
                           u32 bounds_h,
                           u32 reserved_bottom) {
    s32 max_w;
    s32 max_h;

    if (!window || !w || !h) {
        return;
    }

    max_w = (s32)bounds_w - window->x;
    max_h = (s32)bounds_h - (s32)reserved_bottom - window->y;

    if (*w < (s32)min_w) {
        *w = (s32)min_w;
    }
    if (*h < (s32)min_h) {
        *h = (s32)min_h;
    }
    if (*w > max_w) {
        *w = max_w;
    }
    if (*h > max_h) {
        *h = max_h;
    }
}
