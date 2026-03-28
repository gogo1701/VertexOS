#ifndef SIMPLEFS_H
#define SIMPLEFS_H

#include "types.h"

#define SFS_TYPE_FILE 1u
#define SFS_TYPE_DIR  2u

#define SFS_PERM_READ  0x1u
#define SFS_PERM_WRITE 0x2u
#define SFS_PERM_EXEC  0x4u

typedef struct {
    u32 id;
    u8 type;
    u8 perm;
    u32 parent;
    u32 size;
    char name[32];
} sfs_node_info;

u8 sfs_init(void);
u32 sfs_root_id(void);

u8 sfs_resolve(const char* path, u32 cwd_id, u32* out_id);
u8 sfs_create(const char* path, u32 cwd_id, u8 type, u8 perm, u32* out_id);
u8 sfs_remove(const char* path, u32 cwd_id);

u8 sfs_read(u32 node_id, u32 offset, void* out, u32 len, u32* out_read);
u8 sfs_write(u32 node_id, u32 offset, const void* data, u32 len, u32* out_written);
u8 sfs_truncate(u32 node_id);

u8 sfs_stat(u32 node_id, sfs_node_info* out_info);
u32 sfs_list(u32 dir_id, sfs_node_info* out_entries, u32 max_entries);

#endif /* SIMPLEFS_H */
