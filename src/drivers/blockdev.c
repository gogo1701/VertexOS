#include "blockdev.h"
#include "ata.h"

static block_device g_dev;

void blockdev_init(void) {
    ata_init();

    g_dev.name = "ata0";
    g_dev.sector_size = 512;
    g_dev.sector_count = 65536; /* 32 MiB logical limit */
    g_dev.read_sector = ata_read_sector;
    g_dev.write_sector = ata_write_sector;
}

const block_device* blockdev_get(void) {
    return &g_dev;
}

u8 blockdev_read(u32 lba, void* buffer) {
    if (!g_dev.read_sector || lba >= g_dev.sector_count) {
        return 0;
    }
    return g_dev.read_sector(lba, buffer);
}

u8 blockdev_write(u32 lba, const void* buffer) {
    if (!g_dev.write_sector || lba >= g_dev.sector_count) {
        return 0;
    }
    return g_dev.write_sector(lba, buffer);
}
