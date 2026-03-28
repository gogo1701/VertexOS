/* PS/2 Mouse Input Driver */

#ifndef MOUSE_H
#define MOUSE_H

#include "types.h"

/* Initialise PS/2 mouse support. */
void mouse_init(void);

/* IRQ12 handler entrypoint called from interrupt dispatcher. */
void mouse_irq_handler(void);

/* Current mouse pointer position in pixels. */
s32 mouse_get_x(void);
s32 mouse_get_y(void);
void mouse_hide_cursor(void);
void mouse_refresh_cursor(void);

/* Mouse button bit mask: 1=left, 2=right, 4=middle. */
u8 mouse_get_buttons(void);

#endif /* MOUSE_H */
