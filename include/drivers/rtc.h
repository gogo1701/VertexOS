#ifndef RTC_H
#define RTC_H

#include "types.h"

typedef struct {
    u8 second;
    u8 minute;
    u8 hour;
    u8 day;
    u8 month;
    u8 year;
} rtc_datetime;

u8 rtc_read_datetime(rtc_datetime* out);

#endif /* RTC_H */
