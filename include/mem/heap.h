/*
 * Kernel Heap Allocator
 *
 * A simple first-fit heap used for all dynamic kernel allocations.
 * The heap arena is a contiguous region of physical memory initialised
 * once by heap_init().
 *
 * These are the only allocation primitives in the kernel; do not perform
 * raw pointer arithmetic or use global buffers for things that vary in
 * size at runtime.
 *
 * Thread safety: The allocator is NOT thread-safe.  Callers that share
 * the heap across tasks must disable interrupts around alloc/free pairs,
 * or use a higher-level synchronisation mechanism.
 */

#ifndef HEAP_H
#define HEAP_H

#include "types.h"

/*
 * heap_init - Initialise the heap arena.
 *
 * Must be called once during kernel boot, after paging is set up.
 * The entire [heap_start, heap_start+heap_size) range must be identity-
 * mapped and not overlap with the kernel image or PMM bitmaps.
 *
 * @heap_start: Physical (and virtual) base address of the heap region.
 * @heap_size:  Total bytes available to the allocator.
 */
void heap_init(u32 heap_start, u32 heap_size);

/*
 * kmalloc - Allocate size bytes from the kernel heap.
 *
 * Allocations are 4-byte aligned.  A small header is prepended by the
 * allocator and is not counted in the returned size.
 *
 * @size: Number of bytes to allocate.
 *
 * @return: Pointer to the allocated block, or NULL if the heap is
 *          exhausted or size is 0.
 *
 * Example:
 *   u8* buf = (u8*)kmalloc(512);
 *   if (!buf) { panic("out of memory"); }
 */
void* kmalloc(u32 size);

/*
 * kfree - Return a previously allocated block to the heap.
 *
 * Passing a NULL pointer is safe and has no effect.  Passing a pointer
 * that was not returned by kmalloc results in undefined behaviour.
 *
 * @ptr: Pointer previously returned by kmalloc().
 */
void kfree(void* ptr);

/*
 * heap_total_bytes - Return the total heap arena size in bytes.
 */
u32 heap_total_bytes(void);

/*
 * heap_used_bytes - Return the number of bytes currently allocated.
 *
 * Includes allocator header overhead.  Useful for memory-usage diagnostics.
 */
u32 heap_used_bytes(void);

#endif /* HEAP_H */
