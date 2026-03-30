#include "video.h"

#include "blockdev.h"
#include "pci.h"
#include "serial.h"

/* QEMU standard VGA (Bochs VBE) PCI IDs */
#define VGA_PCI_VENDOR  0x1234u
#define VGA_PCI_DEVICE  0x1111u
#define VGA_PCI_BAR0    0x10u
#define VGA_LFB_DEFAULT 0xE0000000u

static u32 discover_vbe_lfb_addr(void) {
    u8 bus, slot, func;
    u32 bar;
    if (!pci_find_device(VGA_PCI_VENDOR, VGA_PCI_DEVICE, &bus, &slot, &func)) {
        serial_write("[DBG video] PCI VGA not found, using default LFB ");
        serial_write_hex32(VGA_LFB_DEFAULT);
        serial_write_char('\n');
        return VGA_LFB_DEFAULT;
    }
    bar = pci_config_read32(bus, slot, func, VGA_PCI_BAR0);
    serial_write("[DBG video] PCI VGA BAR0 raw=");
    serial_write_hex32(bar);
    bar &= 0xFFFFFFF0u;  /* mask off type/flag bits */
    serial_write(" masked=");
    serial_write_hex32(bar);
    serial_write_char('\n');
    return bar ? bar : VGA_LFB_DEFAULT;
}

#define BOOT_PREF_SECTOR 0u
#define BOOT_PREF_OFFSET 508u
#define BOOT_OVERLAY_OFFSET 509u

#define BOOT_FLAG_OVERLAY_MASK 0x01u
#define BOOT_FLAG_RES_SHIFT 1u
#define BOOT_FLAG_RES_MASK 0x06u

static video_mode g_mode = VIDEO_MODE_TEXT;
static video_resolution g_resolution = VIDEO_RES_320X200;
static video_fb_info g_fb_info = { 0xA0000u, 320u, 200u, 320u, 8u };

static video_resolution decode_resolution_bits(u8 flags) {
    u8 value = (u8)((flags & BOOT_FLAG_RES_MASK) >> BOOT_FLAG_RES_SHIFT);
    if (value == (u8)VIDEO_RES_640X480) {
        return VIDEO_RES_640X480;
    }
    if (value == (u8)VIDEO_RES_800X600) {
        return VIDEO_RES_800X600;
    }
    return VIDEO_RES_320X200;
}

static u8 encode_resolution_bits(video_resolution resolution) {
    if (resolution == VIDEO_RES_640X480) {
        return (u8)(VIDEO_RES_640X480 << BOOT_FLAG_RES_SHIFT);
    }
    if (resolution == VIDEO_RES_800X600) {
        return (u8)(VIDEO_RES_800X600 << BOOT_FLAG_RES_SHIFT);
    }
    return (u8)(VIDEO_RES_320X200 << BOOT_FLAG_RES_SHIFT);
}

static void set_fb_defaults(video_resolution resolution) {
    g_fb_info.bpp = 8u;
    g_fb_info.fb_phys = 0xA0000u;

    if (resolution == VIDEO_RES_640X480) {
        g_fb_info.fb_phys = discover_vbe_lfb_addr();
        g_fb_info.width = 640u;
        g_fb_info.height = 480u;
        g_fb_info.pitch = 640u;
        return;
    }

    if (resolution == VIDEO_RES_800X600) {
        g_fb_info.fb_phys = discover_vbe_lfb_addr();
        g_fb_info.width = 800u;
        g_fb_info.height = 600u;
        g_fb_info.pitch = 800u;
        return;
    }

    g_fb_info.width = 320u;
    g_fb_info.height = 200u;
    g_fb_info.pitch = 320u;
}

