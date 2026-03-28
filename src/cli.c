#include "cli.h"
#include "commands.h"
#include "display.h"
#include "keyboard.h"
#include "scheduler.h"
#include "vfs.h"

#define INPUT_MAX 128

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

    display_set_cursor(prompt_row, prompt_col);

    for (i = 0; i < len; i++) {
        display_put_char(input[i]);
    }

    for (i = len; i < *prev_len; i++) {
        display_put_char(' ');
    }

    display_set_cursor(prompt_row, prompt_col + cursor);
    *prev_len = len;
}

void cli_run(void) {
    char input[INPUT_MAX];
    u32 len = 0;
    u32 cursor = 0;
    u32 prev_len = 0;
    u32 prompt_row;
    u32 prompt_col;

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

        if (key == '\n') {
            input[len] = '\0';
            display_put_char('\n');

            if (len > 0) {
                if (!command_execute(input)) {
                    display_print("Unknown command: ");
                    display_print(input);
                    display_put_char('\n');
                }
            }

            len = 0;
            cursor = 0;
            prev_len = 0;
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
