#include "cli.h"
#include "commands.h"
#include "display.h"
#include "keyboard.h"

#define INPUT_MAX 128

void cli_run(void) {
    char input[INPUT_MAX];
    u32 len = 0;

    display_print("> ");

    for (;;) {
        char c = keyboard_read_char();

        if (c == '\b') {
            if (len > 0) {
                len--;
                display_put_char('\b');
            }
            continue;
        }

        if (c == '\n') {
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
            display_print("> ");
            continue;
        }

        if (len < INPUT_MAX - 1) {
            input[len++] = c;
            display_put_char(c);
        }
    }
}
