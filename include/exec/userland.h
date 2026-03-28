/*
 * Userland Program Seeder
 *
 * Seeds built-in userland programs onto the VFS at first boot so they
 * are immediately available under /bin/ without any external loading step.
 *
 * Currently seeded programs:
 *   /bin/hello.elf  - minimal "Hello, World" ELF demo built from user/hello.c
 *
 * The binary data for each program is embedded into the kernel image at
 * link time (via objcopy) and copied to the filesystem by this module.
 */

#ifndef USERLAND_H
#define USERLAND_H

/*
 * userland_seed_programs - Copy built-in ELF programs to /bin/ on the VFS.
 *
 * Called once at the end of kmain() after vfs_init().  Skips any program
 * whose destination path already exists so re-seeding on subsequent boots
 * is safe and non-destructive.
 */
void userland_seed_programs(void);

#endif /* USERLAND_H */
