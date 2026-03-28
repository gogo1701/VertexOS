/*
 * Command Handler System
 * 
 * Provides an easy way to register and execute kernel commands
 */

#ifndef COMMANDS_H
#define COMMANDS_H

#include "types.h"

/* Maximum number of commands the system can handle */
#define MAX_COMMANDS 24

/* Command function pointer type */
typedef void (*command_func)(const char* args);

/*
 * Register a new command
 * 
 * @name: Command name (e.g., "help", "clear")
 * @func: Function to execute when command is invoked
 * @return: 1 if successful, 0 if command table is full
 */
u8 command_register(const char* name, command_func func);

/*
 * Execute a command by name and arguments
 * 
 * @input: The full input line (e.g., "help" or "echo hello world")
 * @return: 1 if command was found and executed, 0 if not found
 */
u8 command_execute(const char* input);

/*
 * Get the number of registered commands
 * 
 * @return: Number of commands currently registered
 */
u32 command_count(void);

/*
 * Initialize built-in commands
 */
void commands_init(void);

#endif /* COMMANDS_H */
