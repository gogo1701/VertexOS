#include "rtl8139.h"

#include "display.h"
#include "io.h"
#include "pci.h"

#define RTL8139_VENDOR_ID 0x10EC
#define RTL8139_DEVICE_ID 0x8139

#define REG_IDR0    0x00
#define REG_MAR0    0x08
#define REG_TSD0    0x10
#define REG_TSAD0   0x20
#define REG_RBSTART 0x30
#define REG_CAPR    0x38
#define REG_CBR     0x3A
#define REG_IMR     0x3C
#define REG_ISR     0x3E
#define REG_TCR     0x40
#define REG_RCR     0x44
#define REG_CR      0x37
#define REG_CONFIG1 0x52

#define CR_RX_ENABLE 0x08
#define CR_TX_ENABLE 0x04
#define CR_RESET     0x10
#define CR_RX_EMPTY  0x01

#define RTL8139_DEBUG 1

#define RX_STATUS_OK 0x0001u

#define RX_BUF_SIZE 8192u
#define RX_BUF_PAD  16u
#define RX_BUF_WRAP 1500u
#define RX_TOTAL_SIZE (RX_BUF_SIZE + RX_BUF_PAD + RX_BUF_WRAP)

#define TX_BUF_SIZE 2048u
#define TX_COUNT 4u

static u16 io_base;
static u8 mac[6];
static u8 up;

/* NIC ring and TX buffers must be in the identity-mapped kernel BSS region
 * (physical == virtual). kmalloc lives in heap pages that are remapped to
 * arbitrary physical frames, so passing a heap pointer to the NIC registers
 * gives a wrong physical (DMA) address. */
static u8 rx_ring_buf[RX_TOTAL_SIZE];
static u8 tx_buf_data[TX_COUNT][TX_BUF_SIZE];

static u8* rx_buffer;
static u32 rx_offset;

static u8* tx_buffers[TX_COUNT];
static u32 tx_index;

static rtl8139_rx_callback rx_callback;
static u8 rx_frame_tmp[1600];

static void dbg(const char* s) {
#if RTL8139_DEBUG
    display_print(s);
#else
    (void)s;
#endif
}

static void dbg_hex(u32 v) {
#if RTL8139_DEBUG
    display_print("0x");
    display_print_num(v, 16);
#else
    (void)v;
#endif
}

static u8 mem_copy(void* dst, const void* src, u32 len) {
    u8* d = (u8*)dst;
    const u8* s = (const u8*)src;
    u32 i;
    if (!dst || !src) {
        return 0;
    }
    for (i = 0; i < len; i++) {
        d[i] = s[i];
    }
    return 1;
}

static u8 reg_inb(u16 reg) {
    return io_inb((u16)(io_base + reg));
}

static u32 reg_inl(u16 reg) {
    return io_inl((u16)(io_base + reg));
}

static void reg_outb(u16 reg, u8 value) {
    io_outb((u16)(io_base + reg), value);
}

static void reg_outw(u16 reg, u16 value) {
    io_outw((u16)(io_base + reg), value);
}

static void reg_outl(u16 reg, u32 value) {
    io_outl((u16)(io_base + reg), value);
}

static void rtl8139_enable_pci_busmaster(u8 bus, u8 slot, u8 func) {
    u32 cmd = pci_config_read32(bus, slot, func, 0x04);
    cmd |= 0x00000007u;
    pci_config_write32(bus, slot, func, 0x04, cmd);
}

static u8 rtl8139_setup_buffers(void) {
    u32 i;

    /* Use static BSS arrays — physical address == virtual address. */
    rx_buffer = rx_ring_buf;

    for (i = 0; i < TX_COUNT; i++) {
        tx_buffers[i] = tx_buf_data[i];
    }

    return 1;
}

