/*
 * Command Handler System Implementation
 */

#include "commands.h"
#include "display.h"
#include "exec.h"
#include "heap.h"
#include "panic.h"
#include "pmm.h"
#include "pit.h"
#include "scheduler.h"
#include "syscall.h"
#include "vfs.h"

typedef struct {
    const char* name;
    const char* usage;
    const char* summary;
    command_func func;
} command_entry;

static command_entry commands[MAX_COMMANDS];
static u32 command_count_val = 0;

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
static void cmd_ls(const char* args);
static void cmd_cat(const char* args);
static void cmd_write(const char* args);
static void cmd_touch(const char* args);
static void cmd_mkdir(const char* args);
static void cmd_rm(const char* args);
static void cmd_cp(const char* args);
static void cmd_mv(const char* args);
static void cmd_cd(const char* args);
static void cmd_pwd(const char* args);
static void cmd_exec(const char* args);

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

static const char* skip_spaces(const char* s) {
    while (s && *s == ' ') {
        s++;
    }
    return s;
}

static u8 read_arg(const char** inout, char* out, u32 out_size) {
    const char* s = skip_spaces(*inout);
    u32 i = 0;
    char quote = 0;

    if (out_size == 0) {
        return 0;
    }

    if (!s || !*s) {
        out[0] = '\0';
        *inout = s;
        return 0;
    }

    if (*s == '"' || *s == '\'') {
        quote = *s;
        s++;
    }

    while (*s) {
        if (quote) {
            if (*s == quote) {
                s++;
                break;
            }
            if (*s == '\\' && s[1] && (s[1] == quote || s[1] == '\\')) {
                s++;
            }
        } else if (*s == ' ') {
            break;
        }

        if (i + 1 < out_size) {
            out[i++] = *s;
        }
        s++;
    }

    out[i] = '\0';
    *inout = s;
    return 1;
}

static u8 read_text_arg(const char** inout, char* out, u32 out_size) {
    const char* s = skip_spaces(*inout);
    u32 i = 0;

    if (out_size == 0) {
        return 0;
    }

    if (!s || !*s) {
        out[0] = '\0';
        *inout = s;
        return 0;
    }

    if (*s == '"' || *s == '\'') {
        return read_arg(inout, out, out_size);
    }

    while (s[i] && i + 1 < out_size) {
        out[i] = s[i];
        i++;
    }
    out[i] = '\0';
    *inout = s + i;

    return i > 0;
}

static command_entry* find_command(const char* name) {
    u32 i;
    for (i = 0; i < command_count_val; i++) {
        if (strings_equal(commands[i].name, name)) {
            return &commands[i];
        }
    }
    return 0;
}

static u8 command_register_full(const char* name, const char* usage, const char* summary, command_func func) {
    if (command_count_val >= MAX_COMMANDS || !name || !func || find_command(name)) {
        return 0;
    }

    commands[command_count_val].name = name;
    commands[command_count_val].usage = usage ? usage : name;
    commands[command_count_val].summary = summary ? summary : "";
    commands[command_count_val].func = func;
    command_count_val++;
    return 1;
}

u8 command_register(const char* name, command_func func) {
    return command_register_full(name, name, "", func);
}

u8 command_execute(const char* input) {
    const char* p = input;
    char cmd[32];
    command_entry* entry;

    if (!read_arg(&p, cmd, sizeof(cmd)) || !cmd[0]) {
        return 1;
    }

    entry = find_command(cmd);
    if (!entry) {
        return 0;
    }

    entry->func(skip_spaces(p));
    return 1;
}

u32 command_count(void) {
    return command_count_val;
}

static u8 copy_file(const char* src, const char* dst) {
    s32 in_fd;
    s32 out_fd;
    u8 buf[128];

    in_fd = vfs_open(src, VFS_O_READ);
    if (in_fd < 0) {
        return 0;
    }

    out_fd = vfs_open(dst, VFS_O_WRITE | VFS_O_CREATE | VFS_O_TRUNC);
    if (out_fd < 0) {
        vfs_close(in_fd);
        return 0;
    }

    for (;;) {
        u32 n = vfs_read(in_fd, buf, sizeof(buf));
        if (n == 0) {
            break;
        }
        if (vfs_write(out_fd, buf, n) != n) {
            vfs_close(in_fd);
            vfs_close(out_fd);
            return 0;
        }
    }

    vfs_close(in_fd);
    vfs_close(out_fd);
    return 1;
}

