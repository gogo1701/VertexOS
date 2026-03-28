/*
 * Boot Memory Map (BIOS E820)
 *
 * Parses the physical memory map provided by the BIOS via the INT 15h,
 * AX=E820 call that is performed by the bootloader in real mode and
 * passed to the kernel as a flat array of e820_entry structs.
 *
 * Only type-1 (usable RAM) regions are returned by the helper, and those
 * are further filtered to fit within the 32-bit address space
 * (base + length <= 0xFFFFFFFF).
 */

#ifndef BOOTINFO_H
#define BOOTINFO_H

#include "types.h"

#define E820_MAX_ENTRIES 32  /* Maximum E820 entries passed from bootloader */
#define E820_TYPE_USABLE 1   /* E820 type value for free/usable RAM         */

/*
 * e820_entry - Raw entry from the BIOS E820 memory map.
 *
 * Stored as a packed struct matching the layout written by the bootloader.
 *
 * @base_low / base_high:     64-bit physical base address (split into halves).
 * @length_low / length_high: 64-bit region length in bytes (split into halves).
 * @type: Region type (1 = usable RAM, 2 = reserved, 3 = ACPI, etc.).
 */
typedef struct {
    u32 base_low;
    u32 base_high;
    u32 length_low;
    u32 length_high;
    u32 type;
} __attribute__((packed)) e820_entry;

/*
 * memory_region - A single usable 32-bit physical memory region.
 *
 * @base:   Physical start address.
 * @length: Region size in bytes.
 */
typedef struct {
    u32 base;
    u32 length;
} memory_region;

/*
 * bootinfo_get_usable_regions - Convert E820 map into 32-bit usable regions.
 *
 * Iterates the raw BIOS memory map and extracts every entry that is
 * type-1 (usable) and fully addressable within 32 bits.  The results
 * are written into out_regions.
 *
 * @entries:     Pointer to the raw E820 array (from bootloader).
 * @entry_count: Number of valid entries in the array.
 * @out_regions: Caller-allocated array to receive usable-memory descriptors.
 * @max_regions: Capacity of out_regions.
 *
 * @return: Number of usable regions written into out_regions.
 */
u32 bootinfo_get_usable_regions(
    const e820_entry* entries,
    u32 entry_count,
    memory_region* out_regions,
    u32 max_regions
);

#endif /* BOOTINFO_H */
