#include "ata.h"
#include "io.h"

#define ATA_IO_BASE 0x1F0
#define ATA_REG_DATA       (ATA_IO_BASE + 0)
#define ATA_REG_SECCOUNT0  (ATA_IO_BASE + 2)
#define ATA_REG_LBA0       (ATA_IO_BASE + 3)
#define ATA_REG_LBA1       (ATA_IO_BASE + 4)
#define ATA_REG_LBA2       (ATA_IO_BASE + 5)
#define ATA_REG_HDDEVSEL   (ATA_IO_BASE + 6)
#define ATA_REG_COMMAND    (ATA_IO_BASE + 7)
#define ATA_REG_STATUS     (ATA_IO_BASE + 7)

#define ATA_CMD_READ_PIO  0x20
#define ATA_CMD_WRITE_PIO 0x30
#define ATA_SR_BSY 0x80
#define ATA_SR_DRQ 0x08
#define ATA_SR_ERR 0x01

static void ata_io_wait(void) {
    io_inb(ATA_REG_STATUS);
    io_inb(ATA_REG_STATUS);
    io_inb(ATA_REG_STATUS);
    io_inb(ATA_REG_STATUS);
}

static u8 ata_wait_ready(void) {
    u8 status;
    for (;;) {
        status = io_inb(ATA_REG_STATUS);
        if (!(status & ATA_SR_BSY)) {
            if (status & ATA_SR_ERR) {
                return 0;
            }
            return 1;
        }
    }
}

static u8 ata_wait_drq(void) {
    u8 status;
    for (;;) {
        status = io_inb(ATA_REG_STATUS);
        if (!(status & ATA_SR_BSY) && (status & ATA_SR_DRQ)) {
            return 1;
        }
        if (status & ATA_SR_ERR) {
            return 0;
        }
    }
}

void ata_init(void) {
    (void)0;
}

u8 ata_read_sector(u32 lba, void* buffer) {
    u16* out = (u16*)buffer;
    u32 i;

    if (!ata_wait_ready()) {
        return 0;
    }

    io_outb(ATA_REG_HDDEVSEL, (u8)(0xE0 | ((lba >> 24) & 0x0F)));
    io_outb(ATA_REG_SECCOUNT0, 1);
    io_outb(ATA_REG_LBA0, (u8)(lba & 0xFF));
    io_outb(ATA_REG_LBA1, (u8)((lba >> 8) & 0xFF));
    io_outb(ATA_REG_LBA2, (u8)((lba >> 16) & 0xFF));
    io_outb(ATA_REG_COMMAND, ATA_CMD_READ_PIO);

    if (!ata_wait_drq()) {
        return 0;
    }

    for (i = 0; i < 256; i++) {
        __asm__ __volatile__("inw %1, %0" : "=a"(out[i]) : "Nd"(ATA_REG_DATA));
    }

    ata_io_wait();
    return 1;
}

u8 ata_write_sector(u32 lba, const void* buffer) {
    const u16* in = (const u16*)buffer;
    u32 i;

    if (!ata_wait_ready()) {
        return 0;
    }

    io_outb(ATA_REG_HDDEVSEL, (u8)(0xE0 | ((lba >> 24) & 0x0F)));
    io_outb(ATA_REG_SECCOUNT0, 1);
    io_outb(ATA_REG_LBA0, (u8)(lba & 0xFF));
    io_outb(ATA_REG_LBA1, (u8)((lba >> 8) & 0xFF));
    io_outb(ATA_REG_LBA2, (u8)((lba >> 16) & 0xFF));
    io_outb(ATA_REG_COMMAND, ATA_CMD_WRITE_PIO);

    if (!ata_wait_drq()) {
        return 0;
    }

    for (i = 0; i < 256; i++) {
        __asm__ __volatile__("outw %0, %1" : : "a"(in[i]), "Nd"(ATA_REG_DATA));
    }

    ata_io_wait();
    return ata_wait_ready();
}
