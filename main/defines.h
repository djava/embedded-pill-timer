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
#define PILL_TIMER_TIMEOUT_DURATION_MS (30000)

typedef enum {
    PILL_DISPENSER_IDX_INVALID = 0,
    PILL_DISPENSER_IDX_A,
    PILL_DISPENSER_IDX_B,
} DispenserIdx_t;

extern i2c_master_bus_handle_t i2c_bus;
extern u8g2_t u8g2;

#define TASK_PRIORITY_HIGH (ESP_TASK_PRIO_MAX - 5)
#define TASK_PRIORITY_MED  (ESP_TASK_PRIO_MAX - 10)
#define TASK_PRIORITY_LOW  (ESP_TASK_PRIO_MAX - 15)

#define DISPLAY_WIDTH_PX  (128)
#define DISPLAY_HEIGHT_PX (64)

#define GPIO_PIN_DISPENSER_A      (4)
#define GPIO_PIN_DISPENSER_B      (10)

extern const gpio_num_t DISPENSER_TO_GPIO_PIN[3];

#define GPIO_PIN_MENU_BUTTON_UP   (20)
#define GPIO_PIN_MENU_BUTTON_DOWN (21)
#define GPIO_PIN_MENU_BUTTON_OK   (9)

#define MS_IN_SECOND (1000)
#define MS_IN_MINUTE (60 * MS_IN_SECOND)
#define MS_IN_HOUR (60 * MS_IN_MINUTE)

#define REL_INTERVAL_CONFIG_INTERVAL_MS (15 * MS_IN_MINUTE)
#define REL_INTERVAL_CONFIG_MAX_MS (20 * MS_IN_HOUR)
#define REL_NUM_PER_DAY_CONFIG_MAX (32)
#define ABS_TIME_CONFIG_INTERVAL_MS (5 * MS_IN_MINUTE)
#define ABS_TIME_CONFIG_MAX_MS (23 * MS_IN_HOUR + 59 * MS_IN_MINUTE)

#endif