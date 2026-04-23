#ifndef DEFINES_H
#define DEFINES_H
#include "driver/i2c_types.h"
#include "pcf8563.h"
#include "u8g2.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>
#include "esp_task.h"

typedef uint32_t duration_ms_t;
typedef uint32_t time_in_day_ms_t;
typedef struct {
    uint8_t hours;
    uint8_t mins;
    uint8_t secs;
} display_time_in_day_t;

#define NUM_PILL_DISPENSERS (2)
#define NUM_PILL_TIMERS (8)

typedef uint8_t dispenser_idx_t;

extern i2c_master_bus_handle_t i2c_bus;
extern u8g2_t u8g2;

#define TASK_PRIORITY_HIGH (ESP_TASK_PRIO_MAX - 5)
#define TASK_PRIORITY_MED  (ESP_TASK_PRIO_MAX - 10)
#define TASK_PRIORITY_LOW  (ESP_TASK_PRIO_MAX - 15)

#define DISPLAY_WIDTH_PX (128)
#define DISPLAY_HEIGHT_PX (64)

#endif