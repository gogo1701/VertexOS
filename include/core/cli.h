/*
 * Command Line Interface
 *
 * Runs the interactive read-eval-print loop that reads a line of input,
 * dispatches it through the command system, and repeats.  Normally started
 * as a kernel task via scheduler_create_task().
 */

#ifndef CLI_H
#define CLI_H

#include "types.h"

/*
 * cli_run - Start the interactive shell loop.
 *
 * Blocks forever, reading user input and executing commands.  Should be
 * run inside a dedicated kernel task.  Never returns.
 */
void cli_run(u32 terminal_session);
void cli_task_entry(void* arg);

#endif /* CLI_H */