static void cmd_help(const char* args) {
    char name[32];

    if (read_arg(&args, name, sizeof(name)) && name[0]) {
        command_entry* entry = find_command(name);
        if (!entry) {
            display_print("help: command not found\n");
            return;
        }

        display_print(entry->name);
        display_print(" - ");
        display_print(entry->summary);
        display_put_char('\n');
        display_print("Usage: ");
        display_print(entry->usage);
        display_put_char('\n');
        return;
    }

    display_print("Commands:\n");
    display_print("(use: help <command> for details)\n");

    {
        u32 i;
        for (i = 0; i < command_count_val; i++) {
            display_print("  ");
            display_print(commands[i].name);
            display_print(" - ");
            display_print(commands[i].summary);
            display_put_char('\n');
        }
    }
}

static void cmd_clear(const char* args) {
    (void)args;
    display_clear();
}

static void cmd_echo(const char* args) {
    char text[128];
    const char* p = args;

    if (!read_text_arg(&p, text, sizeof(text))) {
        display_put_char('\n');
        return;
    }

    display_print(text);
    display_put_char('\n');
}

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

static void cmd_panic(const char* args) {
    if (args && *args) {
        panic(args);
    }
    panic("Manual panic command invoked");
}

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

static void cmd_tasks(const char* args) {
    (void)args;
    display_print("Tasks: ");
    display_print_num(scheduler_task_count(), 10);
    display_print(" current_tid=");
    display_print_num(scheduler_current_tid(), 10);
    display_put_char('\n');
}

static void cmd_yield(const char* args) {
    (void)args;
    scheduler_yield();
}

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

static void cmd_ls(const char* args) {
    char path[64];
    sfs_node_info entries[32];
    u32 i;
    u32 count;

    if (!read_arg(&args, path, sizeof(path)) || !path[0]) {
        path[0] = '.';
        path[1] = '\0';
    }

    count = vfs_list(path, entries, 32);
    for (i = 0; i < count; i++) {
        display_print(entries[i].type == SFS_TYPE_DIR ? "d" : "-");
        display_print((entries[i].perm & SFS_PERM_READ) ? "r" : "-");
        display_print((entries[i].perm & SFS_PERM_WRITE) ? "w" : "-");
        display_print((entries[i].perm & SFS_PERM_EXEC) ? "x" : "-");
        display_print(" ");
        display_print(entries[i].name);
        if (entries[i].type == SFS_TYPE_FILE) {
            display_print(" (");
            display_print_num(entries[i].size, 10);
            display_print("B)");
        }
        display_put_char('\n');
    }
    if (count == 0) {
        display_print("(empty)\n");
    }
}

static void cmd_cat(const char* args) {
    char path[64];
    s32 fd;
    u8 buf[128];
    u32 n;

    if (!read_arg(&args, path, sizeof(path)) || !path[0]) {
        display_print("Usage: cat <path>\n");
        return;
    }

    fd = vfs_open(path, VFS_O_READ);
    if (fd < 0) {
        display_print("cat: open failed\n");
        return;
    }

    for (;;) {
        n = vfs_read(fd, buf, sizeof(buf));
        if (n == 0) {
            break;
        }
        {
            u32 i;
            for (i = 0; i < n; i++) {
                display_put_char((char)buf[i]);
            }
        }
    }
    display_put_char('\n');
    vfs_close(fd);
}

static void cmd_write(const char* args) {
    char path[64];
    char text[128];
    s32 fd;
    u32 len = 0;
    u32 n;

    if (!read_arg(&args, path, sizeof(path)) || !path[0] || !read_text_arg(&args, text, sizeof(text))) {
        display_print("Usage: write <path> <text>\n");
        return;
    }

    fd = vfs_open(path, VFS_O_WRITE | VFS_O_CREATE | VFS_O_TRUNC);
    if (fd < 0) {
        display_print("write: open failed\n");
        return;
    }

    while (text[len]) {
        len++;
    }

    n = vfs_write(fd, text, len);
    vfs_close(fd);

    display_print("wrote ");
    display_print_num(n, 10);
    display_print(" bytes\n");
}

