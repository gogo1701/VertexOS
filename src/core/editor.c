#include "editor.h"

#include "display.h"
#include "keyboard.h"
#include "vfs.h"

#define EDIT_MAX_BYTES 8192u
#define EDIT_PATH_MAX 64u
#define EDIT_STATUS_MAX 80u
#define EDIT_GUTTER_COLS 7u
#define EDIT_CONTENT_ROWS (VGA_HEIGHT - 2u)
#define EDIT_TAB_SPACES 4u

typedef struct {
    char path[EDIT_PATH_MAX];
    char buf[EDIT_MAX_BYTES + 1u];
    u32 len;
    u32 cursor;
    u32 top_line;
    u32 preferred_col;
    u8 keep_preferred_col;
    u8 dirty;
    char status[EDIT_STATUS_MAX];
    u8 first_draw;
    u32 prev_cursor_line;
    u32 prev_top_line;
    u32 prev_total_lines;
} editor_state;

static editor_state g_ed;

static void str_copy(char* dst, const char* src, u32 max) {
    u32 i = 0;
    if (max == 0) {
        return;
    }

    while (src && src[i] && i + 1 < max) {
        dst[i] = src[i];
        i++;
    }

    dst[i] = '\0';
}

static void status_set(const char* msg) {
    str_copy(g_ed.status, msg, sizeof(g_ed.status));
}

static void row_clear(u32 row) {
    u32 i;
    display_set_cursor(row, 0);
    for (i = 0; i < VGA_WIDTH; i++) {
        display_put_char(' ');
    }
}

static void row_write_text(u32 row, u32 col, const char* s, u32 max_chars) {
    u32 i = 0;

    if (row >= VGA_HEIGHT || col >= VGA_WIDTH || max_chars == 0) {
        return;
    }

    display_set_cursor(row, col);

    while (s && s[i] && i < max_chars && (col + i) < VGA_WIDTH) {
        display_put_char(s[i]);
        i++;
    }
}

static void row_write_u32_right(u32 row, u32 col, u32 width, u32 v) {
    char digits[10];
    u32 count = 0;
    u32 i;

    if (width == 0 || row >= VGA_HEIGHT || col >= VGA_WIDTH) {
        return;
    }

    if (v == 0) {
        digits[count++] = '0';
    } else {
        while (v && count < sizeof(digits)) {
            digits[count++] = (char)('0' + (v % 10u));
            v /= 10u;
        }
    }

    display_set_cursor(row, col);

    for (i = count; i < width; i++) {
        display_put_char(' ');
    }

    while (count > 0) {
        count--;
        display_put_char(digits[count]);
    }
}

static u32 pos_for_line(u32 target_line) {
    u32 i;
    u32 line = 0;

    for (i = 0; i < g_ed.len; i++) {
        if (line == target_line) {
            return i;
        }
        if (g_ed.buf[i] == '\n') {
            line++;
        }
    }

    return g_ed.len;
}

static u32 total_lines(void) {
    u32 i;
    u32 lines = 1;

    for (i = 0; i < g_ed.len; i++) {
        if (g_ed.buf[i] == '\n') {
            lines++;
        }
    }

    return lines;
}

static void line_col_for_pos(u32 pos, u32* out_line, u32* out_col) {
    u32 i;
    u32 line = 0;
    u32 col = 0;

    if (pos > g_ed.len) {
        pos = g_ed.len;
    }

    for (i = 0; i < pos; i++) {
        if (g_ed.buf[i] == '\n') {
            line++;
            col = 0;
        } else {
            col++;
        }
    }

    *out_line = line;
    *out_col = col;
}

static u32 line_start_for_pos(u32 pos) {
    if (pos > g_ed.len) {
        pos = g_ed.len;
    }

    while (pos > 0 && g_ed.buf[pos - 1] != '\n') {
        pos--;
    }

    return pos;
}

