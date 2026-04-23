#ifndef RTC_H
#define RTC_H
#include "defines.h"

void rtc_hw_init(void);
display_time_in_day_t rtc_get_display_time_of_day(void);

#endif