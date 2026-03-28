#include "vfs.h"
#include "blockdev.h"

static vfs_fd g_fds[VFS_MAX_FDS];
static u32 g_cwd = 0;
static char g_cwd_path[128];

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

static void cwd_update(const char* path) {
    s_copy(g_cwd_path, path, sizeof(g_cwd_path));
}

u8 vfs_init(void) {
    u32 i;

    blockdev_init();
    if (!sfs_init()) {
        return 0;
    }

    for (i = 0; i < VFS_MAX_FDS; i++) {
        g_fds[i].used = 0;
    }

    g_cwd = sfs_root_id();
    cwd_update("/");
    return 1;
}

s32 vfs_open(const char* path, u32 flags) {
    u32 node_id;
    u32 i;

    if (!sfs_resolve(path, g_cwd, &node_id)) {
        if (flags & VFS_O_CREATE) {
            if (!sfs_create(path, g_cwd, SFS_TYPE_FILE, (u8)(SFS_PERM_READ | SFS_PERM_WRITE), &node_id)) {
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
    u32 id;
    return sfs_create(path, g_cwd, SFS_TYPE_DIR, (u8)(SFS_PERM_READ | SFS_PERM_WRITE | SFS_PERM_EXEC), &id);
}

u8 vfs_remove(const char* path) {
    return sfs_remove(path, g_cwd);
}

u8 vfs_chdir(const char* path) {
    u32 id;
    sfs_node_info st;

    if (!sfs_resolve(path, g_cwd, &id)) {
        return 0;
    }

    if (!sfs_stat(id, &st) || st.type != SFS_TYPE_DIR) {
        return 0;
    }

    g_cwd = id;

    if (path[0] == '/') {
        cwd_update(path);
    } else if (path[0] == '.' && path[1] == '\0') {
        (void)0;
    } else if (path[0] == '.' && path[1] == '.' && path[2] == '\0') {
        if (s_len(g_cwd_path) > 1) {
            s32 i = (s32)s_len(g_cwd_path) - 1;
            while (i > 0 && g_cwd_path[i] == '/') {
                g_cwd_path[i--] = '\0';
            }
            while (i > 0 && g_cwd_path[i] != '/') {
                g_cwd_path[i--] = '\0';
            }
            if (i == 0) {
                g_cwd_path[1] = '\0';
            } else {
                g_cwd_path[i] = '\0';
            }
        }
    } else {
        u32 n = s_len(g_cwd_path);
        if (n > 1 && g_cwd_path[n - 1] != '/') {
            if (n + 1 < sizeof(g_cwd_path)) {
                g_cwd_path[n++] = '/';
                g_cwd_path[n] = '\0';
            }
        }
        if (n + s_len(path) + 1 < sizeof(g_cwd_path)) {
            u32 i;
            for (i = 0; path[i]; i++) {
                g_cwd_path[n + i] = path[i];
            }
            g_cwd_path[n + i] = '\0';
        }
    }

    if (!g_cwd_path[0]) {
        cwd_update("/");
    }

    return 1;
}

u32 vfs_list(const char* path, sfs_node_info* out_entries, u32 max_entries) {
    u32 id;
    sfs_node_info st;

    if (!path || !path[0]) {
        id = g_cwd;
    } else if (!sfs_resolve(path, g_cwd, &id)) {
        return 0;
    }

    if (!sfs_stat(id, &st) || st.type != SFS_TYPE_DIR) {
        return 0;
    }

    return sfs_list(id, out_entries, max_entries);
}

u8 vfs_stat_path(const char* path, sfs_node_info* out_info) {
    u32 id;

    if (!sfs_resolve(path, g_cwd, &id)) {
        return 0;
    }

    return sfs_stat(id, out_info);
}

const char* vfs_get_cwd(void) {
    return g_cwd_path;
}
