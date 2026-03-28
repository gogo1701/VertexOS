/*
 * ATA PIO Disk Driver
 *
 * Provides synchronous, polled (PIO mode) access to the primary ATA bus.
 * All I/O is performed in 512-byte sectors.  IRQs are not used; the driver
 * busy-waits on the status register.
 *
 * Note: Only the primary master drive (LBA28) is currently supported.
 */

#ifndef ATA_H
#define ATA_H

#include "types.h"

/*
 * ata_init - Detect and initialise the primary ATA device.
 *
 * Should be called once during kernel boot before any read/write.
 * Resets the controller and identifies the attached drive.
 */
void ata_init(void);

/*
 * ata_read_sector - Read one 512-byte sector from disk.
 *
 * @lba:    Logical Block Address (0-based) of the sector to read.
 * @buffer: Caller-allocated buffer of at least 512 bytes.
 *
 * @return: 1 on success, 0 on error (drive not ready, bad status).
 */
u8 ata_read_sector(u32 lba, void* buffer);

/*
 * ata_write_sector - Write one 512-byte sector to disk.
 *
 * @lba:    Logical Block Address (0-based) of the sector to write.
 * @buffer: Buffer containing exactly 512 bytes of data to write.
 *
 * @return: 1 on success, 0 on error.
 */
u8 ata_write_sector(u32 lba, const void* buffer);

#endif /* ATA_H */