static void cmd_touch(const char* args) {
    char path[64];
    s32 fd;

    if (!read_arg(&args, path, sizeof(path)) || !path[0]) {
        display_print("Usage: touch <path>\n");
        return;
    }

    fd = vfs_open(path, VFS_O_WRITE | VFS_O_CREATE);
    if (fd < 0) {
        display_print("touch failed\n");
        return;
    }
    vfs_close(fd);
}

static void cmd_mkdir(const char* args) {
    char path[64];

    if (!read_arg(&args, path, sizeof(path)) || !path[0]) {
        display_print("Usage: mkdir <path>\n");
        return;
    }

    if (!vfs_mkdir(path)) {
        display_print("mkdir failed\n");
    }
}

static void cmd_rm(const char* args) {
    char path[64];

    if (!read_arg(&args, path, sizeof(path)) || !path[0]) {
        display_print("Usage: rm <path>\n");
        return;
    }

    if (!vfs_remove(path)) {
        display_print("rm failed\n");
    }
}

static void cmd_cp(const char* args) {
    char src[64];
    char dst[64];

    if (!read_arg(&args, src, sizeof(src)) || !read_arg(&args, dst, sizeof(dst)) || !src[0] || !dst[0]) {
        display_print("Usage: cp <src> <dst>\n");
        return;
    }

    if (!copy_file(src, dst)) {
        display_print("cp failed\n");
    }
}

static void cmd_mv(const char* args) {
    char src[64];
    char dst[64];

    if (!read_arg(&args, src, sizeof(src)) || !read_arg(&args, dst, sizeof(dst)) || !src[0] || !dst[0]) {
        display_print("Usage: mv <src> <dst>\n");
        return;
    }

    if (!copy_file(src, dst)) {
        display_print("mv failed\n");
        return;
    }

    if (!vfs_remove(src)) {
        display_print("mv warning: remove source failed\n");
    }
}

static void cmd_cd(const char* args) {
    char path[64];

    if (!read_arg(&args, path, sizeof(path)) || !path[0]) {
        path[0] = '/';
        path[1] = '\0';
    }

    if (!vfs_chdir(path)) {
        display_print("cd failed\n");
    }
}

static void cmd_pwd(const char* args) {
    (void)args;
    display_print(vfs_get_cwd());
    display_put_char('\n');
}

static void cmd_exec(const char* args) {
    char path[64];

    if (!read_arg(&args, path, sizeof(path)) || !path[0]) {
        display_print("Usage: exec <elf-path>\n");
        return;
    }

    if (!exec_run_elf(path)) {
        display_print("exec failed\n");
    }
}

void commands_init(void) {
    command_register_full("help", "help [command]", "Show command list or command help", cmd_help);
    command_register_full("clear", "clear", "Clear the screen", cmd_clear);
    command_register_full("echo", "echo [text]", "Print text (quotes supported)", cmd_echo);
    command_register_full("uptime", "uptime", "Show kernel uptime", cmd_uptime);
    command_register_full("panic", "panic [message]", "Trigger kernel panic", cmd_panic);
    command_register_full("meminfo", "meminfo", "Show PMM and heap usage", cmd_meminfo);
    command_register_full("alloc", "alloc", "Run basic allocator self-test", cmd_alloc);
    command_register_full("tasks", "tasks", "Show scheduler task state summary", cmd_tasks);
    command_register_full("yield", "yield", "Yield CPU to another task", cmd_yield);
    command_register_full("syscall", "syscall", "Run int 0x80 syscall demo", cmd_syscall);
    command_register_full("ls", "ls [path]", "List directory entries", cmd_ls);
    command_register_full("cat", "cat <path>", "Print file contents", cmd_cat);
    command_register_full("write", "write <path> <text>", "Write text to file (supports quotes)", cmd_write);
    command_register_full("touch", "touch <path>", "Create an empty file", cmd_touch);
    command_register_full("mkdir", "mkdir <path>", "Create a directory", cmd_mkdir);
    command_register_full("rm", "rm <path>", "Remove file or empty directory", cmd_rm);
    command_register_full("cp", "cp <src> <dst>", "Copy file", cmd_cp);
    command_register_full("mv", "mv <src> <dst>", "Move/rename file", cmd_mv);
    command_register_full("cd", "cd [path]", "Change current directory", cmd_cd);
    command_register_full("pwd", "pwd", "Print current directory", cmd_pwd);
    command_register_full("exec", "exec <elf-path>", "Load and run 32-bit ELF from disk", cmd_exec);
}
