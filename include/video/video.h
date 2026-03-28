#ifndef VIDEO_H
#define VIDEO_H

#include "types.h"

typedef enum {
    VIDEO_MODE_TEXT = 0,
    VIDEO_MODE_GRAPHICS = 1
} video_mode;

void video_init(video_mode initial_mode);
video_mode video_get_mode(void);
const char* video_mode_name(video_mode mode);
u8 video_get_boot_preference(video_mode* out_mode);
u8 video_set_boot_preference(video_mode mode);

#endif /* VIDEO_H */
