/*
 * ELF Program Loader
 *
 * Loads and executes a 32-bit ELF executable from the VFS.  Only
 * statically-linked 32-bit (EM_386) ELF executables are supported.
 *
 * Load process:
 *   1. Open the file from VFS and read the ELF header.
 *   2. Validate the ELF magic, class (32-bit), and machine (i386).
 *   3. Iterate program headers; allocate heap memory and copy every
 *      PT_LOAD segment.
 *   4. Jump to the ELF entry point and execute until it issues a
 *      SYS_YIELD or the function returns.
 *
 * Limitations:
 *   - Dynamic linking is not supported.
 *   - The loaded program runs in kernel privilege (Ring 0) for now.
 *   - Program memory is allocated from the kernel heap and is not
 *     reclaimed if the program panics or returns unexpectedly.
 */

#ifndef EXEC_H
#define EXEC_H

#include "types.h"

/*
 * exec_run_elf - Load and run a 32-bit ELF executable from disk.
 *
 * @path: VFS path to the ELF file (e.g. "/bin/hello.elf").
 *
 * @return: 1 if the program was loaded and ran to completion (i.e. its
 *          entry function returned), 0 on any load or validation error.
 *
 * Example:
 *   if (!exec_run_elf("/bin/myprog.elf")) {
 *       display_print("exec: failed to run program\n");
 *   }
 */
u8 exec_run_elf(const char* path);

#endif /* EXEC_H */
