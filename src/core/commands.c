/*
 * Command Handler System Implementation
 */

#include "commands.h"
#include "display.h"
#include "editor.h"
#include "exec.h"
#include "heap.h"
#include "net.h"
#include "panic.h"
#include "pmm.h"
#include "pit.h"
#include "power.h"
#include "rtc.h"
#include "scheduler.h"
#include "syscall.h"
#include "video.h"
#include "vfs.h"

typedef struct {
    const char* name;
    const char* usage;
    const char* summary;
    command_func func;
} command_entry;

static command_entry commands[MAX_COMMANDS];
static u32 command_count_val = 0;
static sfs_node_info ls_entries_buf[32];

static void cmd_help(const char* args);
static void cmd_clear(const char* args);
static void cmd_echo(const char* args);
static void cmd_uptime(const char* args);
static void cmd_time(const char* args);
static void cmd_panic(const char* args);
static void cmd_restart(const char* args);
static void cmd_shutdown(const char* args);
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
static void cmd_edit(const char* args);
static void cmd_video(const char* args);
static void cmd_resolution(const char* args);
static void cmd_ifconfig(const char* args);
static void cmd_dhcp(const char* args);
static void cmd_ping(const char* args);
static void cmd_dns(const char* args);

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

const char* command_name_at(u32 index) {
    if (index >= command_count_val) {
        return 0;
    }
    return commands[index].name;
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

static void print_name_bounded(const char* name) {
    u32 i;

    if (!name) {
        return;
    }

    for (i = 0; i < 31u && name[i]; i++) {
        char c = name[i];
        if (c < 32 || c > 126) {
            c = '?';
        }
        display_put_char(c);
    }
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

static void print_2digit(u8 v) {
    if (v < 10) {
        display_put_char('0');
    }
    display_print_num(v, 10);
}

static void cmd_time(const char* args) {
    rtc_datetime dt;
    (void)args;

    if (!rtc_read_datetime(&dt)) {
        display_print("time: rtc read failed\n");
        return;
    }

    display_print("20");
    print_2digit(dt.year);
    display_put_char('-');
    print_2digit(dt.month);
    display_put_char('-');
    print_2digit(dt.day);
    display_put_char(' ');
    print_2digit(dt.hour);
    display_put_char(':');
    print_2digit(dt.minute);
    display_put_char(':');
    print_2digit(dt.second);
    display_put_char('\n');
}

static void cmd_panic(const char* args) {
    if (args && *args) {
        panic(args);
    }
    panic("Manual panic command invoked");
}

static void cmd_restart(const char* args) {
    (void)args;
    display_print("Restarting system...\n");
    power_restart();
}

static void cmd_shutdown(const char* args) {
    (void)args;
    display_print("Shutting down system...\n");
    power_shutdown();
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
    u32 i;
    u32 count;

    if (!read_arg(&args, path, sizeof(path)) || !path[0]) {
        path[0] = '.';
        path[1] = '\0';
    }

    count = vfs_list(path, ls_entries_buf, 32);
    for (i = 0; i < count; i++) {
        display_print(ls_entries_buf[i].type == SFS_TYPE_DIR ? "d" : "-");
        display_print((ls_entries_buf[i].perm & SFS_PERM_READ) ? "r" : "-");
        display_print((ls_entries_buf[i].perm & SFS_PERM_WRITE) ? "w" : "-");
        display_print((ls_entries_buf[i].perm & SFS_PERM_EXEC) ? "x" : "-");
        display_print(" ");
        print_name_bounded(ls_entries_buf[i].name);
        if (ls_entries_buf[i].type == SFS_TYPE_FILE) {
            display_print(" (");
            display_print_num(ls_entries_buf[i].size, 10);
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

static void cmd_edit(const char* args) {
    char path[64];

    if (!read_arg(&args, path, sizeof(path)) || !path[0]) {
        display_print("Usage: edit <path>\n");
        display_print("NOTE: Editor is a WIP feature. Expect bugs.\n");
        return;
    }

    display_print("[WIP] Opening editor...\n");
    editor_open(path);
}

static void cmd_video(const char* args) {
    char mode[16];
    char subarg[16];
    video_mode pref;
    video_resolution current_res = video_get_resolution();
    video_resolution next_res;
    u8 has_next_res = video_get_boot_resolution_preference(&next_res);

    if (!read_arg(&args, mode, sizeof(mode)) || !mode[0] || strings_equal(mode, "status")) {
        display_print("video current: ");
        display_print(video_mode_name(video_get_mode()));
        if (video_get_mode() == VIDEO_MODE_GRAPHICS) {
            display_print(" (");
            display_print(video_resolution_name(current_res));
            display_put_char(')');
        }
        display_put_char('\n');

        if (video_get_boot_preference(&pref)) {
            display_print("video next boot: ");
            display_print(video_mode_name(pref));
            if (pref == VIDEO_MODE_GRAPHICS && has_next_res) {
                display_print(" (");
                display_print(video_resolution_name(next_res));
                display_put_char(')');
            }
            display_put_char('\n');
        }
        return;
    }

    if (strings_equal(mode, "test")) {
        if (!read_arg(&args, subarg, sizeof(subarg)) || !subarg[0] || strings_equal(subarg, "status")) {
            if (video_get_mode() == VIDEO_MODE_GRAPHICS) {
                display_print("video test: ");
                display_print(display_get_graphics_test_overlay() ? "on" : "off");
                display_put_char('\n');
            } else {
                u8 overlay_saved = 0u;
                if (video_get_boot_overlay_preference(&overlay_saved)) {
                    display_print("video test: ");
                    display_print(overlay_saved ? "on" : "off");
                    display_print(" (next boot)");
                    display_put_char('\n');
                } else {
                    display_print("video test: unknown\n");
                }
            }
            return;
        }

        if (strings_equal(subarg, "on")) {
            if (video_get_mode() != VIDEO_MODE_GRAPHICS) {
                if (!video_set_boot_preference(VIDEO_MODE_GRAPHICS) || !video_set_boot_overlay_preference(1u)) {
                    display_print("video: save failed\n");
                    return;
                }
                display_print("video test: on (graphics+overlay saved, restarting...)\n");
                power_restart();
                return;
            }

            display_set_graphics_test_overlay(1);
            display_print("video test: on\n");
            return;
        }

        if (strings_equal(subarg, "off")) {
            (void)video_set_boot_overlay_preference(0u);
            if (video_get_mode() == VIDEO_MODE_GRAPHICS) {
                display_set_graphics_test_overlay(0);
            }
            display_print("video test: off\n");
            return;
        }

        display_print("Usage: video test [on|off|status]\n");
        return;
    }

    if (strings_equal(mode, "text")) {
        if (!video_set_boot_preference(VIDEO_MODE_TEXT)) {
            display_print("video: save failed\n");
            return;
        }
        display_print("video: text saved for next boot\n");
        return;
    }

    if (strings_equal(mode, "gfx") || strings_equal(mode, "graphics")) {
        if (!video_set_boot_preference(VIDEO_MODE_GRAPHICS)) {
            display_print("video: save failed\n");
            return;
        }
        if (has_next_res) {
            display_print("video: graphics saved for next boot (");
            display_print(video_resolution_name(next_res));
            display_print(")\n");
        } else {
            display_print("video: graphics saved for next boot\n");
        }
        return;
    }

    display_print("Usage: video [status|text|gfx|test]\n");
}

static void cmd_resolution(const char* args) {
    char value[24];
    video_mode boot_mode;
    video_resolution current = video_get_resolution();
    video_resolution next;

    if (!read_arg(&args, value, sizeof(value)) || !value[0] || strings_equal(value, "status")) {
        display_print("resolution current: ");
        display_print(video_resolution_name(current));
        if (video_get_mode() != VIDEO_MODE_GRAPHICS) {
            display_print(" (text mode active)");
        }
        display_put_char('\n');

        if (video_get_boot_resolution_preference(&next)) {
            display_print("resolution next boot: ");
            display_print(video_resolution_name(next));
            if (video_get_boot_preference(&boot_mode) && boot_mode == VIDEO_MODE_TEXT) {
                display_print(" (video text selected)");
            }
            display_put_char('\n');
        }
        return;
    }

    if (strings_equal(value, "320x200")) {
        next = VIDEO_RES_320X200;
    } else if (strings_equal(value, "640x480")) {
        next = VIDEO_RES_640X480;
    } else if (strings_equal(value, "800x600")) {
        next = VIDEO_RES_800X600;
    } else {
        display_print("Usage: resolution [status|320x200|640x480|800x600]\n");
        return;
    }

    if (!video_set_boot_resolution_preference(next)) {
        display_print("resolution: save failed\n");
        return;
    }
    if (!video_set_boot_preference(VIDEO_MODE_GRAPHICS)) {
        display_print("resolution: save failed\n");
        return;
    }

    display_print("resolution: ");
    display_print(video_resolution_name(next));
    display_print(" saved for next boot (restart required)\n");
}

static u8 parse_u32_arg(const char* s, u32* out_value) {
    u32 v = 0;
    u32 i = 0;

    if (!s || !s[0]) {
        return 0;
    }

    while (s[i]) {
        if (s[i] < '0' || s[i] > '9') {
            return 0;
        }
        v = v * 10u + (u32)(s[i] - '0');
        i++;
    }

    *out_value = v;
    return 1;
}

static void cmd_ifconfig(const char* args) {
    (void)args;
    net_print_config();
}

static void cmd_dhcp(const char* args) {
    (void)args;

    if (!net_is_ready()) {
        display_print("dhcp: network link is down\n");
        return;
    }

    display_print("dhcp: requesting lease...\n");
    if (!net_dhcp_request()) {
        display_print("dhcp: failed to acquire lease\n");
        return;
    }

    display_print("dhcp: lease acquired\n");
    net_print_config();
}

static void cmd_ping(const char* args) {
    char ip_text[32];
    char timeout_text[16];
    u32 ip;
    u32 timeout_ms = 1000u;
    u32 rtt;

    if (!read_arg(&args, ip_text, sizeof(ip_text)) || !ip_text[0]) {
        display_print("Usage: ping <ipv4> [timeout_ms]\n");
        return;
    }

    if (!net_parse_ipv4(ip_text, &ip)) {
        display_print("ping: invalid ipv4 address\n");
        return;
    }

    if (read_arg(&args, timeout_text, sizeof(timeout_text)) && timeout_text[0]) {
        if (!parse_u32_arg(timeout_text, &timeout_ms)) {
            display_print("ping: invalid timeout\n");
            return;
        }
    }

    display_print("ping: sending to ");
    display_print(ip_text);
    display_put_char('\n');

    if (!net_ping(ip, timeout_ms, &rtt)) {
        display_print("ping: timeout or network error\n");
        return;
    }

    display_print("ping reply: ");
    display_print_num(rtt, 10);
    display_print(" ms\n");
}

static void cmd_dns(const char* args) {
    char host[128];
    char timeout_text[16];
    char ip_text[16];
    u32 ip;
    u32 timeout_ms = 1500u;

    if (!read_arg(&args, host, sizeof(host)) || !host[0]) {
        display_print("Usage: dns <hostname|ipv4> [timeout_ms]\n");
        return;
    }

    if (read_arg(&args, timeout_text, sizeof(timeout_text)) && timeout_text[0]) {
        if (!parse_u32_arg(timeout_text, &timeout_ms)) {
            display_print("dns: invalid timeout\n");
            return;
        }
    }

    display_print("dns: resolving ");
    display_print(host);
    display_put_char('\n');

    if (!net_resolve_ipv4(host, timeout_ms, &ip)) {
        display_print("dns: resolve failed\n");
        return;
    }

    net_format_ipv4(ip, ip_text, sizeof(ip_text));
    display_print(host);
    display_print(" -> ");
    display_print(ip_text);
    display_put_char('\n');
}

void commands_init(void) {
    command_register_full("help", "help [command]", "Show command list or command help", cmd_help);
    command_register_full("clear", "clear", "Clear the screen", cmd_clear);
    command_register_full("echo", "echo [text]", "Print text (quotes supported)", cmd_echo);
    command_register_full("uptime", "uptime", "Show kernel uptime", cmd_uptime);
    command_register_full("time", "time", "Show RTC date/time", cmd_time);
    command_register_full("panic", "panic [message]", "Trigger kernel panic", cmd_panic);
    command_register_full("restart", "restart", "Reset the machine", cmd_restart);
    command_register_full("shutdown", "shutdown", "Power off the machine", cmd_shutdown);
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
    command_register_full("edit", "edit <path> (WIP)", "Open console code editor (work in progress)", cmd_edit);
    command_register_full("video", "video [status|text|gfx|test [on|off|status]]", "Show mode, save next-boot mode, or toggle graphics test overlay", cmd_video);
    command_register_full("resolution", "resolution [status|320x200|640x480|800x600]", "Set next-boot graphics resolution", cmd_resolution);
    command_register_full("ifconfig", "ifconfig", "Show network interface and IP configuration", cmd_ifconfig);
    command_register_full("dhcp", "dhcp", "Request an IPv4 lease using DHCP", cmd_dhcp);
    command_register_full("ping", "ping <ipv4> [timeout_ms]", "Send one ICMP echo request", cmd_ping);
    command_register_full("dns", "dns <hostname|ipv4> [timeout_ms]", "Resolve hostname to IPv4 (A record)", cmd_dns);
}
