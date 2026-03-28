#ifndef PANIC_H
#define PANIC_H

#include "types.h"

/* Print a fatal error and halt permanently. */
void panic(const char* message);

/* Assert failure handler used by KASSERT macro. */
void panic_assert_fail(const char* expr, const char* file, u32 line);

#define KASSERT(expr) \
    do { \
        if (!(expr)) { \
            panic_assert_fail(#expr, __FILE__, __LINE__); \
        } \
    } while (0)

#endif /* PANIC_H */
