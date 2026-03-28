/*
 * RTC — Real Time Clock (CMOS)
 *
 * Reads the current date and time from the CMOS RTC registers via I/O
 * ports 0x70 (index) and 0x71 (data).
 *
 * All date/time values are returned in BCD-decoded binary form.
 * The century is hard-coded to 20xx; the rtc_datetime::year field holds
 * the last two digits (e.g. 26 for 2026).
 */

#ifndef RTC_H
#define RTC_H

#include "types.h"

/*
 * rtc_datetime - Current date and time from the hardware clock.
 *
 * All fields are binary (BCD decoding is done automatically).
 *
 * @second: 0-59
 * @minute: 0-59
 * @hour:   0-23 (24-hour format)
 * @day:    1-31
 * @month:  1-12
 * @year:   0-99  (century assumed 20xx)
 */
typedef struct {
    u8 second;
    u8 minute;
    u8 hour;
    u8 day;
    u8 month;
    u8 year;
} rtc_datetime;

/*
 * rtc_read_datetime - Read the current date/time from the hardware clock.
 *
 * Waits for the RTC update-in-progress flag to clear, then reads all
 * six CMOS registers atomically (as seen by software).
 *
 * @out: Pointer to caller-allocated rtc_datetime to fill.
 *
 * @return: 1 on success, 0 if the RTC appears invalid.
 */
u8 rtc_read_datetime(rtc_datetime* out);

#endif /* RTC_H */
