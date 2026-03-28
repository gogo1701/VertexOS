#include "simplefs.h"
#include "blockdev.h"

#define SFS_MAGIC 0x31534653u
#define SFS_VERSION 1u

#define SFS_START_LBA 128u
#define SFS_SUPER_LBA (SFS_START_LBA)
#define SFS_TABLE_LBA (SFS_START_LBA + 1u)
#define SFS_TABLE_SECTORS 8u
#define SFS_DATA_LBA (SFS_TABLE_LBA + SFS_TABLE_SECTORS)
#define SFS_MAX_ENTRIES 64u

#define SFS_NAME_MAX 31u

typedef struct {
    u32 magic;
    u32 version;
    u32 total_sectors;
    u32 max_entries;
    u32 next_free_lba;
    u32 root_id;
    u8 reserved[512 - 24];
} __attribute__((packed)) sfs_superblock;

typedef struct {
    u8 used;
    u8 type;
    u8 perm;
    u8 reserved0;
    u32 parent;
    u32 start_lba;
    u32 size;
    u32 capacity_sectors;
    char name[32];
    u8 reserved1[12];
} __attribute__((packed)) sfs_entry;

static sfs_superblock g_super;
static sfs_entry g_entries[SFS_MAX_ENTRIES];

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

static u8 s_eq(const char* a, const char* b) {
    u32 i = 0;
    while (a[i] && b[i]) {
        if (a[i] != b[i]) {
            return 0;
        }
        i++;
    }
    return a[i] == b[i];
}

static void mem_zero(void* p, u32 n) {
    u8* b = (u8*)p;
    u32 i;
    for (i = 0; i < n; i++) {
        b[i] = 0;
    }
}

static void mem_copy(void* dst, const void* src, u32 n) {
    u8* d = (u8*)dst;
    const u8* s = (const u8*)src;
    u32 i;
    for (i = 0; i < n; i++) {
        d[i] = s[i];
    }
}

static u8 sfs_flush(void) {
    u32 i;
    u8 sector[512];

    if (!blockdev_write(SFS_SUPER_LBA, &g_super)) {
        return 0;
    }

    for (i = 0; i < SFS_TABLE_SECTORS; i++) {
        mem_copy(sector, ((u8*)g_entries) + i * 512u, 512u);
        if (!blockdev_write(SFS_TABLE_LBA + i, sector)) {
            return 0;
        }
    }

    return 1;
}

static u8 sfs_load(void) {
    u32 i;
    u8 sector[512];

    if (!blockdev_read(SFS_SUPER_LBA, &g_super)) {
        return 0;
    }

    for (i = 0; i < SFS_TABLE_SECTORS; i++) {
        if (!blockdev_read(SFS_TABLE_LBA + i, sector)) {
            return 0;
        }
        mem_copy(((u8*)g_entries) + i * 512u, sector, 512u);
    }

    return 1;
}

static s32 sfs_find_free_entry(void) {
    u32 i;
    for (i = 0; i < SFS_MAX_ENTRIES; i++) {
        if (!g_entries[i].used) {
            return (s32)i;
        }
    }
    return -1;
}

static s32 sfs_find_child(u32 parent, const char* name) {
    u32 i;
    for (i = 0; i < SFS_MAX_ENTRIES; i++) {
        if (g_entries[i].used && g_entries[i].parent == parent && s_eq(g_entries[i].name, name)) {
            return (s32)i;
        }
    }
    return -1;
}

static u8 path_next(const char* path, u32* pos, char* out_name) {
    u32 i = 0;

    while (path[*pos] == '/') {
        (*pos)++;
    }

    if (!path[*pos]) {
        return 0;
    }

    while (path[*pos] && path[*pos] != '/' && i < SFS_NAME_MAX) {
        out_name[i++] = path[*pos];
        (*pos)++;
    }
    out_name[i] = '\0';

    while (path[*pos] && path[*pos] != '/') {
        (*pos)++;
    }

    return 1;
}

