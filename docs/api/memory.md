# Memory Management API

> Headers: `include/mem/heap.h`, `include/mem/pmm.h`, `include/mem/paging.h`, `include/mem/bootinfo.h`  
> Sources: `src/mem/`

---

## Heap allocator (kmalloc / kfree)

This is the **only** dynamic memory API you should use in kernel code.

```c
#include "heap.h"

void* ptr = kmalloc(size);   /* allocate */
kfree(ptr);                  /* free      */
```

### `kmalloc`

```c
void* kmalloc(u32 size);
```

Allocate `size` bytes from the kernel heap.  Returns a 4-byte-aligned
pointer, or `NULL` if the heap is exhausted or `size == 0`.

Always check the return value:

```c
u8* buf = (u8*)kmalloc(256);
if (!buf) {
    panic("out of memory");
}
```

---

### `kfree`

```c
void kfree(void* ptr);
```

Return an allocation to the heap.  Passing `NULL` is safe.  Do not free
the same pointer twice and do not pass a pointer that was not returned by
`kmalloc`.

---

### Diagnostic helpers

```c
u32 heap_total_bytes(void);   /* total arena size               */
u32 heap_used_bytes(void);    /* bytes currently allocated      */
```

```c
display_print("Heap: ");
display_print_num(heap_used_bytes() / 1024, 10);
display_print(" / ");
display_print_num(heap_total_bytes() / 1024, 10);
display_print(" KiB used\n");
```

---

## Physical Memory Manager (PMM)

Manages raw 4 KiB physical page frames.  **Prefer `kmalloc` for general
allocations.**  Use the PMM directly only when you need a whole page-aligned
physical frame (e.g. for page tables).

```c
#include "pmm.h"

#define PAGE_SIZE 4096u

u32 frame = pmm_alloc_frame();   /* returns physical address or 0 */
pmm_free_frame(frame);
```

### Allocation

```c
u32 pmm_alloc_frame(void);
```

Returns the physical base address of one free 4 KiB frame, or `0` if
no frames are available.

```c
void pmm_free_frame(u32 physical_addr);
```

Returns a frame to the free pool.  `physical_addr` must be the value
originally returned by `pmm_alloc_frame()`.

### Statistics

```c
u32 pmm_total_memory_bytes(void);
u32 pmm_used_memory_bytes(void);
u32 pmm_free_memory_bytes(void);
```

---

## Paging

Sets up 32-bit identity-mapped paging (`virtual == physical`).  Normally
you do not need to call these after boot.

```c
#include "paging.h"

/* Add a new mapping (e.g. to map device MMIO) */
paging_map_page(virtual_addr, physical_addr, 0x03 /* Present | RW */);
paging_reload_directory();   /* flush TLB */
```

### Page flags

| Bit | Meaning |
|-----|---------|
| 0x01 | Present |
| 0x02 | Read/Write |
| 0x04 | User-accessible (0 = kernel only) |

---

## Boot memory map (E820)

The `bootinfo` module is used only during early boot to convert the raw
BIOS E820 table into a clean list of usable memory regions for the PMM.
You will not need this in normal development.

```c
memory_region regions[E820_MAX_ENTRIES];
u32 count = bootinfo_get_usable_regions(e820_map, e820_count,
                                         regions, E820_MAX_ENTRIES);
pmm_init(regions, count, kernel_start, kernel_end);
```
