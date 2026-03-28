#ifndef PAGING_H
#define PAGING_H

#include "types.h"

void paging_init_identity(u32 limit_addr);
void paging_map_page(u32 virtual_addr, u32 physical_addr, u32 flags);
void paging_reload_directory(void);

#endif /* PAGING_H */