u8 sfs_init(void) {
    s32 root_idx;

    if (!sfs_load() || g_super.magic != SFS_MAGIC || g_super.version != SFS_VERSION) {
        mem_zero(&g_super, sizeof(g_super));
        mem_zero(g_entries, sizeof(g_entries));

        g_super.magic = SFS_MAGIC;
        g_super.version = SFS_VERSION;
        g_super.total_sectors = 65536;
        g_super.max_entries = SFS_MAX_ENTRIES;
        g_super.next_free_lba = SFS_DATA_LBA;
        g_super.root_id = 0;

        root_idx = sfs_find_free_entry();
        if (root_idx < 0) {
            return 0;
        }

        g_entries[root_idx].used = 1;
        g_entries[root_idx].type = SFS_TYPE_DIR;
        g_entries[root_idx].perm = (u8)(SFS_PERM_READ | SFS_PERM_WRITE | SFS_PERM_EXEC);
        g_entries[root_idx].parent = (u32)root_idx;
        g_entries[root_idx].start_lba = 0;
        g_entries[root_idx].size = 0;
        g_entries[root_idx].capacity_sectors = 0;
        s_copy(g_entries[root_idx].name, "/", sizeof(g_entries[root_idx].name));

        return sfs_flush();
    }

    return 1;
}

u32 sfs_root_id(void) {
    return g_super.root_id;
}

u8 sfs_resolve(const char* path, u32 cwd_id, u32* out_id) {
    u32 current;
    u32 pos = 0;
    char name[32];

    if (!path || !out_id) {
        return 0;
    }

    if (path[0] == '/') {
        current = g_super.root_id;
    } else {
        current = cwd_id;
    }

    while (path_next(path, &pos, name)) {
        s32 child;

        if (s_eq(name, ".")) {
            continue;
        }
        if (s_eq(name, "..")) {
            current = g_entries[current].parent;
            continue;
        }

        child = sfs_find_child(current, name);
        if (child < 0) {
            return 0;
        }
        current = (u32)child;
    }

    *out_id = current;
    return 1;
}

u8 sfs_create(const char* path, u32 cwd_id, u8 type, u8 perm, u32* out_id) {
    u32 pos = 0;
    u32 current;
    char name[32];
    char last[32];
    s32 idx;

    if (!path || !path[0]) {
        return 0;
    }

    if (path[0] == '/') {
        current = g_super.root_id;
    } else {
        current = cwd_id;
    }

    last[0] = '\0';

    while (path_next(path, &pos, name)) {
        s32 child;
        s_copy(last, name, sizeof(last));

        if (s_eq(name, ".")) {
            continue;
        }
        if (s_eq(name, "..")) {
            current = g_entries[current].parent;
            continue;
        }

        if (!path[pos]) {
            break;
        }

        child = sfs_find_child(current, name);
        if (child < 0 || g_entries[child].type != SFS_TYPE_DIR) {
            return 0;
        }
        current = (u32)child;
    }

    if (!last[0]) {
        return 0;
    }

    if (sfs_find_child(current, last) >= 0) {
        return 0;
    }

    idx = sfs_find_free_entry();
    if (idx < 0) {
        return 0;
    }

    g_entries[idx].used = 1;
    g_entries[idx].type = type;
    g_entries[idx].perm = perm;
    g_entries[idx].parent = current;
    g_entries[idx].start_lba = 0;
    g_entries[idx].size = 0;
    g_entries[idx].capacity_sectors = 0;
    s_copy(g_entries[idx].name, last, sizeof(g_entries[idx].name));

    if (!sfs_flush()) {
        return 0;
    }

    if (out_id) {
        *out_id = (u32)idx;
    }

    return 1;
}

u8 sfs_remove(const char* path, u32 cwd_id) {
    u32 id;
    u32 i;

    if (!sfs_resolve(path, cwd_id, &id)) {
        return 0;
    }

    if (id == g_super.root_id) {
        return 0;
    }

    if (g_entries[id].type == SFS_TYPE_DIR) {
        for (i = 0; i < SFS_MAX_ENTRIES; i++) {
            if (g_entries[i].used && g_entries[i].parent == id) {
                return 0;
            }
        }
    }

    mem_zero(&g_entries[id], sizeof(g_entries[id]));
    return sfs_flush();
}

u8 sfs_read(u32 node_id, u32 offset, void* out, u32 len, u32* out_read) {
    u32 remaining;
    u32 copied = 0;
    u8 sector[512];

    if (out_read) {
        *out_read = 0;
    }

    if (node_id >= SFS_MAX_ENTRIES || !g_entries[node_id].used || g_entries[node_id].type != SFS_TYPE_FILE) {
        return 0;
    }

    if (!(g_entries[node_id].perm & SFS_PERM_READ)) {
        return 0;
    }

    if (offset >= g_entries[node_id].size) {
        return 1;
    }

    remaining = g_entries[node_id].size - offset;
    if (len > remaining) {
        len = remaining;
    }

    while (copied < len) {
        u32 abs_off = offset + copied;
        u32 lba = g_entries[node_id].start_lba + abs_off / 512u;
        u32 in_sector = abs_off % 512u;
        u32 chunk = 512u - in_sector;
        if (chunk > len - copied) {
            chunk = len - copied;
        }

        if (!blockdev_read(lba, sector)) {
            return 0;
        }

        mem_copy(((u8*)out) + copied, sector + in_sector, chunk);
        copied += chunk;
    }

    if (out_read) {
        *out_read = copied;
    }

    return 1;
}

