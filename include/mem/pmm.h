#ifndef PMM_H
#define PMM_H

#include "bootinfo.h"
#include "types.h"

#define PAGE_SIZE 4096u

void pmm_init(
    const memory_region* regions,
    u32 region_count,
    u32 kernel_start,
    u32 kernel_end
);

u32 pmm_alloc_frame(void);
void pmm_free_frame(u32 physical_addr);

u32 pmm_total_memory_bytes(void);
u32 pmm_used_memory_bytes(void);
u32 pmm_free_memory_bytes(void);

#endif /* PMM_H */
