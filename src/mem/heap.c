#include "heap.h"
#include "paging.h"
#include "pmm.h"

#define HEAP_ALIGN 8u

typedef struct block_header {
    u32 size;
    u8 free;
    struct block_header* next;
} block_header;

static block_header* heap_head = 0;
static u32 heap_size_total = 0;

static u32 align_up(u32 value, u32 align) {
    return (value + align - 1u) & ~(align - 1u);
}

void heap_init(u32 heap_start, u32 heap_size) {
    u32 addr;

    heap_size = align_up(heap_size, PAGE_SIZE);

    for (addr = heap_start; addr < heap_start + heap_size; addr += PAGE_SIZE) {
        u32 frame = pmm_alloc_frame();
        paging_map_page(addr, frame, 0x002u);
    }

    paging_reload_directory();

    heap_head = (block_header*)heap_start;
    heap_head->size = heap_size - (u32)sizeof(block_header);
    heap_head->free = 1;
    heap_head->next = 0;

    heap_size_total = heap_size;
}

void* kmalloc(u32 size) {
    block_header* current;

    if (size == 0 || !heap_head) {
        return 0;
    }

    size = align_up(size, HEAP_ALIGN);
    current = heap_head;

    while (current) {
        if (current->free && current->size >= size) {
            u32 remaining = current->size - size;

            if (remaining > sizeof(block_header) + HEAP_ALIGN) {
                block_header* next = (block_header*)((u8*)(current + 1) + size);
                next->size = remaining - (u32)sizeof(block_header);
                next->free = 1;
                next->next = current->next;
                current->next = next;
                current->size = size;
            }

            current->free = 0;
            return (void*)(current + 1);
        }
        current = current->next;
    }

    return 0;
}

void kfree(void* ptr) {
    block_header* current;

    if (!ptr) {
        return;
    }

    current = ((block_header*)ptr) - 1;
    current->free = 1;

    current = heap_head;
    while (current && current->next) {
        if (current->free && current->next->free) {
            current->size += (u32)sizeof(block_header) + current->next->size;
            current->next = current->next->next;
            continue;
        }
        current = current->next;
    }
}

u32 heap_total_bytes(void) {
    return heap_size_total;
}

u32 heap_used_bytes(void) {
    block_header* current = heap_head;
    u32 used = 0;

    while (current) {
        if (!current->free) {
            used += current->size;
        }
        current = current->next;
    }

    return used;
}
