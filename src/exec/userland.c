#include "userland.h"

#include "display.h"
#include "vfs.h"

extern const u8 _binary_build_user_hello_elf_start[];
extern const u8 _binary_build_user_hello_elf_end[];

void userland_seed_programs(void) {
    sfs_node_info st;
    s32 fd;
    const u8* data = _binary_build_user_hello_elf_start;
    u32 size = (u32)(_binary_build_user_hello_elf_end - _binary_build_user_hello_elf_start);
    u32 written;

    if (!vfs_stat_path("/bin", &st)) {
        (void)vfs_mkdir("/bin");
    }

    if (vfs_stat_path("/bin/hello.elf", &st)) {
        return;
    }

    fd = vfs_open("/bin/hello.elf", VFS_O_WRITE | VFS_O_CREATE | VFS_O_TRUNC);
    if (fd < 0) {
        display_print("userland: failed to create /bin/hello.elf\n");
        return;
    }

    written = vfs_write(fd, data, size);
    vfs_close(fd);

    if (written != size) {
        display_print("userland: short write seeding hello.elf\n");
    }
}
