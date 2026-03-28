/*
 * x86 Paging (4 KiB Pages, 32-bit)
 *
 * Sets up a page directory and page tables for the 32-bit kernel address
 * space.  The current implementation uses identity mapping (virtual
 * address == physical address) for simplicity.
 *
 * Page flags (can be OR-ed):
 *   0x01  Present
 *   0x02  Read/Write
 *   0x04  User / Supervisor (0 = kernel only, 1 = user accessible)
 */

#ifndef PAGING_H
#define PAGING_H

#include "types.h"

/*
 * paging_init_identity - Identity-map [0, limit_addr) and enable paging.
 *
 * Maps every 4 KiB page from address 0 up to (but not including)
 * limit_addr as present and read/write.  Enables CR0.PG before returning.
 *
 * @limit_addr: Upper bound of the identity-mapped region.  Must be page-
 *              aligned.  A value of 0x02000000 maps the first 32 MiB.
 */
void paging_init_identity(u32 limit_addr);

/*
 * paging_map_page - Map a single virtual page to a physical frame.
 *
 * @virtual_addr:  Virtual address of the page (must be 4 KiB aligned).
 * @physical_addr: Physical frame address (must be 4 KiB aligned).
 * @flags:         Page table entry flags (Present | RW | User bits).
 *
 * Does not flush the TLB; call paging_reload_directory() after mapping
 * if the change needs to be visible immediately.
 */
void paging_map_page(u32 virtual_addr, u32 physical_addr, u32 flags);

/*
 * paging_reload_directory - Flush the TLB by reloading CR3.
 *
 * Call after paging_map_page() to make new mappings visible.
 */
void paging_reload_directory(void);

#endif /* PAGING_H */
