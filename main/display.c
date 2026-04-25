#include <inttypes.h>
#include "freertos/projdefs.h"
#include "menus.h"
#include "pill_timer_mgr.h"
#include "portmacro.h"
#include "esp_log.h"
#include "esp_task.h"

#include "defines.h"
#include "rtc.h"
#include "display.h"
#include "u8g2.h"
#include "u8g2_esp32_hal.h"

#define DISPLAY_UPDATE_FREQ_MS (1000/30)

_Atomic(DisplayMode_t) display_mode;
SemaphoreHandle_t display_mutex;

static TaskHandle_t display_task_handle;

static void display_task(void*);
static void display_draw_mode_clock(void);
static size_t format_approx_duration(duration_ms_t duration, char* out_str, size_t len_out_str);
static void display_draw_mode_menu(void);
static void display_draw_mode_menu_timer_list(void);
static void display_draw_mode_menu_config_list(void);
static void display_draw_mode_menu_config_item(void);


void display_init(void) {
    display_mode = DISPLAY_MODE_CLOCK;
    display_mutex = xSemaphoreCreateMutex();

    xSemaphoreTake(display_mutex, portMAX_DELAY);

    u8g2_esp32_init_ssd1306_i2c(&u8g2, i2c_bus);

    xTaskCreate(display_task,
        "Display Task",
        4096,
        NULL,
        TASK_PRIORITY_LOW,
        &display_task_handle
    );

    xSemaphoreGive(display_mutex);
}

static void display_task(void*) {
    TickType_t last_tick;
    while (true) {
        last_tick = xTaskGetTickCount();
        xSemaphoreTake(display_mutex, portMAX_DELAY);

        const TickType_t t0 [[maybe_unused]] = xTaskGetTickCount();
        const DisplayMode_t mode = display_mode;
        switch (mode) {
            case DISPLAY_MODE_CLOCK:
                display_draw_mode_clock();
                break;
            case DISPLAY_MODE_MENU:
                display_draw_mode_menu();
                break;
            case DISPLAY_MODE_RINGING:
                u8g2_ClearBuffer(&u8g2);
                break;
        }
        const TickType_t t1 [[maybe_unused]] = xTaskGetTickCount();

        u8g2_SendBuffer(&u8g2);
        const TickType_t t2 [[maybe_unused]] = xTaskGetTickCount();

        xSemaphoreGive(display_mutex);

        // ESP_LOGI("display",
        //          "frame: draw=%" PRIu32 "ms send=%" PRIu32 "ms total=%" PRIu32 "ms (budget %d ms)",
        //          (uint32_t)((t1 - t0) * portTICK_PERIOD_MS),
        //          (uint32_t)((t2 - t1) * portTICK_PERIOD_MS),
        //          (uint32_t)((t2 - last_tick) * portTICK_PERIOD_MS),
        //          DISPLAY_UPDATE_FREQ_MS);

        vTaskDelayUntil(&last_tick, pdMS_TO_TICKS(DISPLAY_UPDATE_FREQ_MS));
    }
}

void tick_display(void) {
    xTaskNotify(display_task_handle, 0, 0);
}

static void display_draw_mode_clock(void) {
    static char str_buf[64];

    const display_time_in_day_t time = rtc_get_display_time_in_day();
    const char* am_pm = time.hours >= 12 ? "PM" : "AM";
    const uint8_t display_hours = ((time.hours + 11) % 12) + 1;
    snprintf(str_buf, sizeof(str_buf), "%02d:%02d:%02d %s", display_hours, time.mins, time.secs, am_pm);

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

    u8g2_DrawHLine(&u8g2, (DISPLAY_WIDTH_PX - str_width) / 2, 35, str_width);

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
    } else if (duration > (MS_IN_MINUTE / 2)) {
        // Less than 85% of an hour, give nearest minute count
        uint32_t num_mins = duration / MS_IN_MINUTE;
        
        // Make this rounding division instead of floor
        const uint32_t rem_ms = duration % MS_IN_MINUTE;
        if (rem_ms > (MS_IN_MINUTE / 2)) { num_mins++; }
        
        const char* plural_modifier = num_mins > 1 ? "s" : "";
        return snprintf(out_str, len_out_str, "%" PRId32 " min%s", num_mins, plural_modifier);
    } else {
        // Less than 30 sec: return as now
        return snprintf(out_str, len_out_str, "Now");
    }
}

static void display_draw_mode_menu(void) {
    if (xSemaphoreTake(menu_state_mutex, pdMS_TO_TICKS(10)) != pdPASS) {
        return;
    }

    u8g2_ClearBuffer(&u8g2);
    switch (menu_state.page) {
        case MENU_PAGE_TIMER_LIST:
            display_draw_mode_menu_timer_list();
            break;
        case MENU_PAGE_CONFIG_LIST:
            break;
        case MENU_PAGE_CONFIG_ITEM:
            break;
    }

    xSemaphoreGive(menu_state_mutex);
}

static void display_draw_mode_menu_timer_list(void) {
    static char str_buf[16];

    const size_t sel_idx = menu_state.sel_index.timer_num;
    if (sel_idx == MENU_SEL_TIMER_BACK_IDX) {
        u8g2_SetFont(&u8g2, u8g2_font_9x18_mr);

        snprintf(str_buf, sizeof(str_buf), "BACK");

        const u8g2_uint_t str_width = u8g2_GetStrWidth(&u8g2, str_buf);
        u8g2_DrawStr(&u8g2,
                    (DISPLAY_WIDTH_PX - str_width) / 2,
                    30,
                    str_buf);
        
        u8g2_DrawHLine(&u8g2, (DISPLAY_WIDTH_PX - str_width) / 2, 32, str_width);
    } else {
        const size_t num_per_column = 4;
        const u8g2_uint_t X_COL1 = 2;
        const u8g2_uint_t X_COL2 = 65;
        const u8g2_uint_t Y_SPACING = 15;

        u8g2_SetFont(&u8g2, u8g2_font_6x13B_mr);

        for (size_t i = 0; i < NUM_PILL_TIMERS; i++) {
            snprintf(str_buf, sizeof(str_buf), "Timer %d", i + 1);

            u8g2_DrawStr(&u8g2, 
                         i < num_per_column ? X_COL1 : X_COL2,
                         Y_SPACING * ((i % num_per_column) + 1), 
                         str_buf);
        }
    
        u8g2_DrawHLine(&u8g2,
                       sel_idx < num_per_column ? X_COL1 : X_COL2,
                       (Y_SPACING * ((sel_idx % num_per_column) + 1)) + 1,
                       u8g2_GetStrWidth(&u8g2, "Timer 1"));
    }
}

static void display_draw_mode_menu_config_list(void) {

}

static void display_draw_mode_menu_config_item(void) {

}
