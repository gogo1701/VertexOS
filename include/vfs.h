#ifndef VFS_H
#define VFS_H

#include "simplefs.h"
#include "types.h"

#define VFS_O_READ   0x01u
#define VFS_O_WRITE  0x02u
#define VFS_O_CREATE 0x04u
#define VFS_O_TRUNC  0x08u

#define VFS_MAX_FDS 16

typedef struct {
    u8 used;
    u32 node_id;
    u32 offset;
    u32 flags;
} vfs_fd;

u8 vfs_init(void);

s32 vfs_open(const char* path, u32 flags);
u32 vfs_read(s32 fd, void* out, u32 len);
u32 vfs_write(s32 fd, const void* data, u32 len);
void vfs_close(s32 fd);

u8 vfs_mkdir(const char* path);
u8 vfs_remove(const char* path);
u8 vfs_chdir(const char* path);

u32 vfs_list(const char* path, sfs_node_info* out_entries, u32 max_entries);
u8 vfs_stat_path(const char* path, sfs_node_info* out_info);

const char* vfs_get_cwd(void);

#endif /* VFS_H */
