/*
 * VFS — Virtual File System
 *
 * The VFS is the primary file I/O interface for all kernel code and user
 * programs.  It wraps SimpleFS with:
 *   - A file-descriptor table (up to VFS_MAX_FDS open files at once)
 *   - A per-process current working directory
 *   - Convenience helpers for directory operations
 *
 * Always use VFS functions rather than calling SimpleFS directly.
 *
 * Open flags (can be OR-ed together):
 *   VFS_O_READ   - open for reading
 *   VFS_O_WRITE  - open for writing
 *   VFS_O_CREATE - create the file if it does not exist
 *   VFS_O_TRUNC  - truncate the file to zero length on open
 *
 * File descriptor lifecycle:
 *   fd = vfs_open(path, flags)   -- returns >= 0 on success, < 0 on error
 *   vfs_read(fd, buf, len)       -- sequential read from current offset
 *   vfs_write(fd, data, len)     -- sequential write, advances offset
 *   vfs_close(fd)                -- must always be called when done
 */

#ifndef VFS_H
#define VFS_H

#include "simplefs.h"
#include "types.h"

#define VFS_O_READ   0x01u  /* Open for reading                */
#define VFS_O_WRITE  0x02u  /* Open for writing                */
#define VFS_O_CREATE 0x04u  /* Create if it does not exist     */
#define VFS_O_TRUNC  0x08u  /* Truncate to zero length on open */

#define VFS_MAX_FDS 16  /* Maximum simultaneously open file descriptors */

/*
 * vfs_fd - Internal file descriptor entry.
 *
 * Not intended for direct use by callers; exposed for completeness.
 *
 * @used:    1 if this slot is open, 0 if free.
 * @node_id: Underlying SimpleFS node ID.
 * @offset:  Current read/write byte position.
 * @flags:   VFS_O_* flags the file was opened with.
 */
typedef struct {
    u8 used;
    u32 node_id;
    u32 offset;
    u32 flags;
} vfs_fd;

/*
 * vfs_init - Mount the filesystem and prepare the descriptor table.
 *
 * Initialises the block device driver, the SimpleFS layer, and resets the
 * file descriptor table.  The current working directory is set to "/".
 *
 * @return: 1 on success, 0 if the block device or filesystem fails to mount.
 */
u8 vfs_init(void);

/*
 * vfs_open - Open a file and return a file descriptor.
 *
 * @path:  Absolute or CWD-relative path to the file.
 * @flags: OR combination of VFS_O_* flags.
 *
 * @return: Non-negative fd index on success, -1 on error
 *          (file not found, no CREATE flag, or fd table full).
 *
 * Example:
 *   s32 fd = vfs_open("/docs/notes.txt", VFS_O_READ);
 *   s32 fd = vfs_open("out.txt", VFS_O_WRITE | VFS_O_CREATE | VFS_O_TRUNC);
 */
s32 vfs_open(const char* path, u32 flags);

/*
 * vfs_read - Read bytes from an open file descriptor.
 *
 * Reads up to len bytes from the current file offset and advances the
 * offset by the number of bytes actually read.
 *
 * @fd:  File descriptor returned by vfs_open().
 * @out: Buffer to receive data.
 * @len: Maximum number of bytes to read.
 *
 * @return: Number of bytes read; 0 at end-of-file or on error.
 */
u32 vfs_read(s32 fd, void* out, u32 len);

/*
 * vfs_write - Write bytes to an open file descriptor.
 *
 * Advances the file offset by the number of bytes written.
 * The file is grown automatically if the write extends past the current end.
 *
 * @fd:   File descriptor returned by vfs_open() with VFS_O_WRITE.
 * @data: Data to write.
 * @len:  Number of bytes to write.
 *
 * @return: Number of bytes actually written; may be less than len on error.
 */
u32 vfs_write(s32 fd, const void* data, u32 len);

/*
 * vfs_close - Release a file descriptor.
 *
 * Must be called for every fd returned by vfs_open().  Failing to close
 * descriptors will exhaust the VFS_MAX_FDS table.
 *
 * @fd: File descriptor to close.
 */
void vfs_close(s32 fd);

/*
 * vfs_mkdir - Create a new directory.
 *
 * @path: Absolute or CWD-relative path for the new directory.
 *
 * @return: 1 on success, 0 if path already exists or the parent is missing.
 */
u8 vfs_mkdir(const char* path);

/*
 * vfs_remove - Delete a file or empty directory.
 *
 * @path: Path to the node to remove.
 *
 * @return: 1 on success, 0 if not found or directory is non-empty.
 */
u8 vfs_remove(const char* path);

/*
 * vfs_chdir - Change the current working directory.
 *
 * @path: Absolute or CWD-relative path to the target directory.
 *
 * @return: 1 on success, 0 if path does not exist or is not a directory.
 */
u8 vfs_chdir(const char* path);

/*
 * vfs_list - List the contents of a directory.
 *
 * @path:        Absolute or CWD-relative path to the directory.
 * @out_entries: Caller-allocated array of sfs_node_info to fill.
 * @max_entries: Capacity of the out_entries array.
 *
 * @return: Number of entries written into out_entries (0 if empty).
 */
u32 vfs_list(const char* path, sfs_node_info* out_entries, u32 max_entries);

/*
 * vfs_stat_path - Fill metadata for a path without opening it.
 *
 * @path:     Path to query.
 * @out_info: Caller-allocated struct to receive the metadata.
 *
 * @return: 1 on success, 0 if path does not exist.
 */
u8 vfs_stat_path(const char* path, sfs_node_info* out_info);

/*
 * vfs_get_cwd - Return the current working directory as an absolute path.
 *
 * @return: Pointer to a null-terminated string.  The buffer is owned by
 *          the VFS; do not modify or free it.
 */
const char* vfs_get_cwd(void);
u8 vfs_setenv(const char* key, const char* value);
const char* vfs_getenv(const char* key);
u8 vfs_unsetenv(const char* key);

#endif /* VFS_H */
