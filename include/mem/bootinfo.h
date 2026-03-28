#ifndef BOOTINFO_H
#define BOOTINFO_H

#include "types.h"

#define E820_MAX_ENTRIES 32
#define E820_TYPE_USABLE 1

typedef struct {
    u32 base_low;
    u32 base_high;
    u32 length_low;
    u32 length_high;
    u32 type;
} __attribute__((packed)) e820_entry;

typedef struct {
    u32 base;
    u32 length;
} memory_region;

/* Parse and validate BIOS E820 map into usable 32-bit memory regions. */
u32 bootinfo_get_usable_regions(
    const e820_entry* entries,
    u32 entry_count,
    memory_region* out_regions,
    u32 max_regions
);

#endif /* BOOTINFO_H */
