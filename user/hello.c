void _start(void) {
    volatile unsigned int i;

    for (i = 0; i < 100000u; i++) {
        __asm__ __volatile__("nop");
    }

    __asm__ __volatile__(
        "int $0x80"
        :
        : "a"(0), "b"(0), "c"(0), "d"(0)
        : "memory"
    );
}
