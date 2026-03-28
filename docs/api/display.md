# Display & Video API

> Headers: `include/video/display.h`, `include/video/video.h`, `include/video/framebuffer.h`  
> Sources: `src/video/display.c`, `src/video/video.c`, `src/video/framebuffer.c`

---

## Display (text output)

The display module is the primary output API.  All kernel code and shell
commands use it to write text to the screen (and to COM1 serial
simultaneously).

### Constants

```c
#define VGA_WIDTH  80   /* Columns */
#define VGA_HEIGHT 25   /* Rows    */
```

### Functions

#### `display_init`

```c
void display_init(void);
```

Clear the screen and reset the cursor to (0, 0).  Called once in `kmain()`.

---

#### `display_print`

```c
void display_print(const char* s);
```

Print a null-terminated string.  Handles `\n` (newline) and scrolls the
display automatically when the last row is reached.

```c
display_print("Loading modules...\n");
```

---

#### `display_put_char`

```c
void display_put_char(char c);
```

Print a single character.  `\n` moves to the next line; `\b` erases the
previous character.

---

#### `display_print_num`

```c
void display_print_num(u32 num, u32 base);
```

Print an unsigned 32-bit integer in the given base.

```c
display_print("Free: ");
display_print_num(pmm_free_memory_bytes() / 1024, 10);
display_print(" KiB\n");

display_print("Addr: 0x");
display_print_num(ptr, 16);
display_put_char('\n');
```

---

#### `display_clear`

```c
void display_clear(void);
```

Erase all text and reset the cursor to the top-left corner.

---

#### `display_set_cursor` / `display_get_cursor`

```c
void display_set_cursor(u32 row, u32 col);
void display_get_cursor(u32* row, u32* col);
```

`display_set_cursor` positions both the software cursor and the VGA
hardware cursor (blinking underline).  Coordinates are clamped to screen
bounds automatically.

---

#### `display_refresh`

```c
void display_refresh(void);
```

Redraw the entire screen from the internal cell buffer.  Needed after
switching video modes.

---

#### `display_set_graphics_test_overlay` / `display_get_graphics_test_overlay`

```c
void display_set_graphics_test_overlay(u8 enabled);
u8   display_get_graphics_test_overlay(void);
```

Toggle the graphics-mode diagnostic overlay (coloured border + colour
blocks).  Only has a visible effect when booted into graphics mode.
Use the `video test on/off` shell command rather than calling this directly.

---

## Video mode control

### `video_mode` enum

```c
typedef enum {
    VIDEO_MODE_TEXT     = 0,   /* 80x25 VGA text (default) */
    VIDEO_MODE_GRAPHICS = 1    /* 320x200 mode 13h         */
} video_mode;
```

### Functions

#### `video_get_mode`

```c
video_mode video_get_mode(void);
```

Return the video mode that is active in the current boot session.

---

#### `video_set_boot_preference` / `video_get_boot_preference`

```c
u8 video_set_boot_preference(video_mode mode);
u8 video_get_boot_preference(video_mode* out_mode);
```

Persist or read the next-boot video mode.  The preference is written to a
reserved byte in the boot sector and applied by the bootloader on the next
reset using BIOS INT 10h.  A **reboot is required** for any change to take
effect.

```c
/* Save graphics mode for next boot */
video_set_boot_preference(VIDEO_MODE_GRAPHICS);

/* Read back the saved preference */
video_mode pref;
if (video_get_boot_preference(&pref)) {
    display_print(video_mode_name(pref));
}
```

---

#### `video_mode_name`

```c
const char* video_mode_name(video_mode mode);
```

Returns `"text"` or `"graphics"`.  Useful for status messages.

---

## Framebuffer (graphics mode)

Only relevant when running in `VIDEO_MODE_GRAPHICS`.

```c
#define FB_WIDTH  320
#define FB_HEIGHT 200
```

Pixels are 8-bit VGA palette indices.

### Functions

```c
void framebuffer_init(void);
void framebuffer_clear(u8 color);
void framebuffer_put_pixel(u32 x, u32 y, u8 color);
void framebuffer_fill_rect(u32 x, u32 y, u32 w, u32 h, u8 color);
```

### Example — draw a coloured rectangle

```c
#include "video.h"
#include "framebuffer.h"

void draw_ui(void) {
    if (video_get_mode() != VIDEO_MODE_GRAPHICS) return;

    framebuffer_clear(0);              /* black background */
    framebuffer_fill_rect(10, 10, 100, 50, 4);   /* red box */
    framebuffer_put_pixel(160, 100, 15);          /* white centre pixel */
}
```

### Standard VGA palette (first 16 colours)

| Index | Colour        |
|-------|---------------|
| 0     | Black         |
| 1     | Blue          |
| 2     | Green         |
| 3     | Cyan          |
| 4     | Red           |
| 5     | Magenta       |
| 6     | Brown         |
| 7     | Light grey    |
| 8     | Dark grey     |
| 9     | Light blue    |
| 10    | Light green   |
| 11    | Light cyan    |
| 12    | Light red     |
| 13    | Light magenta |
| 14    | Yellow        |
| 15    | White         |
