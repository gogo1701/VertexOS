#include "pci.h"
#include "io.h"

#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA    0xCFC

static u32 pci_make_address(u8 bus, u8 slot, u8 func, u8 offset) {
    return (1u << 31) |
           ((u32)bus << 16) |
           ((u32)slot << 11) |
           ((u32)func << 8) |
           (offset & 0xFCu);
}

u32 pci_config_read32(u8 bus, u8 slot, u8 func, u8 offset) {
    io_outl(PCI_CONFIG_ADDRESS, pci_make_address(bus, slot, func, offset));
    return io_inl(PCI_CONFIG_DATA);
}

void pci_config_write32(u8 bus, u8 slot, u8 func, u8 offset, u32 value) {
    io_outl(PCI_CONFIG_ADDRESS, pci_make_address(bus, slot, func, offset));
    io_outl(PCI_CONFIG_DATA, value);
}

u8 pci_find_device(u16 vendor_id, u16 device_id, u8* out_bus, u8* out_slot, u8* out_func) {
    u16 vendor;
    u16 device;
    u8 bus;
    u8 slot;
    u8 func;

    for (bus = 0; ; bus++) {
        for (slot = 0; slot < 32; slot++) {
            for (func = 0; func < 8; func++) {
                u32 id = pci_config_read32(bus, slot, func, 0x00);
                vendor = (u16)(id & 0xFFFFu);
                if (vendor == 0xFFFFu) {
                    if (func == 0) {
                        break;
                    }
                    continue;
                }

                device = (u16)((id >> 16) & 0xFFFFu);
                if (vendor == vendor_id && device == device_id) {
                    if (out_bus) {
                        *out_bus = bus;
                    }
                    if (out_slot) {
                        *out_slot = slot;
                    }
                    if (out_func) {
                        *out_func = func;
                    }
                    return 1;
                }
            }
        }

        if (bus == 255) {
            break;
        }
    }

    return 0;
}
