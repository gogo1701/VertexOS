#ifndef RTL8139_H
#define RTL8139_H

#include "types.h"

typedef void (*rtl8139_rx_callback)(const u8* frame, u32 len);

u8 rtl8139_init(rtl8139_rx_callback rx_cb);
u8 rtl8139_is_up(void);
const u8* rtl8139_mac(void);
u8 rtl8139_send(const void* frame, u32 len);
void rtl8139_poll(void);

#endif /* RTL8139_H */