static u32 line_end_for_pos(u32 pos) {
    if (pos > g_ed.len) {
        pos = g_ed.len;
    }

    while (pos < g_ed.len && g_ed.buf[pos] != '\n') {
        pos++;
    }

    return pos;
}

static u32 pos_for_line_col(u32 line, u32 col) {
    u32 pos = pos_for_line(line);
    while (pos < g_ed.len && g_ed.buf[pos] != '\n' && col > 0) {
        pos++;
        col--;
    }
    return pos;
}

/* Calculate visual column (accounting for tab expansion to 4 spaces) */
static u32 visual_col_for_buffer_col(u32 line, u32 buf_col) {
    u32 pos = pos_for_line(line);
    u32 visual_col = 0;
    u32 buf_offset = 0;

    while (buf_offset < buf_col && pos < g_ed.len && g_ed.buf[pos] != '\n') {
        if (g_ed.buf[pos] == '\t') {
            visual_col += EDIT_TAB_SPACES;
        } else {
            visual_col += 1u;
        }
        pos++;
        buf_offset++;
    }

    return visual_col;
}

static void ensure_cursor_visible(void) {
    u32 line;
    u32 col;

    line_col_for_pos(g_ed.cursor, &line, &col);

    if (line < g_ed.top_line) {
        g_ed.top_line = line;
    } else if (line >= g_ed.top_line + EDIT_CONTENT_ROWS) {
        g_ed.top_line = line - EDIT_CONTENT_ROWS + 1u;
    }
}

static u8 insert_at_cursor(char c) {
    u32 i;

    if (g_ed.len >= EDIT_MAX_BYTES) {
        status_set("editor: file limit reached (8 KiB)");
        return 0;
    }

    for (i = g_ed.len; i > g_ed.cursor; i--) {
        g_ed.buf[i] = g_ed.buf[i - 1];
    }

    g_ed.buf[g_ed.cursor] = c;
    g_ed.cursor++;
    g_ed.len++;
    g_ed.buf[g_ed.len] = '\0';
    g_ed.dirty = 1;
    return 1;
}

static void insert_spaces(u32 n) {
    while (n > 0) {
        if (!insert_at_cursor(' ')) {
            return;
        }
        n--;
    }
}

static void delete_backspace(void) {
    u32 i;

    if (g_ed.cursor == 0 || g_ed.len == 0) {
        return;
    }

    for (i = g_ed.cursor - 1u; i + 1u < g_ed.len; i++) {
        g_ed.buf[i] = g_ed.buf[i + 1u];
    }

    g_ed.cursor--;
    g_ed.len--;
    g_ed.buf[g_ed.len] = '\0';
    g_ed.dirty = 1;
}

static void delete_forward(void) {
    u32 i;

    if (g_ed.cursor >= g_ed.len || g_ed.len == 0) {
        return;
    }

    for (i = g_ed.cursor; i + 1u < g_ed.len; i++) {
        g_ed.buf[i] = g_ed.buf[i + 1u];
    }

    g_ed.len--;
    g_ed.buf[g_ed.len] = '\0';
    g_ed.dirty = 1;
}

static void insert_newline_with_indent(void) {
    u32 i;
    u32 line_start;

    if (!insert_at_cursor('\n')) {
        return;
    }

    line_start = line_start_for_pos(g_ed.cursor - 1u);
    i = line_start;

    while (i < g_ed.len && g_ed.buf[i] != '\n') {
        if (g_ed.buf[i] == ' ') {
            if (!insert_at_cursor(' ')) {
                return;
            }
        } else if (g_ed.buf[i] == '\t') {
            insert_spaces(EDIT_TAB_SPACES);
        } else {
            break;
        }
        i++;
    }
}

static u8 save_to_path(const char* path) {
    s32 fd;
    u32 written;

    fd = vfs_open(path, VFS_O_WRITE | VFS_O_CREATE | VFS_O_TRUNC);
    if (fd < 0) {
        status_set("save failed: open error");
        return 0;
    }

    written = vfs_write(fd, g_ed.buf, g_ed.len);
    vfs_close(fd);

    if (written != g_ed.len) {
        status_set("save failed: short write");
        return 0;
    }

    g_ed.dirty = 0;
    status_set("saved");
    return 1;
}

