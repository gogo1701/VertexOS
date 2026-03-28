#include "pmm.h"
#include "panic.h"

#define PMM_MAX_MEMORY_BYTES (256u * 1024u * 1024u)
#define PMM_MAX_FRAMES (PMM_MAX_MEMORY_BYTES / PAGE_SIZE)
#define PMM_BITMAP_BYTES (PMM_MAX_FRAMES / 8u)

static u8 pmm_bitmap[PMM_BITMAP_BYTES];
static u32 pmm_total_bytes = 0;
static u32 pmm_used_bytes = 0;

static void bitmap_set(u32 frame) {
    pmm_bitmap[frame / 8u] |= (u8)(1u << (frame % 8u));
}

static void bitmap_clear(u32 frame) {
    pmm_bitmap[frame / 8u] &= (u8)~(1u << (frame % 8u));
}

static u8 bitmap_test(u32 frame) {
    return (pmm_bitmap[frame / 8u] >> (frame % 8u)) & 1u;
}

static void reserve_range(u32 start, u32 end) {
    u32 addr;

    start &= ~(PAGE_SIZE - 1u);
    end = (end + PAGE_SIZE - 1u) & ~(PAGE_SIZE - 1u);

    for (addr = start; addr < end; addr += PAGE_SIZE) {
        u32 frame = addr / PAGE_SIZE;
        if (frame < PMM_MAX_FRAMES && !bitmap_test(frame)) {
            bitmap_set(frame);
            pmm_used_bytes += PAGE_SIZE;
        }
    }
}

void pmm_init(
    const memory_region* regions,
    u32 region_count,
    u32 kernel_start,
    u32 kernel_end
) {
    u32 i;

    for (i = 0; i < PMM_BITMAP_BYTES; i++) {
        pmm_bitmap[i] = 0xFF;
    }

    pmm_total_bytes = 0;
    pmm_used_bytes = 0;

    for (i = 0; i < region_count; i++) {
        u32 region_base = regions[i].base;
        u32 region_end = region_base + regions[i].length;
        u32 addr;

        if (region_end <= region_base) {
            continue;
        }

        if (region_base >= PMM_MAX_MEMORY_BYTES) {
            continue;
        }

        if (region_end > PMM_MAX_MEMORY_BYTES) {
            region_end = PMM_MAX_MEMORY_BYTES;
        }

        for (addr = region_base; addr + PAGE_SIZE <= region_end; addr += PAGE_SIZE) {
            u32 frame = addr / PAGE_SIZE;
            if (bitmap_test(frame)) {
                bitmap_clear(frame);
                pmm_total_bytes += PAGE_SIZE;
            }
        }
    }

    /* Reserve low memory, kernel image, and known boot metadata area. */
    reserve_range(0x00000000u, 0x00100000u);
    reserve_range(kernel_start, kernel_end);
    reserve_range(0x00004F00u, 0x00006000u);

    if (pmm_total_bytes == 0) {
        panic("PMM initialization failed: no usable memory");
    }
}

u32 pmm_alloc_frame(void) {
    u32 frame;

    for (frame = 0; frame < PMM_MAX_FRAMES; frame++) {
        if (!bitmap_test(frame)) {
            bitmap_set(frame);
            pmm_used_bytes += PAGE_SIZE;
            return frame * PAGE_SIZE;
        }
    }

    panic("Out of physical memory frames");
    return 0;
}

void pmm_free_frame(u32 physical_addr) {
    u32 frame = physical_addr / PAGE_SIZE;

    if (frame >= PMM_MAX_FRAMES) {
        return;
    }

    if (bitmap_test(frame)) {
        bitmap_clear(frame);
        if (pmm_used_bytes >= PAGE_SIZE) {
            pmm_used_bytes -= PAGE_SIZE;
        }
    }
}

u32 pmm_total_memory_bytes(void) {
    return pmm_total_bytes;
}

u32 pmm_used_memory_bytes(void) {
    return pmm_used_bytes;
}

u32 pmm_free_memory_bytes(void) {
    return pmm_total_bytes - pmm_used_bytes;
}
