/*
 * Video Mode Manager
 *
 * Tracks the current video mode (text or graphics) and persists the
 * next-boot mode preference in the boot sector so the bootloader can
 * apply it via BIOS before entering protected mode.
 *
 * Available modes:
 *   VIDEO_MODE_TEXT     - VGA colour text mode (80x25, default)
 *   VIDEO_MODE_GRAPHICS - VGA mode 13h (320x200, 256 colours)
 *
 * Boot flow:
 *   1. Bootloader reads the saved preference from the boot sector.
 *   2. Uses BIOS INT 10h to set the video mode in real mode.
 *   3. Passes the selected mode to kmain() as boot_video_mode.
 *   4. kmain() calls video_init(boot_video_mode) so the kernel
 *      knows which backend to use.
 *
 * Shell commands:
 *   video status         - show current and saved next-boot mode
 *   video text           - save TEXT preference for next boot
 *   video gfx            - save GRAPHICS preference for next boot
 *   video test on/off    - enable/disable test overlay (gfx mode only)
 */

#ifndef VIDEO_H
#define VIDEO_H

#include "types.h"

/*
 * video_mode - Supported display modes.
 */
typedef enum {
    VIDEO_MODE_TEXT     = 0,  /* 80x25 VGA text mode (default)          */
    VIDEO_MODE_GRAPHICS = 1   /* 320x200 256-colour VGA mode 13h        */
} video_mode;

typedef enum {
    VIDEO_RES_320X200 = 0,
    VIDEO_RES_640X480 = 1,
    VIDEO_RES_800X600 = 2
} video_resolution;

typedef struct {
    u32 fb_phys;
    u32 width;
    u32 height;
    u32 pitch;
    u8 bpp;
} video_fb_info;

/*
 * video_init - Initialise the video subsystem for the current session.
 *
 * @initial_mode: The mode the bootloader selected, passed from kmain().
 *
 * Should be called before display_init().
 */
void video_init(u32 boot_video_state);

/*
 * video_get_mode - Return the video mode active in the current session.
 */
video_mode video_get_mode(void);

/*
 * video_mode_name - Return a human-readable name for a mode constant.
 *
 * @mode: VIDEO_MODE_TEXT or VIDEO_MODE_GRAPHICS.
 *
 * @return: "text" or "graphics".  Never NULL.
 */
const char* video_mode_name(video_mode mode);
video_resolution video_get_resolution(void);
const char* video_resolution_name(video_resolution resolution);
const video_fb_info* video_get_fb_info(void);

/*
 * video_get_boot_preference - Read the saved next-boot mode from disk.
 *
 * Reads the preference byte written into the boot sector by a previous
 * call to video_set_boot_preference().
 *
 * @out_mode: Receives the saved preference if the call succeeds.
 *
 * @return: 1 if a valid preference was found, 0 if unset or disk error.
 */
u8 video_get_boot_preference(video_mode* out_mode);
u8 video_get_boot_resolution_preference(video_resolution* out_resolution);
u8 video_get_boot_overlay_preference(u8* out_enabled);

/*
 * video_set_boot_preference - Persist a next-boot mode preference to disk.
 *
 * Writes the preference byte into a reserved field in sector 0.  The
 * bootloader reads this on the next boot and sets the video mode before
 * entering protected mode.  A reboot is required for the change to apply.
 *
 * @mode: VIDEO_MODE_TEXT or VIDEO_MODE_GRAPHICS.
 *
 * @return: 1 on success, 0 if the disk write fails.
 */
u8 video_set_boot_preference(video_mode mode);
u8 video_set_boot_resolution_preference(video_resolution resolution);

u8 video_set_boot_overlay_preference(u8 enabled);

#endif /* VIDEO_H */
