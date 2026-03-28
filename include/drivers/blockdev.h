/*
 * Block Device Abstraction Layer
 *
 * Provides a single, uniform read/write interface on top of the underlying
 * hardware disk driver (ATA).  The rest of the kernel (filesystem, VFS)
 * must only use this layer and never call hardware drivers directly.
 *
 * Currently one device is registered at startup (the primary ATA disk).
 * The abstraction makes it straightforward to add RAM disks, virtual
 * drives, or additional physical devices in the future.
 */

#ifndef BLOCKDEV_H
#define BLOCKDEV_H

#include "types.h"

/*
 * block_device - Descriptor for a registered block storage device.
 *
 * @name:         Human-readable device label (e.g. "ata0").
 * @sector_size:  Bytes per logical sector (typically 512).
 * @sector_count: Total number of addressable sectors.
 * @read_sector:  Driver callback; returns 1 on success.
 * @write_sector: Driver callback; returns 1 on success.
 */
typedef struct {
    const char* name;
    u32 sector_size;
    u32 sector_count;
    u8 (*read_sector)(u32 lba, void* buffer);
    u8 (*write_sector)(u32 lba, const void* buffer);
} block_device;

/*
 * blockdev_init - Register the primary ATA disk as the system block device.
 *
 * Called once during kernel initialisation.  Must come before any vfs_init()
 * or filesystem call.
 */
void blockdev_init(void);

/*
 * blockdev_get - Return a pointer to the registered block device descriptor.
 *
 * @return: Pointer to the active block_device, or NULL if none is registered.
 */
const block_device* blockdev_get(void);

/*
 * blockdev_read - Read one sector via the registered block device.
 *
 * @lba:    Sector address (0-based).
 * @buffer: Caller-allocated buffer of at least sector_size bytes.
 *
 * @return: 1 on success, 0 if no device is registered or the driver fails.
 */
u8 blockdev_read(u32 lba, void* buffer);

/*
 * blockdev_write - Write one sector via the registered block device.
 *
 * @lba:    Sector address (0-based).
 * @buffer: Data to write; must be exactly sector_size bytes.
 *
 * @return: 1 on success, 0 on failure.
 */
u8 blockdev_write(u32 lba, const void* buffer);

#endif /* BLOCKDEV_H */
