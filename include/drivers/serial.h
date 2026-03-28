#ifndef SERIAL_H
#define SERIAL_H

#include "types.h"

void serial_init(void);
u8 serial_is_ready(void);
void serial_write_char(char c);
void serial_write(const char* s);

#endif /* SERIAL_H */
