#include <inttypes.h>
#include "freertos/projdefs.h"
#include "pill_timer_mgr.h"
#include "portmacro.h"
#include "esp_task.h"

#include "defines.h"
#include "rtc.h"
#include "display.h"
#include "u8g2.h"
#include "u8g2_esp32_hal.h"

#define DISPLAY_UPDATE_FREQ_MS (33)

DisplayMode_t display_mode;
SemaphoreHandle_t display_mutex;

static TaskHandle_t display_task_handle;

static void display_task(void*);
static void display_draw_mode_clock();
static size_t format_approx_duration(duration_ms_t duration, char* out_str, size_t len_out_str);

void display_init(void) {
    display_mode = DISPLAY_MODE_CLOCK;
    display_mutex = xSemaphoreCreateMutex();

    xSemaphoreTake(display_mutex, portMAX_DELAY);

    u8g2_esp32_init_ssd1306_i2c(&u8g2, i2c_bus);

    xTaskCreate(display_task,
        "Display Task",
        4096,
        NULL,
        TASK_PRIORITY_HIGH,
        &display_task_handle
    );

    xSemaphoreGive(display_mutex);
}

static void display_task(void*) {
    while (true) {
        xSemaphoreTake(display_mutex, portMAX_DELAY);

        switch (display_mode) {
            case DISPLAY_MODE_CLOCK: 
                display_draw_mode_clock();
                break;
            case DISPLAY_MODE_MENU: break;
            case DISPLAY_MODE_RINGING: break;
        }
        u8g2_SendBuffer(&u8g2);

        xSemaphoreGive(display_mutex);

        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(250));
    }
}

void tick_display(void) {
    xTaskNotify(display_task_handle, 0, 0);
}

static void display_draw_mode_clock(void) {
    static char str_buf[64];

    const display_time_in_day_t time = rtc_get_display_time_in_day();
    const char* am_pm = time.hours > 12 ? "PM" : "AM";
    snprintf(str_buf, sizeof(str_buf), "%02d:%02d:%02d %s", time.hours % 12, time.mins, time.secs, am_pm);

    u8g2_ClearBuffer(&u8g2);

    u8g2_SetFont(&u8g2, u8g2_font_9x18_mr);
    
    u8g2_uint_t str_width = u8g2_GetStrWidth(&u8g2, str_buf);
    u8g2_DrawStr(&u8g2,
                 (DISPLAY_WIDTH_PX - str_width) / 2,
                 30,
                 str_buf);

    PillTimer_t* next_timer;
    duration_ms_t time_till_next_timer = pill_timer_get_next_to_ring(&next_timer);
    if (next_timer) {
        char next_timer_buf[8];
        format_approx_duration(time_till_next_timer, next_timer_buf, sizeof(next_timer_buf));

        char dispenser_char = 'A' + next_timer->dispenser_idx - PILL_DISPENSER_IDX_A;
        snprintf(str_buf, sizeof(str_buf), "Up Next: %c in %s", dispenser_char, next_timer_buf);
    } else {
        snprintf(str_buf, sizeof(str_buf), "Next: None");
    }

    u8g2_SetFont(&u8g2, u8g2_font_7x13_mr);

    str_width = u8g2_GetStrWidth(&u8g2, str_buf);
    u8g2_DrawStr(&u8g2,
                 (DISPLAY_WIDTH_PX - str_width) / 2,
                 50,
                 str_buf);
}

// Recommended: `len_out_str >= 8`
static size_t format_approx_duration(duration_ms_t duration, char* out_str, size_t len_out_str) {
    if (duration > ((MS_IN_HOUR * 85) / 100)) {
        // Duration > 85% of an hour: Round to the nearest hour
        uint32_t num_hours = duration / MS_IN_HOUR;
        
        // Make this rounding division instead of floor
        const uint32_t rem_ms = duration % MS_IN_HOUR;
        if (rem_ms > (MS_IN_HOUR / 2)) { num_hours++; }
        
        const char* plural_modifier = num_hours > 1 ? "s" : "";
        return snprintf(out_str, len_out_str, "%" PRId32 " hr%s", num_hours, plural_modifier);
    } else {
        // Less than 85% of an hour, give nearest minute count
        uint32_t num_mins = duration / MS_IN_MINUTE;
        
        // Make this rounding division instead of floor
        const uint32_t rem_ms = duration % MS_IN_MINUTE;
        if (rem_ms > (MS_IN_MINUTE / 2)) { num_mins++; }
        
        const char* plural_modifier = num_mins > 1 ? "s" : "";
        return snprintf(out_str, len_out_str, "%" PRId32 " min%s", num_mins, plural_modifier);
    }
}
