#include "bootinfo.h"

u32 bootinfo_multiboot_count = 0;
static e820_entry bootinfo_multiboot_map[E820_MAX_ENTRIES];

typedef struct {
    u32 size;
    u32 base_low;
    u32 base_high;
    u32 length_low;
    u32 length_high;
    u32 type;
} __attribute__((packed)) multiboot_mmap_entry;

typedef struct {
    u32 flags;
    u32 mem_lower;
    u32 mem_upper;
    u32 boot_device;
    u32 cmdline;
    u32 mods_count;
    u32 mods_addr;
    u32 syms[4];
    u32 mmap_length;
    u32 mmap_addr;
} __attribute__((packed)) multiboot_info_t;

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

const e820_entry* bootinfo_parse_multiboot(u32 mb_info_addr) {
    const multiboot_info_t* mbi = (const multiboot_info_t*)(u32)mb_info_addr;
    const u8* cursor;
    const u8* end;
    u32 count = 0;

    bootinfo_multiboot_count = 0;

    if (!mbi || !(mbi->flags & (1u << 6))) {
        return 0;
    }

    cursor = (const u8*)(u32)mbi->mmap_addr;
    end = cursor + mbi->mmap_length;

    while (cursor < end && count < E820_MAX_ENTRIES) {
        const multiboot_mmap_entry* entry = (const multiboot_mmap_entry*)cursor;
        u64 base = ((u64)entry->base_high << 32) | entry->base_low;
        u64 length = ((u64)entry->length_high << 32) | entry->length_low;
        u64 region_end = base + length;

        if (entry->type == E820_TYPE_USABLE && length > 0 && base < 0x100000000ULL) {
            if (region_end > 0x100000000ULL) {
                region_end = 0x100000000ULL;
            }

            bootinfo_multiboot_map[count].base_low = (u32)base;
            bootinfo_multiboot_map[count].base_high = (u32)(base >> 32);
            bootinfo_multiboot_map[count].length_low = (u32)length;
            bootinfo_multiboot_map[count].length_high = (u32)(length >> 32);
            bootinfo_multiboot_map[count].type = entry->type;
            count++;
        }

        cursor += entry->size + sizeof(entry->size);
    }

    bootinfo_multiboot_count = count;
    return bootinfo_multiboot_map;
}
