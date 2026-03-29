# Driver APIs

Low-level hardware drivers.  Most kernel and application code should never
call these directly; they are used internally by the higher-level subsystems
(interrupts, scheduler, VFS, display, etc.).

The only driver APIs that are commonly needed in new code are
[Serial](#serial) for early/debug logging and
[RTC](#rtc-real-time-clock) for reading the current time.

For network-facing features, use the higher-level network API documented in
[Network Stack](#network-stack-net) rather than talking to the NIC directly.

---

## ATA PIO Disk Driver

> Header: `include/drivers/ata.h`

```c
void ata_init(void);
u8   ata_read_sector(u32 lba, void* buffer);
u8   ata_write_sector(u32 lba, const void* buffer);
```

Polled 512-byte sector access to the primary ATA master drive.

**Do not call this directly** — use the block device layer or the VFS instead.

---

## Block Device Abstraction

> Header: `include/drivers/blockdev.h`

```c
void               blockdev_init(void);
const block_device* blockdev_get(void);
u8                 blockdev_read(u32 lba, void* buffer);
u8                 blockdev_write(u32 lba, const void* buffer);
```

Uniform read/write interface over the registered hardware driver.  Used by
the SimpleFS layer; you will not normally call this from new code.

---

## Interrupt Controller (PIC)

> Header: `include/drivers/pic.h`

```c
void pic_remap(u8 master_offset, u8 slave_offset);
void pic_set_mask(u8 irq_line);
void pic_clear_mask(u8 irq_line);
void pic_send_eoi(u8 irq_line);
```

Most code only ever needs `pic_send_eoi()` when writing a new IRQ handler.

### Writing a new IRQ handler

1. Register an entry in the IDT via the assembly stubs in
   `boot/interrupts.asm`.
2. Call `pic_clear_mask(irq_number)` once during initialisation to unmask
   the IRQ line.
3. At the **end** of every invocation of your handler call:
   ```c
   pic_send_eoi(irq_number);
   ```

---

## Timer (PIT)

> Header: `include/drivers/pit.h`

```c
void pit_init(u32 frequency_hz);
void pit_irq_handler(void);
u32  pit_get_ticks(void);
u32  pit_get_frequency(void);
```

### Measuring elapsed time

```c
u32 start = pit_get_ticks();
/* ... work ... */
u32 elapsed_ms = ((pit_get_ticks() - start) * 1000u) / pit_get_frequency();
```

---

## Keyboard

> Header: `include/drivers/keyboard.h`

```c
#define KEY_LEFT   0x100
#define KEY_RIGHT  0x101
#define KEY_DELETE 0x102
#define KEY_UP     0x103
#define KEY_DOWN   0x104

void keyboard_init(void);
void keyboard_irq_handler(void);
s32  keyboard_read_key(void);    /* blocks; returns ASCII or KEY_* */
char keyboard_read_char(void);   /* blocks; returns ASCII only      */
```

`keyboard_read_key()` returns either a printable ASCII value or one of the
`KEY_*` constants for special keys.  Use it when you need arrow key support.

`keyboard_read_char()` is a convenience wrapper for commands that only
need printable characters.

---

## RTL8139 NIC

> Header: `include/drivers/rtl8139.h`

```c
typedef void (*rtl8139_rx_callback)(const u8* frame, u32 len);

u8 rtl8139_init(rtl8139_rx_callback rx_cb);
u8 rtl8139_is_up(void);
const u8* rtl8139_mac(void);
u8 rtl8139_send(const void* frame, u32 len);
void rtl8139_poll(void);
```

Low-level Ethernet driver for the QEMU RTL8139 device.

- `rtl8139_init()` probes PCI, configures RX/TX rings, and installs the receive callback.
- `rtl8139_poll()` handles pending RX/TX status in polling mode.
- `rtl8139_send()` transmits a raw Ethernet frame.

You typically should not call this from command code. Use `net_*` APIs instead.

### Debug log toggle

The NIC driver has a compile-time debug switch in `src/drivers/rtl8139.c`:

```c
#define RTL8139_DEBUG 0
```

Set to `1` only when diagnosing low-level NIC bring-up issues.

---

## Network Stack (net)

> Header: `include/drivers/net.h`

```c
typedef struct {
    u8 mac[6];
    u32 ip;
    u32 subnet;
    u32 gateway;
    u32 dns;
    u8 link_up;
    u8 dhcp_configured;
} net_config;

void net_init(void);
void net_poll(void);
u8 net_is_ready(void);

const net_config* net_get_config(void);
void net_print_config(void);

u8 net_dhcp_request(void);
u8 net_ping(u32 target_ip, u32 timeout_ms, u32* out_rtt_ms);
u8 net_resolve_ipv4(const char* host, u32 timeout_ms, u32* out_ip);

u8 net_parse_ipv4(const char* text, u32* out_ip);
void net_format_ipv4(u32 ip, char* out, u32 out_size);
```

This module provides the command-facing networking surface (ARP, IPv4, ICMP,
UDP, DHCP, DNS resolve).

### Typical flow in shell commands

1. Call `net_is_ready()` to check link status.
2. Call `net_dhcp_request()` to obtain IP configuration.
3. Use `net_ping()` for reachability tests.
4. Use `net_resolve_ipv4()` for hostname to IPv4 resolution.

### Debug log toggle

The network stack also has a compile-time debug switch in `src/drivers/net.c`:

```c
#define NET_DEBUG 0
```

Set to `1` only for packet flow debugging.

---

## Serial (COM1)

> Header: `include/drivers/serial.h`

```c
void serial_init(void);
void serial_write_char(char c);
void serial_write(const char* s);
u8   serial_is_ready(void);
```

All output from `display_print()` is automatically mirrored to COM1.  You
only need to call these functions directly for very early boot messages
(before `display_init()`) or for raw binary protocols.

### QEMU — viewing serial output

```
qemu-system-i386 -drive format=raw,file=build/os-image.bin -serial stdio
```

---

## RTC (Real Time Clock)

> Header: `include/drivers/rtc.h`

```c
typedef struct {
    u8 second;  /* 0-59  */
    u8 minute;  /* 0-59  */
    u8 hour;    /* 0-23  */
    u8 day;     /* 1-31  */
    u8 month;   /* 1-12  */
    u8 year;    /* 0-99 (century = 20xx) */
} rtc_datetime;

u8 rtc_read_datetime(rtc_datetime* out);
```

### Example

```c
#include "rtc.h"
#include "display.h"

void print_time(void) {
    rtc_datetime dt;
    if (!rtc_read_datetime(&dt)) {
        display_print("rtc: read failed\n");
        return;
    }
    display_print("20");
    display_print_num(dt.year, 10);
    display_put_char('-');
    display_print_num(dt.month, 10);
    display_put_char('-');
    display_print_num(dt.day, 10);
}
```

---

## Power Control

> Header: `include/drivers/power.h`

```c
void power_restart(void);   /* hard CPU reset via keyboard controller */
void power_shutdown(void);  /* ACPI/QEMU power-off; HLT fallback      */
```

Both functions disable interrupts and **never return**.

---

## Interrupt Subsystem

> Header: `include/drivers/interrupts.h`

```c
void interrupts_init(void);    /* set up IDT, PIC, PIT, enable IRQs */
void interrupts_enable(void);  /* STI */
void interrupts_disable(void); /* CLI */
void interrupts_halt(void);    /* HLT (wakes on next IRQ) */
```

`interrupts_init()` is called once in `kmain()`.  The other helpers are
occasionally needed when writing low-level code that must temporarily
suppress or wait for interrupts.
