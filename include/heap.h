#ifndef HEAP_H
#define HEAP_H

#include "types.h"

void heap_init(u32 heap_start, u32 heap_size);
void* kmalloc(u32 size);
void kfree(void* ptr);

u32 heap_total_bytes(void);
u32 heap_used_bytes(void);

#endif /* HEAP_H */