u8 sfs_write(u32 node_id, u32 offset, const void* data, u32 len, u32* out_written) {
    u32 end_pos;
    u32 need_sectors;
    u32 copied = 0;
    u8 sector[512];

    if (out_written) {
        *out_written = 0;
    }

    if (node_id >= SFS_MAX_ENTRIES || !g_entries[node_id].used || g_entries[node_id].type != SFS_TYPE_FILE) {
        return 0;
    }

    if (!(g_entries[node_id].perm & SFS_PERM_WRITE)) {
        return 0;
    }

    end_pos = offset + len;
    need_sectors = (end_pos + 511u) / 512u;

    if (g_entries[node_id].start_lba == 0) {
        g_entries[node_id].start_lba = g_super.next_free_lba;
        g_entries[node_id].capacity_sectors = need_sectors;
        g_super.next_free_lba += need_sectors;
    } else if (need_sectors > g_entries[node_id].capacity_sectors) {
        u32 new_start = g_super.next_free_lba;
        u32 i;
        u8 tmp[512];

        for (i = 0; i < g_entries[node_id].capacity_sectors; i++) {
            if (!blockdev_read(g_entries[node_id].start_lba + i, tmp)) {
                return 0;
            }
            if (!blockdev_write(new_start + i, tmp)) {
                return 0;
            }
        }

        g_entries[node_id].start_lba = new_start;
        g_entries[node_id].capacity_sectors = need_sectors;
        g_super.next_free_lba += need_sectors;
    }

    while (copied < len) {
        u32 abs_off = offset + copied;
        u32 lba = g_entries[node_id].start_lba + abs_off / 512u;
        u32 in_sector = abs_off % 512u;
        u32 chunk = 512u - in_sector;

        if (chunk > len - copied) {
            chunk = len - copied;
        }

        if (!blockdev_read(lba, sector)) {
            mem_zero(sector, sizeof(sector));
        }

        mem_copy(sector + in_sector, ((const u8*)data) + copied, chunk);

        if (!blockdev_write(lba, sector)) {
            return 0;
        }

        copied += chunk;
    }

    if (end_pos > g_entries[node_id].size) {
        g_entries[node_id].size = end_pos;
    }

    if (!sfs_flush()) {
        return 0;
    }

    if (out_written) {
        *out_written = copied;
    }

    return 1;
}

u8 sfs_truncate(u32 node_id) {
    if (node_id >= SFS_MAX_ENTRIES || !g_entries[node_id].used || g_entries[node_id].type != SFS_TYPE_FILE) {
        return 0;
    }

    if (!(g_entries[node_id].perm & SFS_PERM_WRITE)) {
        return 0;
    }

    g_entries[node_id].size = 0;
    if (!sfs_flush()) {
        return 0;
    }
    return 1;
}

u8 sfs_stat(u32 node_id, sfs_node_info* out_info) {
    if (!out_info || node_id >= SFS_MAX_ENTRIES || !g_entries[node_id].used) {
        return 0;
    }

    out_info->id = node_id;
    out_info->type = g_entries[node_id].type;
    out_info->perm = g_entries[node_id].perm;
    out_info->parent = g_entries[node_id].parent;
    out_info->size = g_entries[node_id].size;
    s_copy(out_info->name, g_entries[node_id].name, sizeof(out_info->name));
    return 1;
}

u32 sfs_list(u32 dir_id, sfs_node_info* out_entries, u32 max_entries) {
    u32 i;
    u32 count = 0;

    if (dir_id >= SFS_MAX_ENTRIES || !g_entries[dir_id].used || g_entries[dir_id].type != SFS_TYPE_DIR) {
        return 0;
    }

    for (i = 0; i < SFS_MAX_ENTRIES && count < max_entries; i++) {
        if (g_entries[i].used && g_entries[i].parent == dir_id && i != dir_id) {
            if (out_entries) {
                sfs_stat(i, &out_entries[count]);
            }
            count++;
        }
    }

    return count;
}
