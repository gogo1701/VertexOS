#include "exec.h"

#include "display.h"
#include "heap.h"
#include "vfs.h"

#define ELF_MAGIC 0x464C457Fu
#define ELF_CLASS_32 1u
#define ELF_DATA_LSB 1u
#define ELF_VERSION_CURRENT 1u
#define ELF_TYPE_EXEC 2u
#define ELF_MACHINE_386 3u
#define ELF_PT_LOAD 1u
#define EXEC_IMAGE_LIMIT (1024u * 1024u)

typedef struct {
    u8 ident[16];
    u16 type;
    u16 machine;
    u32 version;
    u32 entry;
    u32 phoff;
    u32 shoff;
    u32 flags;
    u16 ehsize;
    u16 phentsize;
    u16 phnum;
    u16 shentsize;
    u16 shnum;
    u16 shstrndx;
} __attribute__((packed)) elf32_ehdr;

typedef struct {
    u32 type;
    u32 offset;
    u32 vaddr;
    u32 paddr;
    u32 filesz;
    u32 memsz;
    u32 flags;
    u32 align;
} __attribute__((packed)) elf32_phdr;

static u32 s_len(const char* s) {
    u32 n = 0;
    while (s && s[n]) {
        n++;
    }
    return n;
}

static void mem_zero(void* p, u32 n) {
    u8* b = (u8*)p;
    u32 i;
    for (i = 0; i < n; i++) {
        b[i] = 0;
    }
}

static void mem_copy(void* dst, const void* src, u32 n) {
    u8* d = (u8*)dst;
    const u8* s = (const u8*)src;
    u32 i;
    for (i = 0; i < n; i++) {
        d[i] = s[i];
    }
}

static void exec_error(const char* msg) {
    display_print("exec: ");
    display_print(msg);
    display_put_char('\n');
}

static u8 read_file_all(const char* path, u8** out_data, u32* out_size) {
    sfs_node_info st;
    s32 fd;
    u8* data;
    u32 total = 0;

    if (!vfs_stat_path(path, &st) || st.type != SFS_TYPE_FILE) {
        return 0;
    }

    fd = vfs_open(path, VFS_O_READ);
    if (fd < 0) {
        return 0;
    }

    data = (u8*)kmalloc(st.size ? st.size : 1u);
    if (!data) {
        vfs_close(fd);
        return 0;
    }

    while (total < st.size) {
        u32 chunk = vfs_read(fd, data + total, st.size - total);
        if (chunk == 0) {
            break;
        }
        total += chunk;
    }

    vfs_close(fd);

    if (total != st.size) {
        kfree(data);
        return 0;
    }

    *out_data = data;
    *out_size = st.size;
    return 1;
}

u8 exec_run_elf(const char* path) {
    u8* file_data = 0;
    u32 file_size = 0;
    elf32_ehdr* eh;
    u32 i;
    u32 min_vaddr = 0xFFFFFFFFu;
    u32 max_vaddr = 0;
    u8* image;
    u32 image_size;
    void (*entry)(void);

    if (!path || !path[0]) {
        exec_error("missing path");
        return 0;
    }

    if (!read_file_all(path, &file_data, &file_size)) {
        exec_error("unable to read file");
        return 0;
    }

    if (file_size < sizeof(elf32_ehdr)) {
        exec_error("file too small");
        kfree(file_data);
        return 0;
    }

    eh = (elf32_ehdr*)file_data;

    if (*(u32*)&eh->ident[0] != ELF_MAGIC) {
        exec_error("not an ELF file");
        kfree(file_data);
        return 0;
    }

    if (eh->ident[4] != ELF_CLASS_32 || eh->ident[5] != ELF_DATA_LSB || eh->ident[6] != ELF_VERSION_CURRENT) {
        exec_error("unsupported ELF class/data/version");
        kfree(file_data);
        return 0;
    }

    if (eh->type != ELF_TYPE_EXEC || eh->machine != ELF_MACHINE_386) {
        exec_error("unsupported ELF type or machine");
        kfree(file_data);
        return 0;
    }

    if (eh->phentsize != sizeof(elf32_phdr) || eh->phoff + (u32)eh->phnum * sizeof(elf32_phdr) > file_size) {
        exec_error("invalid program header table");
        kfree(file_data);
        return 0;
    }

    for (i = 0; i < eh->phnum; i++) {
        elf32_phdr* ph = (elf32_phdr*)(file_data + eh->phoff + i * sizeof(elf32_phdr));
        u32 seg_end;

        if (ph->type != ELF_PT_LOAD || ph->memsz == 0) {
            continue;
        }

        if (ph->offset + ph->filesz > file_size || ph->filesz > ph->memsz) {
            exec_error("invalid load segment");
            kfree(file_data);
            return 0;
        }

        seg_end = ph->vaddr + ph->memsz;
        if (ph->vaddr < min_vaddr) {
            min_vaddr = ph->vaddr;
        }
        if (seg_end > max_vaddr) {
            max_vaddr = seg_end;
        }
    }

    if (min_vaddr == 0xFFFFFFFFu || max_vaddr <= min_vaddr) {
        exec_error("no loadable segments");
        kfree(file_data);
        return 0;
    }

    image_size = max_vaddr - min_vaddr;
    if (image_size > EXEC_IMAGE_LIMIT) {
        exec_error("program image too large");
        kfree(file_data);
        return 0;
    }

    image = (u8*)kmalloc(image_size);
    if (!image) {
        exec_error("out of memory");
        kfree(file_data);
        return 0;
    }
    mem_zero(image, image_size);

    for (i = 0; i < eh->phnum; i++) {
        elf32_phdr* ph = (elf32_phdr*)(file_data + eh->phoff + i * sizeof(elf32_phdr));

        if (ph->type != ELF_PT_LOAD || ph->memsz == 0) {
            continue;
        }

        mem_copy(image + (ph->vaddr - min_vaddr), file_data + ph->offset, ph->filesz);
    }

    if (eh->entry < min_vaddr || eh->entry >= max_vaddr) {
        exec_error("entry outside image");
        kfree(image);
        kfree(file_data);
        return 0;
    }

    display_print("exec: running ");
    display_print(path);
    display_print(" (");
    display_print_num(s_len(path), 10);
    display_print(" chars path)\n");

    entry = (void (*)(void))(image + (eh->entry - min_vaddr));
    entry();

    display_print("exec: program returned\n");

    kfree(image);
    kfree(file_data);
    return 1;
}
