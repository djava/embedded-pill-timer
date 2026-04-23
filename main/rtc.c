#include "rtc.h"
#include "defines.h"
#include "esp_err.h"
#include "pcf8563.h"

pcf8563_t pcf8563;

void rtc_hw_init(void) {
    pcf8563_init(&pcf8563, i2c_bus);

    bool has_clock;
    pcf8563_time_t time;
    ESP_ERROR_CHECK(pcf8563_get_time(&pcf8563, &time, &has_clock));

    if (!has_clock) {
        time = (pcf8563_time_t) {
            .year = 2026,
            .month = 4,
            .day = 22,
            .hour = 9,
            .min = 13,
            .sec = 0
        };
        ESP_ERROR_CHECK(pcf8563_set_time(&pcf8563, &time));
    }
}

#define MS_IN_SECOND (1000)
#define MS_IN_MINUTE (60 * MS_IN_SECOND)
#define MS_IN_HOUR (60 * MS_IN_MINUTE)

display_time_in_day_t rtc_get_display_time_of_day(void) {
    bool clock_invalid;
    pcf8563_time_t pcf_time;

    ESP_ERROR_CHECK(pcf8563_get_time(&pcf8563, &pcf_time, &clock_invalid));

    const display_time_in_day_t time = {
        .hours = pcf_time.hour,
        .mins = pcf_time.min,
        .secs = pcf_time.sec
    };
    return time;
}