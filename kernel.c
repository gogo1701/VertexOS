typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;

#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define INPUT_MAX 128

static volatile u16* const VGA = (u16*)0xB8000;
static const u8 COLOR = 0x0F;
static u32 cursor_row = 0;
static u32 cursor_col = 0;

static inline u8 inb(u16 port) {
    u8 value;
    __asm__ __volatile__("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static void clear_screen(void) {
    u32 i;
    for (i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        VGA[i] = (u16)(' ' | (COLOR << 8));
    }
    cursor_row = 0;
    cursor_col = 0;
}

static void scroll_if_needed(void) {
    u32 row;
    u32 col;

    if (cursor_row < VGA_HEIGHT) {
        return;
    }

    for (row = 1; row < VGA_HEIGHT; row++) {
        for (col = 0; col < VGA_WIDTH; col++) {
            VGA[(row - 1) * VGA_WIDTH + col] = VGA[row * VGA_WIDTH + col];
        }
    }

    for (col = 0; col < VGA_WIDTH; col++) {
        VGA[(VGA_HEIGHT - 1) * VGA_WIDTH + col] = (u16)(' ' | (COLOR << 8));
    }

    cursor_row = VGA_HEIGHT - 1;
}

static void put_char(char c) {
    if (c == '\n') {
        cursor_col = 0;
        cursor_row++;
        scroll_if_needed();
        return;
    }

    if (c == '\b') {
        if (cursor_col > 0) {
            cursor_col--;
            VGA[cursor_row * VGA_WIDTH + cursor_col] = (u16)(' ' | (COLOR << 8));
        }
        return;
    }

    VGA[cursor_row * VGA_WIDTH + cursor_col] = (u16)((u8)c | (COLOR << 8));
    cursor_col++;

    if (cursor_col >= VGA_WIDTH) {
        cursor_col = 0;
        cursor_row++;
        scroll_if_needed();
    }
}

static void print(const char* s) {
    while (*s) {
        put_char(*s);
        s++;
    }
}

static char scancode_to_ascii(u8 scancode) {
    static const char map[128] = {
        0,
        27,
        '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=',
        '\b',
        '\t',
        'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']',
        '\n',
        0,
        'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
        0,
        '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/',
        0,
        '*',
        0,
        ' '
    };

    if (scancode < 128) {
        return map[scancode];
    }
    return 0;
}

static u8 read_scancode_blocking(void) {
    for (;;) {
        if (inb(0x64) & 1) {
            return inb(0x60);
        }
    }
}

void kmain(void) {
    char input[INPUT_MAX];
    u32 len = 0;

    clear_screen();
    print("Simple C console\n");
    print("Type and press Enter.\n\n");
    print("> ");

    for (;;) {
        u8 scancode = read_scancode_blocking();
        char c;

        if (scancode & 0x80) {
            continue;
        }

        c = scancode_to_ascii(scancode);
        if (!c) {
            continue;
        }

        if (c == '\b') {
            if (len > 0) {
                len--;
                put_char('\b');
            }
            continue;
        }

        if (c == '\n') {
            input[len] = '\0';
            put_char('\n');
            print("You said: ");
            print(input);
            put_char('\n');
            put_char('\n');
            print("> ");
            len = 0;
            continue;
        }

        if (len < INPUT_MAX - 1) {
            input[len] = c;
            len++;
            put_char(c);
        }
    }
}
