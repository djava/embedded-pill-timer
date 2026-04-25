#ifndef RTC_H
#define RTC_H
#include "defines.h"

void rtc_hw_init(void);
display_time_in_day_t rtc_get_display_time_in_day(void);
time_in_day_ms_t rtc_get_time_in_day_ms(void);
pcf8563_time_t rtc_get_full_timestamp(void);
bool rtc_date_was_across_midnight(pcf8563_time_t *old_time);

#endif