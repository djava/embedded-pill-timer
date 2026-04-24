#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "rtc.h"
#include "defines.h"
#include "esp_err.h"
#include "pcf8563.h"

static pcf8563_t pcf8563;
static SemaphoreHandle_t rtc_mutex;

void rtc_hw_init(void) {
    pcf8563_init(&pcf8563, i2c_bus);

    bool clock_invalid;
    pcf8563_time_t time;
    ESP_ERROR_CHECK(pcf8563_get_time(&pcf8563, &time, &clock_invalid));

    if (clock_invalid) {
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

    rtc_mutex = xSemaphoreCreateMutex();
}

display_time_in_day_t rtc_get_display_time_in_day(void) {
    bool clock_invalid;
    pcf8563_time_t pcf_time;
    
    xSemaphoreTake(rtc_mutex, portMAX_DELAY);
    ESP_ERROR_CHECK(pcf8563_get_time(&pcf8563, &pcf_time, &clock_invalid));
    xSemaphoreGive(rtc_mutex);
    
    const display_time_in_day_t time = {
        .hours = pcf_time.hour,
        .mins = pcf_time.min,
        .secs = pcf_time.sec
    };
    return time;
}


time_in_day_ms_t rtc_get_time_in_day_ms(void) {
    bool clock_invalid;
    pcf8563_time_t pcf_time;

    xSemaphoreTake(rtc_mutex, portMAX_DELAY);
    ESP_ERROR_CHECK(pcf8563_get_time(&pcf8563, &pcf_time, &clock_invalid));
    xSemaphoreGive(rtc_mutex);

    const time_in_day_ms_t time =
        MS_IN_HOUR * pcf_time.hour +
        MS_IN_MINUTE * pcf_time.min +
        MS_IN_SECOND * pcf_time.sec;

    return time;
}