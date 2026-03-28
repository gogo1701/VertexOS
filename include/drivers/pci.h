#ifndef PCI_H
#define PCI_H

#include "types.h"

u32 pci_config_read32(u8 bus, u8 slot, u8 func, u8 offset);
void pci_config_write32(u8 bus, u8 slot, u8 func, u8 offset, u32 value);

u8 pci_find_device(u16 vendor_id, u16 device_id, u8* out_bus, u8* out_slot, u8* out_func);

#endif /* PCI_H */
