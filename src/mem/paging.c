#include "paging.h"
#include "panic.h"
#include "pmm.h"

#define PAGE_PRESENT 0x001u
#define PAGE_WRITABLE 0x002u

static u32* page_directory = 0;

static void memset32(u32* ptr, u32 value, u32 count) {
    u32 i;
    for (i = 0; i < count; i++) {
        ptr[i] = value;
    }
}

void paging_map_page(u32 virtual_addr, u32 physical_addr, u32 flags) {
    u32 pd_index = virtual_addr >> 22;
    u32 pt_index = (virtual_addr >> 12) & 0x3FFu;
    u32* table;

    if (!page_directory) {
        panic("Paging not initialized");
    }

    if (!(page_directory[pd_index] & PAGE_PRESENT)) {
        u32 table_phys = pmm_alloc_frame();
        table = (u32*)table_phys;
        memset32(table, 0, 1024);
        page_directory[pd_index] = table_phys | PAGE_PRESENT | PAGE_WRITABLE;
    }

    table = (u32*)(page_directory[pd_index] & 0xFFFFF000u);
    table[pt_index] = (physical_addr & 0xFFFFF000u) | (flags & 0xFFFu) | PAGE_PRESENT;
}

void paging_reload_directory(void) {
    __asm__ __volatile__("mov %0, %%cr3" : : "r"(page_directory));
}

void paging_init_identity(u32 limit_addr) {
    u32 addr;
    u32 dir_phys = pmm_alloc_frame();

    page_directory = (u32*)dir_phys;
    memset32(page_directory, 0, 1024);

    limit_addr = (limit_addr + PAGE_SIZE - 1u) & ~(PAGE_SIZE - 1u);

    for (addr = 0; addr < limit_addr; addr += PAGE_SIZE) {
        paging_map_page(addr, addr, PAGE_WRITABLE);
    }

    __asm__ __volatile__("mov %0, %%cr3" : : "r"(page_directory));

    __asm__ __volatile__(
        "mov %%cr0, %%eax\n"
        "or $0x80000000, %%eax\n"
        "mov %%eax, %%cr0\n"
        :
        :
        : "eax"
    );
}