static u8 prompt_path(char* out, u32 out_size) {
    char input[EDIT_PATH_MAX];
    u32 len = 0;
    u32 cursor = 0;

    input[0] = '\0';

    for (;;) {
        s32 key;

        row_clear(VGA_HEIGHT - 1u);
        row_write_text(VGA_HEIGHT - 1u, 0, "save as: ", VGA_WIDTH);
        row_write_text(VGA_HEIGHT - 1u, 9, input, VGA_WIDTH - 9u);
        display_set_cursor(VGA_HEIGHT - 1u, 9u + cursor);

        key = keyboard_read_key();

        if (key == 27) {
            status_set("save as canceled");
            return 0;
        }

        if (key == '\n') {
            if (len == 0) {
                status_set("save as: path required");
                return 0;
            }
            str_copy(out, input, out_size);
            return 1;
        }

        if (key == KEY_LEFT) {
            if (cursor > 0) {
                cursor--;
            }
            continue;
        }

        if (key == KEY_RIGHT) {
            if (cursor < len) {
                cursor++;
            }
            continue;
        }

        if (key == KEY_DELETE) {
            u32 i;
            if (cursor < len) {
                for (i = cursor; i + 1u < len; i++) {
                    input[i] = input[i + 1u];
                }
                len--;
                input[len] = '\0';
            }
            continue;
        }

        if (key == '\b') {
            u32 i;
            if (cursor > 0) {
                for (i = cursor - 1u; i + 1u < len; i++) {
                    input[i] = input[i + 1u];
                }
                cursor--;
                len--;
                input[len] = '\0';
            }
            continue;
        }

        if (key >= 32 && key <= 126 && len + 1u < sizeof(input)) {
            u32 i;
            for (i = len; i > cursor; i--) {
                input[i] = input[i - 1u];
            }
            input[cursor] = (char)key;
            cursor++;
            len++;
            input[len] = '\0';
        }
    }
}

/* Render a single content line to the screen */
static void render_line(u32 screen_row, u32 abs_line) {
    u32 line_pos = pos_for_line(abs_line);
    u32 line_end = line_end_for_pos(line_pos);
    u32 content_col = EDIT_GUTTER_COLS;
    u32 p;

    row_clear(screen_row);

    /* Line number gutter */
    row_write_u32_right(screen_row, 0, 4, abs_line + 1u);
    row_write_text(screen_row, 4, " | ", 3);

    /* Line content */
    for (p = line_pos; p < line_end && content_col < VGA_WIDTH; p++) {
        char c = g_ed.buf[p];
        if (c == '\t') {
            u32 t;
            for (t = 0; t < EDIT_TAB_SPACES && content_col < VGA_WIDTH; t++) {
                display_set_cursor(screen_row, content_col++);
                display_put_char(' ');
            }
        } else if (c >= 32 && c <= 126) {
            display_set_cursor(screen_row, content_col++);
            display_put_char(c);
        } else {
            display_set_cursor(screen_row, content_col++);
            display_put_char(' ');
        }
    }
}

