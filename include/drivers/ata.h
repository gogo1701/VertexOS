#ifndef ATA_H
#define ATA_H

#include "types.h"

void ata_init(void);
u8 ata_read_sector(u32 lba, void* buffer);
u8 ata_write_sector(u32 lba, const void* buffer);

#endif /* ATA_H */
