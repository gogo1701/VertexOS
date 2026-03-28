/*
 * Command Handler System Implementation
 */

#include "commands.h"
#include "display.h"

/* Command registry entry */
typedef struct {
    const char* name;
    command_func func;
} command_entry;

/* Command registry */
static command_entry commands[MAX_COMMANDS];
static u32 command_count_val = 0;

/* Forward declarations of built-in commands */
static void cmd_help(const char* args);
static void cmd_clear(const char* args);
static void cmd_echo(const char* args);

u8 command_register(const char* name, command_func func) {
    if (command_count_val >= MAX_COMMANDS) {
        return 0;
    }
    
    commands[command_count_val].name = name;
    commands[command_count_val].func = func;
    command_count_val++;
    
    return 1;
}

u8 command_execute(const char* input) {
    /* Skip leading spaces */
    while (*input == ' ') {
        input++;
    }
    
    /* Find command name (up to space or end of string) */
    const char* cmd_start = input;
    u32 cmd_len = 0;
    while (input[cmd_len] && input[cmd_len] != ' ') {
        cmd_len++;
    }
    
    /* Skip to arguments */
    const char* args = input + cmd_len;
    while (*args == ' ') {
        args++;
    }
    
    /* Search for matching command */
    u32 i;
    for (i = 0; i < command_count_val; i++) {
        const char* name = commands[i].name;
        u32 j = 0;
        
        /* Check if command name matches */
        while (j < cmd_len && name[j] == cmd_start[j]) {
            j++;
        }
        
        /* Command matches if we've consumed the full cmd_len and name ends */
        if (j == cmd_len && name[j] == '\0') {
            commands[i].func(args);
            return 1;
        }
    }
    
    return 0;
}

u32 command_count(void) {
    return command_count_val;
}

/* ============================
 * Built-in Commands
 * ============================ */

/*
 * help - Display available commands
 */
static void cmd_help(const char* args) {
    (void)args;  /* Unused parameter */
    
    display_print("Available commands:\n");
    u32 i;
    for (i = 0; i < command_count_val; i++) {
        display_print("  ");
        display_print(commands[i].name);
        display_put_char('\n');
    }
}

/*
 * clear - Clear the screen
 */
static void cmd_clear(const char* args) {
    (void)args;  /* Unused parameter */
    display_clear();
}

/*
 * echo - Echo back the arguments
 */
static void cmd_echo(const char* args) {
    display_print(args);
    display_put_char('\n');
}

void commands_init(void) {
    command_register("help", cmd_help);
    command_register("clear", cmd_clear);
    command_register("echo", cmd_echo);
}
