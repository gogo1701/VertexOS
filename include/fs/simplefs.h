/*
 * SimpleFS — On-Disk Filesystem Implementation
 *
 * A flat, single-level node-based filesystem stored on the block device.
 * SimpleFS is the low-level on-disk layer; higher-level code should use
 * the VFS API (vfs.h) rather than calling SimpleFS directly.
 *
 * Layout on disk (each unit = 512-byte sector):
 *   Sector   0        : boot/config sector (managed by bootloader)
 *   Sectors  1-127    : reserved
 *   Sectors  128+     : filesystem metadata and file data
 *
 * Permissions (sfs_node_info::perm bitmask):
 *   SFS_PERM_READ  0x1 - node is readable
 *   SFS_PERM_WRITE 0x2 - node is writable
 *   SFS_PERM_EXEC  0x4 - node can be executed via exec()
 *
 * Node types:
 *   SFS_TYPE_FILE 1 - regular file
 *   SFS_TYPE_DIR  2 - directory
 */

#ifndef SIMPLEFS_H
#define SIMPLEFS_H

#include "types.h"

#define SFS_TYPE_FILE 1u
#define SFS_TYPE_DIR  2u

#define SFS_PERM_READ  0x1u
#define SFS_PERM_WRITE 0x2u
#define SFS_PERM_EXEC  0x4u

/*
 * sfs_node_info - Metadata snapshot for a single filesystem node.
 *
 * @id:     Unique numeric node ID.
 * @type:   SFS_TYPE_FILE or SFS_TYPE_DIR.
 * @perm:   Permission bitmask (SFS_PERM_* flags).
 * @parent: Node ID of the parent directory.
 * @size:   File size in bytes (0 for directories).
 * @name:   Null-terminated entry name (up to 31 characters).
 */
typedef struct {
    u32 id;
    u8 type;
    u8 perm;
    u32 parent;
    u32 size;
    char name[32];
} sfs_node_info;

/*
 * sfs_init - Scan the block device and mount the filesystem.
 *
 * If no filesystem signature is found the disk is formatted automatically.
 *
 * @return: 1 on success, 0 on failure (block device not ready).
 */
u8 sfs_init(void);

/*
 * sfs_root_id - Return the node ID of the root directory.
 *
 * The root ID is always valid after a successful sfs_init().
 */
u32 sfs_root_id(void);

/*
 * sfs_resolve - Resolve a path string to a node ID.
 *
 * Supports absolute paths (/a/b/c) and relative paths (a/b/c).
 * "." and ".." are not currently supported.
 *
 * @path:   Null-terminated path to resolve.
 * @cwd_id: Node ID of the current working directory (for relative paths).
 * @out_id: Receives the resolved node ID on success.
 *
 * @return: 1 if the path resolves to an existing node, 0 otherwise.
 */
u8 sfs_resolve(const char* path, u32 cwd_id, u32* out_id);

/*
 * sfs_create - Create a new file or directory node.
 *
 * @path:   Path at which to create the node.
 * @cwd_id: Node ID of the current working directory.
 * @type:   SFS_TYPE_FILE or SFS_TYPE_DIR.
 * @perm:   Permission bitmask for the new node.
 * @out_id: Receives the new node's ID on success.
 *
 * @return: 1 on success, 0 if the path exists already or the disk is full.
 */
u8 sfs_create(const char* path, u32 cwd_id, u8 type, u8 perm, u32* out_id);

/*
 * sfs_remove - Delete a node by path.
 *
 * Files are removed immediately.  Directories must be empty.
 *
 * @path:   Path to the node to remove.
 * @cwd_id: Node ID of the current working directory.
 *
 * @return: 1 on success, 0 if not found or directory is non-empty.
 */
u8 sfs_remove(const char* path, u32 cwd_id);

/*
 * sfs_read - Read bytes from an open file node.
 *
 * @node_id:   Node ID of the file to read.
 * @offset:    Byte offset within the file to start reading from.
 * @out:       Caller-allocated buffer to receive the data.
 * @len:       Maximum number of bytes to read.
 * @out_read:  Receives the actual number of bytes read (may be less
 *             than len near end-of-file).
 *
 * @return: 1 on success, 0 if node_id is invalid or not a file.
 */
u8 sfs_read(u32 node_id, u32 offset, void* out, u32 len, u32* out_read);

/*
 * sfs_write - Write bytes to a file node, growing it if necessary.
 *
 * @node_id:     Node ID of the target file.
 * @offset:      Byte offset to start writing at.
 * @data:        Data to write.
 * @len:         Number of bytes to write.
 * @out_written: Receives the number of bytes actually written.
 *
 * @return: 1 on success, 0 on error.
 */
u8 sfs_write(u32 node_id, u32 offset, const void* data, u32 len, u32* out_written);

/*
 * sfs_truncate - Set the file size to zero and free all data blocks.
 *
 * @node_id: Node ID of the file to truncate.
 *
 * @return: 1 on success, 0 if the node does not exist or is a directory.
 */
u8 sfs_truncate(u32 node_id);

/*
 * sfs_stat - Fill an sfs_node_info struct for the given node.
 *
 * @node_id:  Node to query.
 * @out_info: Caller-allocated struct to fill.
 *
 * @return: 1 on success, 0 if node_id is invalid.
 */
u8 sfs_stat(u32 node_id, sfs_node_info* out_info);

/*
 * sfs_list - Enumerate the children of a directory.
 *
 * @dir_id:      Node ID of the directory to list.
 * @out_entries: Caller-allocated array to receive node info structs.
 * @max_entries: Capacity of out_entries.
 *
 * @return: Number of entries written into out_entries (0 if empty).
 */
u32 sfs_list(u32 dir_id, sfs_node_info* out_entries, u32 max_entries);

#endif /* SIMPLEFS_H */
