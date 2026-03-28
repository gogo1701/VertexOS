#include "video.h"

#include "blockdev.h"

#define BOOT_PREF_SECTOR 0u
#define BOOT_PREF_OFFSET 508u
#define BOOT_OVERLAY_OFFSET 509u

static video_mode g_mode = VIDEO_MODE_TEXT;
void video_init(video_mode initial_mode) {
    if (initial_mode == VIDEO_MODE_GRAPHICS) {
        g_mode = VIDEO_MODE_GRAPHICS;
    } else {
        g_mode = VIDEO_MODE_TEXT;
    }
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

video_mode video_get_mode(void) {
    return g_mode;
}

const char* video_mode_name(video_mode mode) {
    if (mode == VIDEO_MODE_GRAPHICS) {
        return "graphics";
    }
    return "text";
}

u8 video_set_boot_preference(video_mode mode) {
    u8 sector[512];

    if (!blockdev_read(BOOT_PREF_SECTOR, sector)) {
        return 0;
    }

    sector[BOOT_PREF_OFFSET] = (mode == VIDEO_MODE_GRAPHICS) ? 1u : 0u;
    return blockdev_write(BOOT_PREF_SECTOR, sector);
}

u8 video_get_boot_overlay_preference(u8* out_enabled) {
    u8 sector[512];

    if (!out_enabled || !blockdev_read(BOOT_PREF_SECTOR, sector)) {
        return 0;
    }

    *out_enabled = sector[BOOT_OVERLAY_OFFSET] ? 1u : 0u;
    return 1;
}

u8 video_set_boot_overlay_preference(u8 enabled) {
    u8 sector[512];

    if (!blockdev_read(BOOT_PREF_SECTOR, sector)) {
        return 0;
    }

    sector[BOOT_OVERLAY_OFFSET] = enabled ? 1u : 0u;
    return blockdev_write(BOOT_PREF_SECTOR, sector);
}
