#include "bootinfo.h"

u32 bootinfo_get_usable_regions(
    const e820_entry* entries,
    u32 entry_count,
    memory_region* out_regions,
    u32 max_regions
) {
    u32 i;
    u32 out_count = 0;

    if (!entries || !out_regions || max_regions == 0) {
        return 0;
    }

    if (entry_count > E820_MAX_ENTRIES) {
        entry_count = E820_MAX_ENTRIES;
    }

    for (i = 0; i < entry_count; i++) {
        u64 base;
        u64 length;
        u64 end;

        if (entries[i].type != E820_TYPE_USABLE) {
            continue;
        }

        base = ((u64)entries[i].base_high << 32) | entries[i].base_low;
        length = ((u64)entries[i].length_high << 32) | entries[i].length_low;

        if (length == 0) {
            continue;
        }

        end = base + length;
        if (end <= base) {
            continue;
        }

        /* This kernel currently uses 32-bit addresses only. */
        if (base >= 0x100000000ULL) {
            continue;
        }

        if (end > 0x100000000ULL) {
            end = 0x100000000ULL;
        }

        if (out_count < max_regions) {
            out_regions[out_count].base = (u32)base;
            out_regions[out_count].length = (u32)(end - base);
            out_count++;
        }
    }

    return out_count;
}
