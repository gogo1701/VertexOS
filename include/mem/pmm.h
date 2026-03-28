/*
 * PMM — Physical Memory Manager
 *
 * Manages free physical memory using a bitmap where each bit represents
 * one 4 KiB page frame.  All allocations and frees are frame-granular.
 *
 * The kernel image pages and PMM bitmap itself are automatically marked
 * as used during pmm_init() so they can never be handed out.
 *
 * Typical usage:
 *   u32 frame = pmm_alloc_frame();     // get one 4 KiB physical page
 *   // ... use the page ...
 *   pmm_free_frame(frame);             // return it when done
 */

#ifndef PMM_H
#define PMM_H

#include "bootinfo.h"
#include "types.h"

#define PAGE_SIZE 4096u  /* Size of one physical page frame in bytes */

/*
 * pmm_init - Initialise the physical memory manager.
 *
 * Builds the page-frame bitmap from the E820 usable-memory regions,
 * then marks kernel image pages (kernel_start..kernel_end) as reserved.
 *
 * @regions:       Array of usable memory regions from bootinfo.
 * @region_count:  Number of entries in regions[].
 * @kernel_start:  Physical start of the kernel image (_kernel_start symbol).
 * @kernel_end:    Physical end of the kernel image (_kernel_end symbol).
 */
void pmm_init(
    const memory_region* regions,
    u32 region_count,
    u32 kernel_start,
    u32 kernel_end
);

/*
 * pmm_alloc_frame - Allocate one free 4 KiB physical page frame.
 *
 * Finds the lowest-numbered free frame in the bitmap, marks it as used,
 * and returns its physical base address.
 *
 * @return: Physical address of the allocated frame (always PAGE_SIZE-aligned),
 *          or 0 if no free frames remain.
 */
u32 pmm_alloc_frame(void);

/*
 * pmm_free_frame - Return a physical frame to the free pool.
 *
 * @physical_addr: Physical address previously returned by pmm_alloc_frame().
 *                 Must be PAGE_SIZE-aligned; passing an unaligned or
 *                 already-free address results in undefined behaviour.
 */
void pmm_free_frame(u32 physical_addr);

/*
 * pmm_total_memory_bytes - Return total managed physical memory in bytes.
 */
u32 pmm_total_memory_bytes(void);

/*
 * pmm_used_memory_bytes - Return bytes currently marked as used.
 */
u32 pmm_used_memory_bytes(void);

/*
 * pmm_free_memory_bytes - Return bytes currently available for allocation.
 */
u32 pmm_free_memory_bytes(void);

#endif /* PMM_H */