u8 rtl8139_init(rtl8139_rx_callback cb) {
    u8 bus;
    u8 slot;
    u8 func;
    u32 bar0;
    u32 i;

    rx_callback = cb;
    up = 0;

    if (!pci_find_device(RTL8139_VENDOR_ID, RTL8139_DEVICE_ID, &bus, &slot, &func)) {
        dbg("rtl8139: pci device not found\n");
        return 0;
    }

    rtl8139_enable_pci_busmaster(bus, slot, func);

    bar0 = pci_config_read32(bus, slot, func, 0x10);
    if ((bar0 & 0x1u) == 0) {
        /* BAR0 is not I/O-mapped; this driver uses port I/O only. */
        dbg("rtl8139: BAR0 is not io-mapped\n");
        return 0;
    }
    io_base = (u16)(bar0 & 0xFFFCu);

    dbg("rtl8139: io base ");
    dbg_hex(io_base);
    dbg("\n");

    if (io_base == 0) {
        return 0;
    }

    if (!rtl8139_setup_buffers()) {
        dbg("rtl8139: buffer allocation failed\n");
        return 0;
    }

    reg_outb(REG_CONFIG1, 0x00);

    reg_outb(REG_CR, CR_RESET);
    for (i = 0; i < 100000u; i++) {
        if ((reg_inb(REG_CR) & CR_RESET) == 0) {
            break;
        }
    }

    if ((reg_inb(REG_CR) & CR_RESET) != 0) {
        dbg("rtl8139: reset timeout\n");
        return 0;
    }

    for (i = 0; i < 6; i++) {
        mac[i] = reg_inb((u16)(REG_IDR0 + i));
    }

    /* Reference driver comment: "Must enable Tx/Rx before setting transfer
     * thresholds!" - enable RE+TE first, THEN configure RCR/TCR/RBSTART. */
    reg_outb(REG_CR, (u8)(CR_RX_ENABLE | CR_TX_ENABLE));

    /* RCR: RXFTH=4(128B) bits[15:13], RBLEN=0(8KB) bits[12:11],
     * MXDMA=4(256B) bits[10:8], AB|AM|APM|AAP bits[3:0].
     * (4<<13)|(0<<11)|(4<<8)|0x0F = 0x8000|0x0400|0x0F = 0x840F */
    reg_outl(REG_RCR, 0x840Fu);

    /* TCR: MXDMA=4 (256-byte bursts) per reference. */
    reg_outl(REG_TCR, (4u << 8));

    /* Set RBSTART after enabling RE+TE (matches reference driver order). */
    reg_outl(REG_RBSTART, (u32)rx_buffer);

    /* Accept all multicast hashes. */
    reg_outl(REG_MAR0,     0xFFFFFFFFu);
    reg_outl(REG_MAR0 + 4, 0xFFFFFFFFu);

    /* Re-enable RE+TE after configuring (reference does a second enable
     * after set_rx_mode to make sure the chip picks up the new settings). */
    reg_outb(REG_CR, (u8)(CR_RX_ENABLE | CR_TX_ENABLE));

    /* IMR set last, ISR cleared first (OSDev wiki QEMU requirement). */
    reg_outw(REG_ISR, 0xFFFF);
    reg_outw(REG_IMR, 0x0005);

    rx_offset = 0;
    tx_index = 0;
    up = 1;
    dbg("rtl8139: init ok\n");
    return 1;
}

u8 rtl8139_is_up(void) {
    return up;
}

const u8* rtl8139_mac(void) {
    return mac;
}

u8 rtl8139_send(const void* frame, u32 len) {
    u32 tsd_reg;
    u32 tsad_reg;
    u8* tx_buf;
    u32 status;

    if (!up || !frame || len == 0 || len > 1514u) {
        return 0;
    }

    tsd_reg  = REG_TSD0  + (tx_index * 4u);
    tsad_reg = REG_TSAD0 + (tx_index * 4u);
    tx_buf   = tx_buffers[tx_index];

    status = reg_inl((u16)tsd_reg);
    if ((status & (1u << 13)) == 0 && (status & (1u << 15)) == 0) {
        /* Controller still owns this descriptor; drop for now. */
        return 0;
    }

    if (!mem_copy(tx_buf, frame, len)) {
        return 0;
    }

    /* Reference driver: write TX buffer physical address BEFORE writing TSD.
     * Without this the NIC DMA's from whatever garbage was in TSAD. */
    reg_outl((u16)tsad_reg, (u32)tx_buf);

    /* TX_FIFO_THRESH=256 bytes -> bits[20:16] = 8 (units of 32 bytes).
     * 8 << 16 = 0x80000. OR'd with packet length in bits[12:0]. */
    reg_outl((u16)tsd_reg, 0x80000u | (len & 0x1FFFu));

    tx_index = (u32)((tx_index + 1u) % TX_COUNT);
    return 1;
}

static u16 read_u16(const u8* p) {
    return (u16)(p[0] | ((u16)p[1] << 8));
}

void rtl8139_poll(void) {
    u16 isr;

    if (!up) {
        return;
    }

    isr = io_inw((u16)(io_base + REG_ISR));
    if (isr == 0) {
        return;
    }

    dbg("rtl8139: isr=");
    dbg_hex(isr);
    dbg("\n");

    /* OSDev wiki: clear ISR BEFORE reading packets - required on QEMU.
     * Clearing after causes subsequent packets to be missed. */
    reg_outw(REG_ISR, isr);

    while ((reg_inb(REG_CR) & CR_RX_EMPTY) == 0) {
        u8* p = rx_buffer + rx_offset;
        u16 status = read_u16(p);
        u16 size = read_u16(p + 2);
        u32 frame_len;
        u32 i;
        u32 next;

        (void)status;

        if ((status & RX_STATUS_OK) == 0) {
            dbg("rtl8139: rx bad status=");
            dbg_hex(status);
            dbg(" size=");
            dbg_hex(size);
            dbg("\n");
        }

        if (size < 4 || size > 1600) {
            dbg("rtl8139: rx size invalid status=");
            dbg_hex(status);
            dbg(" size=");
            dbg_hex(size);
            dbg("\n");
            reg_outw(REG_ISR, 0xFFFF);
            break;
        }

        dbg("rtl8139: rx frame status=");
        dbg_hex(status);
        dbg(" len=");
        dbg_hex(size);
        dbg("\n");

        frame_len = (u32)(size - 4u);

        for (i = 0; i < frame_len; i++) {
            rx_frame_tmp[i] = rx_buffer[(rx_offset + 4u + i) % RX_BUF_SIZE];
        }

        if (rx_callback) {
            rx_callback(rx_frame_tmp, frame_len);
        }

        next = rx_offset + (u32)size + 4u;
        next = (next + 3u) & ~3u;
        rx_offset = next % RX_BUF_SIZE;

        reg_outw(REG_CAPR, (u16)((rx_offset - 16u) & 0x1FFFu));
    }
}