void video_init(u32 boot_video_state) {
    g_mode = (boot_video_state & 0x01u) ? VIDEO_MODE_GRAPHICS : VIDEO_MODE_TEXT;
    g_resolution = decode_resolution_bits((u8)boot_video_state);
    set_fb_defaults(g_resolution);

    serial_write("[DBG video] boot_video_state=");
    serial_write_hex32(boot_video_state);
    serial_write(" mode=");
    serial_write(g_mode == VIDEO_MODE_GRAPHICS ? "graphics" : "text");
    serial_write(" res=");
    serial_write(video_resolution_name(g_resolution));
    serial_write(" fb_phys=");
    serial_write_hex32(g_fb_info.fb_phys);
    serial_write(" w=");
    serial_write_dec(g_fb_info.width);
    serial_write(" h=");
    serial_write_dec(g_fb_info.height);
    serial_write_char('\n');
}

u8 video_get_boot_preference(video_mode* out_mode) {
    u8 sector[512];
    u8 pref;

    if (!out_mode || !blockdev_read(BOOT_PREF_SECTOR, sector)) {
        return 0;
    }

    pref = sector[BOOT_PREF_OFFSET];
    *out_mode = (pref == 1u) ? VIDEO_MODE_GRAPHICS : VIDEO_MODE_TEXT;
    return 1;
}

u8 video_get_boot_resolution_preference(video_resolution* out_resolution) {
    u8 sector[512];

    if (!out_resolution || !blockdev_read(BOOT_PREF_SECTOR, sector)) {
        return 0;
    }

    *out_resolution = decode_resolution_bits(sector[BOOT_OVERLAY_OFFSET]);
    return 1;
}

video_mode video_get_mode(void) {
    return g_mode;
}

const char* video_mode_name(video_mode mode) {
    if (mode == VIDEO_MODE_GRAPHICS) {
        return "graphics";
    }
    return "text";
}

video_resolution video_get_resolution(void) {
    return g_resolution;
}

const char* video_resolution_name(video_resolution resolution) {
    if (resolution == VIDEO_RES_640X480) {
        return "640x480";
    }
    if (resolution == VIDEO_RES_800X600) {
        return "800x600";
    }
    return "320x200";
}

const video_fb_info* video_get_fb_info(void) {
    return &g_fb_info;
}

u8 video_set_boot_preference(video_mode mode) {
    u8 sector[512];

    if (!blockdev_read(BOOT_PREF_SECTOR, sector)) {
        return 0;
    }

    sector[BOOT_PREF_OFFSET] = (mode == VIDEO_MODE_GRAPHICS) ? 1u : 0u;
    return blockdev_write(BOOT_PREF_SECTOR, sector);
}

u8 video_set_boot_resolution_preference(video_resolution resolution) {
    u8 sector[512];
    u8 flags;

    if (!blockdev_read(BOOT_PREF_SECTOR, sector)) {
        return 0;
    }

    flags = sector[BOOT_OVERLAY_OFFSET];
    flags &= (u8)~BOOT_FLAG_RES_MASK;
    flags |= encode_resolution_bits(resolution);
    sector[BOOT_OVERLAY_OFFSET] = flags;
    return blockdev_write(BOOT_PREF_SECTOR, sector);
}

u8 video_get_boot_overlay_preference(u8* out_enabled) {
    u8 sector[512];

    if (!out_enabled || !blockdev_read(BOOT_PREF_SECTOR, sector)) {
        return 0;
    }

    *out_enabled = (sector[BOOT_OVERLAY_OFFSET] & BOOT_FLAG_OVERLAY_MASK) ? 1u : 0u;
    return 1;
}

u8 video_set_boot_overlay_preference(u8 enabled) {
    u8 sector[512];
    u8 flags;

    if (!blockdev_read(BOOT_PREF_SECTOR, sector)) {
        return 0;
    }

    flags = sector[BOOT_OVERLAY_OFFSET];
    flags &= (u8)~BOOT_FLAG_OVERLAY_MASK;
    if (enabled) {
        flags |= BOOT_FLAG_OVERLAY_MASK;
    }
    sector[BOOT_OVERLAY_OFFSET] = flags;
    return blockdev_write(BOOT_PREF_SECTOR, sector);
}