static void draw_editor(void) {
    u32 line;
    u32 col;
    u32 total;
    u32 cursor_screen_row;
    u32 cursor_screen_col;
    char header[EDIT_STATUS_MAX];

    ensure_cursor_visible();

    line_col_for_pos(g_ed.cursor, &line, &col);
    total = total_lines();

    /* Full redraw when: first draw, viewport scrolls, or line count changed (Enter/Backspace) */
    if (g_ed.first_draw || g_ed.top_line != g_ed.prev_top_line || total != g_ed.prev_total_lines) {
        u32 i;

        /* Header */
        str_copy(header, "edit ", sizeof(header));
        str_copy(header + 5, g_ed.path, sizeof(header) - 5u);
        if (g_ed.dirty) {
            u32 n = 0;
            while (header[n]) {
                n++;
            }
            if (n + 3u < sizeof(header)) {
                header[n++] = ' ';
                header[n++] = '*';
                header[n] = '\0';
            }
        }

        row_clear(0);
        row_write_text(0, 0, header, VGA_WIDTH);
        row_write_text(0, 56, "F2 save  F10 quit  ESC menu", VGA_WIDTH - 56u);

        /* Full redraw of content area */
        for (i = 0; i < EDIT_CONTENT_ROWS; i++) {
            u32 screen_row = i + 1u;
            u32 abs_line = g_ed.top_line + i;

            if (abs_line >= total) {
                row_clear(screen_row);
            } else {
                render_line(screen_row, abs_line);
            }
        }

        g_ed.first_draw = 0;
    } else if (line != g_ed.prev_cursor_line) {
        /* Cursor moved to a different line: only redraw old and new cursor lines */
        u32 old_screen_row;
        u32 new_screen_row;

        /* Redraw old cursor line if it's still visible */
        if (g_ed.prev_cursor_line >= g_ed.top_line && g_ed.prev_cursor_line < g_ed.top_line + EDIT_CONTENT_ROWS) {
            old_screen_row = 1u + (g_ed.prev_cursor_line - g_ed.top_line);
            render_line(old_screen_row, g_ed.prev_cursor_line);
        }

        /* Redraw new cursor line */
        if (line >= g_ed.top_line && line < g_ed.top_line + EDIT_CONTENT_ROWS) {
            new_screen_row = 1u + (line - g_ed.top_line);
            render_line(new_screen_row, line);
        }
    } else {
        /* Typing on same line: only redraw current line */
        u32 screen_row;

        if (line >= g_ed.top_line && line < g_ed.top_line + EDIT_CONTENT_ROWS) {
            screen_row = 1u + (line - g_ed.top_line);
            render_line(screen_row, line);
        }
    }

    /* Always redraw status bar */
    row_clear(VGA_HEIGHT - 1u);
    row_write_text(VGA_HEIGHT - 1u, 0, g_ed.status, VGA_WIDTH);

    /* Position cursor */
    if (line < g_ed.top_line) {
        line = g_ed.top_line;
    }

    if (line >= g_ed.top_line + EDIT_CONTENT_ROWS) {
        line = g_ed.top_line + EDIT_CONTENT_ROWS - 1u;
    }

    cursor_screen_row = 1u + (line - g_ed.top_line);
    cursor_screen_col = EDIT_GUTTER_COLS + visual_col_for_buffer_col(line, col);
    if (cursor_screen_col >= VGA_WIDTH) {
        cursor_screen_col = VGA_WIDTH - 1u;
    }

    display_set_cursor(cursor_screen_row, cursor_screen_col);

    /* Cache state for next frame */
    g_ed.prev_cursor_line = line;
    g_ed.prev_top_line = g_ed.top_line;
    g_ed.prev_total_lines = total;
}

static void load_file(const char* path) {
    sfs_node_info st;
    s32 fd;
    u8 tmp[256];

    g_ed.len = 0;
    g_ed.cursor = 0;
    g_ed.top_line = 0;
    g_ed.preferred_col = 0;
    g_ed.keep_preferred_col = 0;
    g_ed.dirty = 0;
    g_ed.buf[0] = '\0';
    g_ed.first_draw = 1;
    g_ed.prev_cursor_line = 0;
    g_ed.prev_top_line = 0;
    g_ed.prev_total_lines = 0;

    if (!vfs_stat_path(path, &st)) {
        status_set("new file");
        return;
    }

    if (st.type != SFS_TYPE_FILE) {
        status_set("open failed: not a file");
        return;
    }

    fd = vfs_open(path, VFS_O_READ);
    if (fd < 0) {
        status_set("open failed");
        return;
    }

    for (;;) {
        u32 n = vfs_read(fd, tmp, sizeof(tmp));
        u32 i;

        if (n == 0) {
            break;
        }

        for (i = 0; i < n; i++) {
            if (g_ed.len >= EDIT_MAX_BYTES) {
                g_ed.buf[g_ed.len] = '\0';
                vfs_close(fd);
                status_set("loaded (truncated to 8 KiB)");
                return;
            }
            g_ed.buf[g_ed.len++] = (char)tmp[i];
        }
    }

    vfs_close(fd);
    g_ed.buf[g_ed.len] = '\0';
    status_set("loaded");
}

