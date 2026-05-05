#include "cli.h"
#include "commands.h"
#include "display.h"
#include "keyboard.h"
#include "scheduler.h"
#include "vfs.h"

#define INPUT_MAX 128
#define HISTORY_MAX 16

static u8 strings_equal(const char* a, const char* b) {
    while (*a && *b) {
        if (*a != *b) {
            return 0;
        }
        a++;
        b++;
    }
    return *a == *b;
}

static u8 starts_with(const char* s, const char* prefix) {
    u32 i = 0;
    while (prefix[i]) {
        if (s[i] != prefix[i]) {
            return 0;
        }
        i++;
    }
    return 1;
}

static void copy_str(char* dst, const char* src, u32 max) {
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

static void cli_print_prompt(void) {
    const char* cwd = vfs_get_cwd();
    if (cwd && cwd[0]) {
        display_print(cwd);
    } else {
        display_print("/");
    }
    display_print("> ");
}

static void redraw_input_line(
    const char* input,
    u32 len,
    u32 cursor,
    u32 prompt_row,
    u32 prompt_col,
    u32* prev_len
) {
    u32 i;

    display_begin_update();
    display_set_cursor(prompt_row, prompt_col);

    for (i = 0; i < len; i++) {
        display_put_char(input[i]);
    }

    for (i = len; i < *prev_len; i++) {
        display_put_char(' ');
    }

    display_set_cursor(prompt_row, prompt_col + cursor);
    display_end_update();
    *prev_len = len;
}

void cli_run(void) {
    char input[INPUT_MAX];
    char history[HISTORY_MAX][INPUT_MAX];
    u32 len = 0;
    u32 cursor = 0;
    u32 prev_len = 0;
    u32 prompt_row;
    u32 prompt_col;
    u32 history_count = 0;
    s32 history_pos = -1;
    u32 i;

    input[0] = '\0';
    for (i = 0; i < HISTORY_MAX; i++) {
        history[i][0] = '\0';
    }

    cli_print_prompt();
    display_get_cursor(&prompt_row, &prompt_col);

    for (;;) {
        s32 key = keyboard_read_key();

        if (key == KEY_LEFT) {
            if (cursor > 0) {
                cursor--;
                display_set_cursor(prompt_row, prompt_col + cursor);
            }
            continue;
        }

        if (key == KEY_RIGHT) {
            if (cursor < len) {
                cursor++;
                display_set_cursor(prompt_row, prompt_col + cursor);
            }
            continue;
        }

        if (key == KEY_DELETE) {
            if (cursor < len) {
                u32 i;
                for (i = cursor; i + 1 < len; i++) {
                    input[i] = input[i + 1];
                }
                len--;
                redraw_input_line(input, len, cursor, prompt_row, prompt_col, &prev_len);
            }
            continue;
        }

        if (key == '\b') {
            if (cursor > 0) {
                u32 i;
                for (i = cursor - 1; i + 1 < len; i++) {
                    input[i] = input[i + 1];
                }
                cursor--;
                len--;
                redraw_input_line(input, len, cursor, prompt_row, prompt_col, &prev_len);
            }
            continue;
        }

        if (key == KEY_UP) {
            if (history_count > 0) {
                if (history_pos < 0) {
                    history_pos = (s32)history_count - 1;
                } else if (history_pos > 0) {
                    history_pos--;
                }

                copy_str(input, history[history_pos], INPUT_MAX);
                len = 0;
                while (input[len]) {
                    len++;
                }
                cursor = len;
                redraw_input_line(input, len, cursor, prompt_row, prompt_col, &prev_len);
            }
            continue;
        }

        if (key == KEY_DOWN) {
            if (history_count > 0 && history_pos >= 0) {
                if (history_pos < (s32)history_count - 1) {
                    history_pos++;
                    copy_str(input, history[history_pos], INPUT_MAX);
                } else {
                    history_pos = -1;
                    input[0] = '\0';
                }

                len = 0;
                while (input[len]) {
                    len++;
                }
                cursor = len;
                redraw_input_line(input, len, cursor, prompt_row, prompt_col, &prev_len);
            }
            continue;
        }

        if (key == '\t') {
            u8 has_space = 0;
            u32 prefix_len = 0;

            for (i = 0; i < len; i++) {
                if (input[i] == ' ') {
                    has_space = 1;
                    break;
                }
            }

            if (has_space || cursor != len || len == 0) {
                continue;
            }

            prefix_len = len;
            {
                u32 matches = 0;
                const char* first = 0;

                for (i = 0; i < command_count(); i++) {
                    const char* name = command_name_at(i);
                    if (name && starts_with(name, input)) {
                        if (!first) {
                            first = name;
                        }
                        matches++;
                    }
                }

                if (matches == 1 && first) {
                    copy_str(input, first, INPUT_MAX);
                    len = 0;
                    while (input[len]) {
                        len++;
                    }
                    if (len + 1 < INPUT_MAX) {
                        input[len++] = ' ';
                        input[len] = '\0';
                    }
                    cursor = len;
                    redraw_input_line(input, len, cursor, prompt_row, prompt_col, &prev_len);
                } else if (matches > 1) {
                    display_put_char('\n');
                    for (i = 0; i < command_count(); i++) {
                        const char* name = command_name_at(i);
                        if (name && starts_with(name, input)) {
                            display_print(name);
                            display_put_char(' ');
                        }
                    }
                    display_put_char('\n');
                    cli_print_prompt();
                    display_get_cursor(&prompt_row, &prompt_col);
                    cursor = prefix_len;
                    redraw_input_line(input, len, cursor, prompt_row, prompt_col, &prev_len);
                }
            }
            continue;
        }

        if (key == '\n') {
            input[len] = '\0';
            display_put_char('\n');

            if (len > 0) {
                if (history_count == 0 || !strings_equal(history[history_count - 1], input)) {
                    if (history_count < HISTORY_MAX) {
                        copy_str(history[history_count], input, INPUT_MAX);
                        history_count++;
                    } else {
                        for (i = 1; i < HISTORY_MAX; i++) {
                            copy_str(history[i - 1], history[i], INPUT_MAX);
                        }
                        copy_str(history[HISTORY_MAX - 1], input, INPUT_MAX);
                    }
                }

                if (!command_execute(input)) {
                    display_print("Unknown command: ");
                    display_print(input);
                    display_put_char('\n');
                }
            }

            len = 0;
            cursor = 0;
            prev_len = 0;
            history_pos = -1;
            input[0] = '\0';
            cli_print_prompt();
            display_get_cursor(&prompt_row, &prompt_col);
            continue;
        }

        if (key >= 32 && key <= 126 && len < INPUT_MAX - 1) {
            u32 i;
            for (i = len; i > cursor; i--) {
                input[i] = input[i - 1];
            }
            input[cursor] = (char)key;
            cursor++;
            len++;
            redraw_input_line(input, len, cursor, prompt_row, prompt_col, &prev_len);
        }

        scheduler_maybe_preempt();
    }
}
