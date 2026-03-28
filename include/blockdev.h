#ifndef BLOCKDEV_H
#define BLOCKDEV_H

#include "types.h"

typedef struct {
    const char* name;
    u32 sector_size;
    u32 sector_count;
    u8 (*read_sector)(u32 lba, void* buffer);
    u8 (*write_sector)(u32 lba, const void* buffer);
} block_device;

void blockdev_init(void);
const block_device* blockdev_get(void);

u8 blockdev_read(u32 lba, void* buffer);
u8 blockdev_write(u32 lba, const void* buffer);

#endif /* BLOCKDEV_H */
