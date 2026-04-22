#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "driver/i2c_master.h"

typedef struct {
    uint8_t sec;     // 0-59
    uint8_t min;     // 0-59
    uint8_t hour;    // 0-23
    uint8_t day;     // 1-31
    uint8_t weekday; // 0-6 (0 = Sunday)
    uint8_t month;   // 1-12
    uint16_t year;   // e.g. 2026
} pcf8563_time_t;

typedef struct {
    i2c_master_dev_handle_t dev;
} pcf8563_t;

void pcf8563_init(pcf8563_t *rtc, i2c_master_bus_handle_t bus);
esp_err_t pcf8563_set_time(pcf8563_t *rtc, const pcf8563_time_t *t);
esp_err_t pcf8563_get_time(pcf8563_t *rtc, pcf8563_time_t *t, bool *clock_invalid);
