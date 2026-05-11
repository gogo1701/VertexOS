#include "vfs.h"
#include "blockdev.h"
#include "display.h"
#include "scheduler.h"

static vfs_fd g_fds[VFS_MAX_FDS];

#define VFS_CWD_PATH_MAX 128u
#define VFS_ENV_SLOTS 16u
#define VFS_ENV_KEY_MAX 16u
#define VFS_ENV_VALUE_MAX 64u

typedef struct {
    u32 cwd;
    char cwd_path[VFS_CWD_PATH_MAX];
    u8 env_used[VFS_ENV_SLOTS];
    char env_keys[VFS_ENV_SLOTS][VFS_ENV_KEY_MAX];
    char env_values[VFS_ENV_SLOTS][VFS_ENV_VALUE_MAX];
} vfs_terminal_context;

static vfs_terminal_context g_vfs_ctx[DISPLAY_MAX_TERMINALS];

static u32 s_len(const char* s) {
    u32 n = 0;
    while (s && s[n]) {
        n++;
    }
    return n;
}

static void s_copy(char* dst, const char* src, u32 max) {
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

static vfs_terminal_context* vfs_context_for_session(s32 session) {
    if (session >= 0 && session < (s32)DISPLAY_MAX_TERMINALS) {
        return &g_vfs_ctx[session];
    }
    return &g_vfs_ctx[0];
}

static vfs_terminal_context* vfs_current_context(void) {
    u32 tid = scheduler_current_tid();
    s32 session = display_terminal_session_for_task(tid);
    return vfs_context_for_session(session);
}

static void cwd_update(vfs_terminal_context* ctx, const char* path) {
    if (!ctx) {
        return;
    }
    s_copy(ctx->cwd_path, path, VFS_CWD_PATH_MAX);
}

static u8 env_key_valid(const char* key) {
    return key && key[0] != '\0';
}

static s32 env_find_slot(vfs_terminal_context* ctx, const char* key) {
    u32 i;
    if (!ctx || !env_key_valid(key)) {
        return -1;
    }
    for (i = 0u; i < VFS_ENV_SLOTS; i++) {
        if (!ctx->env_used[i]) {
            continue;
        }
        if (s_len(ctx->env_keys[i]) == s_len(key)) {
            u32 j = 0u;
            while (ctx->env_keys[i][j] && key[j] && ctx->env_keys[i][j] == key[j]) {
                j++;
            }
            if (ctx->env_keys[i][j] == '\0' && key[j] == '\0') {
                return (s32)i;
            }
        }
    }
    return -1;
}

static s32 env_find_free_slot(vfs_terminal_context* ctx) {
    u32 i;
    if (!ctx) {
        return -1;
    }
    for (i = 0u; i < VFS_ENV_SLOTS; i++) {
        if (!ctx->env_used[i]) {
            return (s32)i;
        }
    }
    return -1;
}

u8 vfs_init(void) {
    u32 i;
    u32 s;

    blockdev_init();
    if (!sfs_init()) {
        return 0;
    }

    for (i = 0; i < VFS_MAX_FDS; i++) {
        g_fds[i].used = 0;
    }

    for (s = 0u; s < DISPLAY_MAX_TERMINALS; s++) {
        u32 e;
        g_vfs_ctx[s].cwd = sfs_root_id();
        cwd_update(&g_vfs_ctx[s], "/");
        for (e = 0u; e < VFS_ENV_SLOTS; e++) {
            g_vfs_ctx[s].env_used[e] = 0u;
            g_vfs_ctx[s].env_keys[e][0] = '\0';
            g_vfs_ctx[s].env_values[e][0] = '\0';
        }
    }
    return 1;
}

s32 vfs_open(const char* path, u32 flags) {
    u32 node_id;
    u32 i;
    vfs_terminal_context* ctx = vfs_current_context();

    if (!sfs_resolve(path, ctx->cwd, &node_id)) {
        if (flags & VFS_O_CREATE) {
            if (!sfs_create(path, ctx->cwd, SFS_TYPE_FILE, (u8)(SFS_PERM_READ | SFS_PERM_WRITE), &node_id)) {
                return -1;
            }
        } else {
            return -1;
        }
    }

    if (flags & VFS_O_TRUNC) {
        if (!sfs_truncate(node_id)) {
            return -1;
        }
    }

    for (i = 0; i < VFS_MAX_FDS; i++) {
        if (!g_fds[i].used) {
            g_fds[i].used = 1;
            g_fds[i].node_id = node_id;
            g_fds[i].offset = 0;
            g_fds[i].flags = flags;
            return (s32)i;
        }
    }

    return -1;
}

u32 vfs_read(s32 fd, void* out, u32 len) {
    u32 read_len = 0;

    if (fd < 0 || fd >= (s32)VFS_MAX_FDS || !g_fds[fd].used) {
        return 0;
    }
    if (!(g_fds[fd].flags & VFS_O_READ)) {
        return 0;
    }

    if (!sfs_read(g_fds[fd].node_id, g_fds[fd].offset, out, len, &read_len)) {
        return 0;
    }

    g_fds[fd].offset += read_len;
    return read_len;
}

u32 vfs_write(s32 fd, const void* data, u32 len) {
    u32 write_len = 0;

    if (fd < 0 || fd >= (s32)VFS_MAX_FDS || !g_fds[fd].used) {
        return 0;
    }
    if (!(g_fds[fd].flags & VFS_O_WRITE)) {
        return 0;
    }

    if (!sfs_write(g_fds[fd].node_id, g_fds[fd].offset, data, len, &write_len)) {
        return 0;
    }

    g_fds[fd].offset += write_len;
    return write_len;
}

void vfs_close(s32 fd) {
    if (fd < 0 || fd >= (s32)VFS_MAX_FDS) {
        return;
    }
    g_fds[fd].used = 0;
}

u8 vfs_mkdir(const char* path) {
    vfs_terminal_context* ctx = vfs_current_context();
    u32 id;
    return sfs_create(path, ctx->cwd, SFS_TYPE_DIR, (u8)(SFS_PERM_READ | SFS_PERM_WRITE | SFS_PERM_EXEC), &id);
}

u8 vfs_remove(const char* path) {
    vfs_terminal_context* ctx = vfs_current_context();
    return sfs_remove(path, ctx->cwd);
}

u8 vfs_chdir(const char* path) {
    u32 id;
    sfs_node_info st;
    vfs_terminal_context* ctx = vfs_current_context();

    if (!sfs_resolve(path, ctx->cwd, &id)) {
        return 0;
    }

    if (!sfs_stat(id, &st) || st.type != SFS_TYPE_DIR) {
        return 0;
    }

    ctx->cwd = id;

    if (path[0] == '/') {
        cwd_update(ctx, path);
    } else if (path[0] == '.' && path[1] == '\0') {
        (void)0;
    } else if (path[0] == '.' && path[1] == '.' && path[2] == '\0') {
        if (s_len(ctx->cwd_path) > 1) {
            s32 i = (s32)s_len(ctx->cwd_path) - 1;
            while (i > 0 && ctx->cwd_path[i] == '/') {
                ctx->cwd_path[i--] = '\0';
            }
            while (i > 0 && ctx->cwd_path[i] != '/') {
                ctx->cwd_path[i--] = '\0';
            }
            if (i == 0) {
                ctx->cwd_path[1] = '\0';
            } else {
                ctx->cwd_path[i] = '\0';
            }
        }
    } else {
        u32 n = s_len(ctx->cwd_path);
        if (n > 1 && ctx->cwd_path[n - 1] != '/') {
            if (n + 1 < VFS_CWD_PATH_MAX) {
                ctx->cwd_path[n++] = '/';
                ctx->cwd_path[n] = '\0';
            }
        }
        if (n + s_len(path) + 1 < VFS_CWD_PATH_MAX) {
            u32 i;
            for (i = 0; path[i]; i++) {
                ctx->cwd_path[n + i] = path[i];
            }
            ctx->cwd_path[n + i] = '\0';
        }
    }

    if (!ctx->cwd_path[0]) {
        cwd_update(ctx, "/");
    }

    return 1;
}

u32 vfs_list(const char* path, sfs_node_info* out_entries, u32 max_entries) {
    u32 id;
    sfs_node_info st;
    vfs_terminal_context* ctx = vfs_current_context();

    if (!path || !path[0]) {
        id = ctx->cwd;
    } else if (!sfs_resolve(path, ctx->cwd, &id)) {
        return 0;
    }

    if (!sfs_stat(id, &st) || st.type != SFS_TYPE_DIR) {
        return 0;
    }

    return sfs_list(id, out_entries, max_entries);
}

u8 vfs_stat_path(const char* path, sfs_node_info* out_info) {
    u32 id;
    vfs_terminal_context* ctx = vfs_current_context();

    if (!sfs_resolve(path, ctx->cwd, &id)) {
        return 0;
    }

    return sfs_stat(id, out_info);
}

const char* vfs_get_cwd(void) {
    return vfs_current_context()->cwd_path;
}

u8 vfs_setenv(const char* key, const char* value) {
    vfs_terminal_context* ctx = vfs_current_context();
    s32 slot;

    if (!env_key_valid(key) || !value) {
        return 0;
    }

    slot = env_find_slot(ctx, key);
    if (slot < 0) {
        slot = env_find_free_slot(ctx);
    }
    if (slot < 0) {
        return 0;
    }

    ctx->env_used[slot] = 1u;
    s_copy(ctx->env_keys[slot], key, VFS_ENV_KEY_MAX);
    s_copy(ctx->env_values[slot], value, VFS_ENV_VALUE_MAX);
    return 1;
}

const char* vfs_getenv(const char* key) {
    vfs_terminal_context* ctx = vfs_current_context();
    s32 slot = env_find_slot(ctx, key);

    if (slot < 0) {
        return 0;
    }
    return ctx->env_values[slot];
}

u8 vfs_unsetenv(const char* key) {
    vfs_terminal_context* ctx = vfs_current_context();
    s32 slot = env_find_slot(ctx, key);

    if (slot < 0) {
        return 0;
    }

    ctx->env_used[slot] = 0u;
    ctx->env_keys[slot][0] = '\0';
    ctx->env_values[slot][0] = '\0';
    return 1;
}
