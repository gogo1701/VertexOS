/*
 * Command Handler System Implementation
 */

#include "commands.h"
#include "display.h"
#include "heap.h"
#include "panic.h"
#include "pmm.h"
#include "pit.h"
#include "scheduler.h"
#include "syscall.h"

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
static void cmd_uptime(const char* args);
static void cmd_panic(const char* args);
static void cmd_meminfo(const char* args);
static void cmd_alloc(const char* args);
static void cmd_tasks(const char* args);
static void cmd_yield(const char* args);
static void cmd_syscall(const char* args);

u8 command_register(const char* name, command_func func) {
    u32 i;

    for (i = 0; i < command_count_val; i++) {
        if (strings_equal(commands[i].name, name)) {
            return 0;
        }
    }

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

    if (cmd_len == 0) {
        return 1;
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

/*
 * uptime - Print uptime derived from PIT timer ticks
 */
static void cmd_uptime(const char* args) {
    u32 ticks;
    u32 hz;
    u32 seconds;
    u32 centiseconds;

    (void)args;

    ticks = pit_get_ticks();
    hz = pit_get_frequency();
    if (hz == 0) {
        hz = 1;
    }

    seconds = ticks / hz;
    centiseconds = ((ticks % hz) * 100u) / hz;

    display_print("Uptime: ");
    display_print_num(seconds, 10);
    display_put_char('.');
    if (centiseconds < 10) {
        display_put_char('0');
    }
    display_print_num(centiseconds, 10);
    display_print(" seconds\n");
}

/*
 * panic - Trigger a kernel panic for testing diagnostics
 */
static void cmd_panic(const char* args) {
    if (args && *args) {
        panic(args);
    }
    panic("Manual panic command invoked");
}

/*
 * meminfo - Display PMM and heap usage stats
 */
static void cmd_meminfo(const char* args) {
    u32 total;
    u32 used;
    u32 free;
    u32 heap_total;
    u32 heap_used;

    (void)args;

    total = pmm_total_memory_bytes();
    used = pmm_used_memory_bytes();
    free = pmm_free_memory_bytes();
    heap_total = heap_total_bytes();
    heap_used = heap_used_bytes();

    display_print("PMM total: ");
    display_print_num(total / 1024u, 10);
    display_print(" KiB\n");

    display_print("PMM used : ");
    display_print_num(used / 1024u, 10);
    display_print(" KiB\n");

    display_print("PMM free : ");
    display_print_num(free / 1024u, 10);
    display_print(" KiB\n");

    display_print("Heap total: ");
    display_print_num(heap_total / 1024u, 10);
    display_print(" KiB\n");

    display_print("Heap used : ");
    display_print_num(heap_used / 1024u, 10);
    display_print(" KiB\n");
}

/*
 * alloc - Simple kmalloc/kfree self-test
 */
static void cmd_alloc(const char* args) {
    void* a;
    void* b;

    (void)args;

    a = kmalloc(64);
    b = kmalloc(128);

    if (!a || !b) {
        display_print("alloc test failed\n");
        return;
    }

    display_print("alloc ok: a=0x");
    display_print_num((u32)a, 16);
    display_print(" b=0x");
    display_print_num((u32)b, 16);
    display_put_char('\n');

    kfree(a);
    kfree(b);
    display_print("alloc free ok\n");
}

/*
 * tasks - Show scheduler task stats
 */
static void cmd_tasks(const char* args) {
    (void)args;
    display_print("Tasks: ");
    display_print_num(scheduler_task_count(), 10);
    display_print(" current_tid=");
    display_print_num(scheduler_current_tid(), 10);
    display_put_char('\n');
}

/*
 * yield - Voluntarily yield CPU to next task
 */
static void cmd_yield(const char* args) {
    (void)args;
    scheduler_yield();
}

/*
 * syscall - Test int 0x80 syscall interface
 */
static void cmd_syscall(const char* args) {
    u32 tid;
    u32 ticks;
    (void)args;

    tid = syscall_invoke(SYS_GET_TID, 0, 0, 0);
    ticks = syscall_invoke(SYS_GET_TICKS, 0, 0, 0);

    display_print("syscall tid=");
    display_print_num(tid, 10);
    display_print(" ticks=");
    display_print_num(ticks, 10);
    display_put_char('\n');

    syscall_invoke(SYS_YIELD, 0, 0, 0);
}

void commands_init(void) {
    command_register("help", cmd_help);
    command_register("clear", cmd_clear);
    command_register("echo", cmd_echo);
    command_register("uptime", cmd_uptime);
    command_register("panic", cmd_panic);
    command_register("meminfo", cmd_meminfo);
    command_register("alloc", cmd_alloc);
    command_register("tasks", cmd_tasks);
    command_register("yield", cmd_yield);
    command_register("syscall", cmd_syscall);
}
