# Filesystem & VFS API

> Headers: `include/fs/vfs.h`, `include/fs/simplefs.h`  
> Sources: `src/fs/vfs.c`, `src/fs/simplefs.c`

All file I/O must go through the **VFS layer**.  Do not call SimpleFS
functions directly from new code; the VFS handles the file descriptor table
and the current working directory on top of SimpleFS.

---

## File descriptor lifecycle

```
fd = vfs_open(path, flags)   ← returns >= 0 on success, -1 on error
vfs_read(fd, buf, len)       ← sequential, advances internal offset
vfs_write(fd, data, len)     ← sequential, auto-grows file
vfs_close(fd)                ← MUST always be called
```

The descriptor table holds **16 simultaneous open files** (`VFS_MAX_FDS`).
Forgetting to call `vfs_close()` will eventually exhaust the table.

---

## Open flags

| Constant      | Value | Meaning |
|---------------|-------|---------|
| `VFS_O_READ`  | 0x01  | Open for reading |
| `VFS_O_WRITE` | 0x02  | Open for writing |
| `VFS_O_CREATE`| 0x04  | Create the file if it does not exist |
| `VFS_O_TRUNC` | 0x08  | Truncate to zero length on open |

Flags can be combined with `|`:

```c
/* Read an existing file */
s32 fd = vfs_open("/readme.txt", VFS_O_READ);

/* Create or overwrite a file */
s32 fd = vfs_open("/out.txt", VFS_O_WRITE | VFS_O_CREATE | VFS_O_TRUNC);
```

---

## VFS functions

### `vfs_init`

```c
u8 vfs_init(void);
```

Mount the filesystem.  Called once in `kmain()`.  Returns `1` on success.

---

### `vfs_open`

```c
s32 vfs_open(const char* path, u32 flags);
```

Open a file.  `path` may be absolute (`/bin/hello.elf`) or relative to the
current working directory (`notes.txt`).

**Returns** a non-negative fd on success, `-1` on error.

---

### `vfs_read`

```c
u32 vfs_read(s32 fd, void* out, u32 len);
```

Read up to `len` bytes from the current file position.  Advances the internal
offset.  **Returns** the number of bytes actually read (may be less near
end-of-file); returns `0` at EOF or on error.

---

### `vfs_write`

```c
u32 vfs_write(s32 fd, const void* data, u32 len);
```

Write `len` bytes starting at the current file offset.  Grows the file if
necessary.  **Returns** bytes actually written.

---

### `vfs_close`

```c
void vfs_close(s32 fd);
```

Release the file descriptor.  Always call this when done.

---

### `vfs_mkdir`

```c
u8 vfs_mkdir(const char* path);
```

Create a new directory.  **Returns** `1` on success, `0` if the path already
exists or the parent directory does not exist.

---

### `vfs_remove`

```c
u8 vfs_remove(const char* path);
```

Delete a file or **empty** directory.  Returns `1` on success.

---

### `vfs_chdir`

```c
u8 vfs_chdir(const char* path);
```

Change the current working directory.  Returns `1` on success.

---

### `vfs_list`

```c
u32 vfs_list(const char* path, sfs_node_info* out_entries, u32 max_entries);
```

Enumerate directory contents.  Returns the number of entries written.

```c
sfs_node_info entries[32];
u32 n = vfs_list("/bin", entries, 32);
for (u32 i = 0; i < n; i++) {
    display_print(entries[i].name);
    display_put_char('\n');
}
```

---

### `vfs_stat_path`

```c
u8 vfs_stat_path(const char* path, sfs_node_info* out_info);
```

Query metadata without opening the file.  Returns `1` if the path exists.

---

### `vfs_get_cwd`

```c
const char* vfs_get_cwd(void);
```

Returns the absolute path of the current working directory as a read-only
string owned by the VFS (do not free).

---

## `sfs_node_info` — node metadata

```c
typedef struct {
    u32 id;       /* Unique node ID                            */
    u8  type;     /* SFS_TYPE_FILE (1) or SFS_TYPE_DIR (2)    */
    u8  perm;     /* Bitmask of SFS_PERM_* flags               */
    u32 parent;   /* Node ID of the parent directory           */
    u32 size;     /* File size in bytes (0 for directories)    */
    char name[32];/* Null-terminated entry name                */
} sfs_node_info;
```

### Permission flags

| Constant        | Value | Meaning |
|-----------------|-------|---------|
| `SFS_PERM_READ` | 0x1   | Readable |
| `SFS_PERM_WRITE`| 0x2   | Writable |
| `SFS_PERM_EXEC` | 0x4   | Executable via `exec` |

---

## Complete example — writing and reading a file

```c
#include "vfs.h"
#include "display.h"

void save_message(void) {
    const char* text = "Hello from VertexOS";
    u32 len = 19;
    s32 fd;

    /* Write */
    fd = vfs_open("/msg.txt", VFS_O_WRITE | VFS_O_CREATE | VFS_O_TRUNC);
    if (fd < 0) { display_print("open failed\n"); return; }
    vfs_write(fd, text, len);
    vfs_close(fd);

    /* Read back */
    u8 buf[64];
    fd = vfs_open("/msg.txt", VFS_O_READ);
    if (fd < 0) { display_print("open failed\n"); return; }
    u32 n = vfs_read(fd, buf, sizeof(buf) - 1);
    buf[n] = '\0';
    vfs_close(fd);

    display_print((char*)buf);
    display_put_char('\n');
}
```
