#include "freertos/projdefs.h"
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

static void display_draw_mode_clock();

void display_task(void*);

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

void display_task(void*) {
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
    static char time_fmt_buf[32];

    const display_time_in_day_t time = rtc_get_display_time_in_day();
    snprintf(time_fmt_buf, sizeof(time_fmt_buf), "%02d:%02d:%02d", time.hours, time.mins, time.secs);

    u8g2_ClearBuffer(&u8g2);

    u8g2_SetFont(&u8g2, u8g2_font_9x18_mr);
    
    const u8g2_uint_t str_width = u8g2_GetStrWidth(&u8g2, time_fmt_buf);
    u8g2_DrawStr(&u8g2,
                 (DISPLAY_WIDTH_PX - str_width) / 2,
                 (DISPLAY_HEIGHT_PX + 18) / 2,
                 time_fmt_buf);
}