void editor_open(const char* path) {
    str_copy(g_ed.path, path, sizeof(g_ed.path));
    load_file(g_ed.path);

    for (;;) {
        s32 key = 0;

        draw_editor();
        key = keyboard_read_key();

        if (key == KEY_LEFT) {
            if (g_ed.cursor > 0) {
                g_ed.cursor--;
            }
            g_ed.keep_preferred_col = 0;
            continue;
        }

        if (key == KEY_RIGHT) {
            if (g_ed.cursor < g_ed.len) {
                g_ed.cursor++;
            }
            g_ed.keep_preferred_col = 0;
            continue;
        }

        if (key == KEY_UP || key == KEY_DOWN) {
            u32 line;
            u32 col;

            line_col_for_pos(g_ed.cursor, &line, &col);
            if (!g_ed.keep_preferred_col) {
                g_ed.preferred_col = col;
                g_ed.keep_preferred_col = 1;
            }

            if (key == KEY_UP && line > 0) {
                g_ed.cursor = pos_for_line_col(line - 1u, g_ed.preferred_col);
            } else if (key == KEY_DOWN) {
                u32 total = total_lines();
                if (line + 1u < total) {
                    g_ed.cursor = pos_for_line_col(line + 1u, g_ed.preferred_col);
                }
            }
            continue;
        }

        if (key == KEY_DELETE) {
            delete_forward();
            g_ed.keep_preferred_col = 0;
            continue;
        }

        if (key == '\b') {
            delete_backspace();
            g_ed.keep_preferred_col = 0;
            continue;
        }

        if (key == '\t') {
            insert_spaces(EDIT_TAB_SPACES);
            g_ed.keep_preferred_col = 0;
            continue;
        }

        if (key == '\n') {
            insert_newline_with_indent();
            g_ed.keep_preferred_col = 0;
            continue;
        }

        if (key == KEY_F2) {
            (void)save_to_path(g_ed.path);
            continue;
        }

        if (key == KEY_F10) {
            if (!g_ed.dirty) {
                display_clear();
                display_print("editor closed\n");
                return;
            }

            status_set("unsaved: F10 discard, F2 save+quit, ESC cancel");
            draw_editor();
            key = keyboard_read_key();

            if (key == KEY_F10) {
                display_clear();
                display_print("editor closed (discarded)\n");
                return;
            }

            if (key == KEY_F2) {
                if (save_to_path(g_ed.path)) {
                    display_clear();
                    display_print("editor closed (saved)\n");
                    return;
                }
            }

            status_set("quit canceled");
            continue;
        }

        if (key == 27) {
            char new_path[EDIT_PATH_MAX];

            status_set("menu: s save, a save-as, q quit, c continue");
            draw_editor();
            key = keyboard_read_key();

            if (key == 's') {
                (void)save_to_path(g_ed.path);
            } else if (key == 'a') {
                if (prompt_path(new_path, sizeof(new_path))) {
                    if (save_to_path(new_path)) {
                        str_copy(g_ed.path, new_path, sizeof(g_ed.path));
                        status_set("saved as new path");
                    }
                }
            } else if (key == 'q') {
                if (!g_ed.dirty) {
                    display_clear();
                    display_print("editor closed\n");
                    return;
                }
                status_set("use F10 quit flow for unsaved buffer");
            } else {
                status_set("continue editing");
            }
            continue;
        }

        if (key >= 32 && key <= 126) {
            (void)insert_at_cursor((char)key);
            g_ed.keep_preferred_col = 0;
            continue;
        }
    }
}
