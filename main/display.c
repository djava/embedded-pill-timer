#include <inttypes.h>
#include <sys/param.h>
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

static char str_buf[64];
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

        char dispenser_char = 'A' + next_timer->dispenser_idx;
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
            display_draw_mode_menu_config_list();
            break;
        case MENU_PAGE_CONFIG_ITEM:
            display_draw_mode_menu_config_item();
            break;
    }

    xSemaphoreGive(menu_state_mutex);
}

static void display_draw_mode_menu_timer_list(void) {
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
        const size_t ITEMS_PER_COLUMN = 4;
        const u8g2_uint_t X_COL1 = 2;
        const u8g2_uint_t X_COL2 = 65;
        const u8g2_uint_t Y_SPACING = 15;

        u8g2_SetFont(&u8g2, u8g2_font_6x13B_mr);

        for (size_t i = 0; i < NUM_PILL_TIMERS; i++) {
            snprintf(str_buf, sizeof(str_buf), "Timer %d", i + 1);

            u8g2_DrawStr(&u8g2, 
                         i < ITEMS_PER_COLUMN ? X_COL1 : X_COL2,
                         Y_SPACING * ((i % ITEMS_PER_COLUMN) + 1), 
                         str_buf);
        }
    
        u8g2_DrawHLine(&u8g2,
                       sel_idx < ITEMS_PER_COLUMN ? X_COL1 : X_COL2,
                       (Y_SPACING * ((sel_idx % ITEMS_PER_COLUMN) + 1)) + 1,
                       u8g2_GetStrWidth(&u8g2, "Timer 1"));
    }
}

static void display_draw_mode_menu_config_list(void) {
    const size_t MAX_ITEMS_PER_SUBPAGE = 4;
    const u8g2_uint_t X_COL = 2;
    const u8g2_uint_t Y_SPACING = 15;

    const size_t subpage_index = menu_state.sel_index.config_idx / MAX_ITEMS_PER_SUBPAGE;
    const size_t sel_index_in_subpage = menu_state.sel_index.config_idx % MAX_ITEMS_PER_SUBPAGE;
    const size_t total_num_subpages = // Ceiling division
        (MENU_TIMER_NUM_ITEMS_IN_CONFIG_IDX + MAX_ITEMS_PER_SUBPAGE - 1) / MAX_ITEMS_PER_SUBPAGE;
    const size_t first_index_in_subpage = subpage_index * MAX_ITEMS_PER_SUBPAGE;
    const size_t items_in_subpage = 
        MIN(first_index_in_subpage + MAX_ITEMS_PER_SUBPAGE, MENU_TIMER_NUM_ITEMS_IN_CONFIG_IDX)
        - first_index_in_subpage;

    u8g2_SetFont(&u8g2, u8g2_font_6x13B_mr);

    for (size_t row_idx = 0; row_idx < items_in_subpage; row_idx++) {
        const MenuTimerConfigIdx_t config_idx = row_idx + first_index_in_subpage;
        switch (config_idx) {
            case MENU_TIMER_CONFIG_IDX_ACTIVE:
                snprintf(str_buf, sizeof(str_buf),
                        "Active: %c", menu_state.sel_timer_active ? 'Y' : 'N');
                break;
            case MENU_TIMER_CONFIG_IDX_DISPENSER:
                snprintf(str_buf, sizeof(str_buf),
                        "Dispenser: %c", 'A' + menu_state.sel_dispenser);
                break;
            case MENU_TIMER_CONFIG_IDX_MODE: {
                const char* mode_str =
                    menu_state.sel_mode == PILL_TIMER_MODE_RELATIVE ? "Relative" :
                    menu_state.sel_mode == PILL_TIMER_MODE_ABSOLUTE ? "Absolute" :
                                                                      "INVALID";
                snprintf(str_buf, sizeof(str_buf), "Mode: %s", mode_str);
                break;
            }
            case MENU_TIMER_CONFIG_IDX_REL_INTERVAL: {
                const uint8_t num_hours = menu_state.rel_interval / MS_IN_HOUR;
                const uint8_t num_mins = (menu_state.rel_interval % MS_IN_HOUR) / MS_IN_MINUTE;
                snprintf(str_buf, sizeof(str_buf), "[REL] Intrvl: %dh%dm", num_hours, num_mins);
                break;
            }
            case MENU_TIMER_CONFIG_IDX_REL_NUM_PER_DAY:
                snprintf(str_buf, sizeof(str_buf), "[REL] # per day: %d", menu_state.rel_num_per_day);
                break;
            case MENU_TIMER_CONFIG_IDX_ABS_TIME: {
                const uint8_t hours = menu_state.abs_time / MS_IN_HOUR;
                const uint8_t mins = (menu_state.abs_time % MS_IN_HOUR) / MS_IN_MINUTE;
                snprintf(str_buf, sizeof(str_buf), "[ABS] Time: %02d:%02d", hours, mins);
                break;
            }
            case MENU_TIMER_CONFIG_IDX_SAVE:
                snprintf(str_buf, sizeof(str_buf), "Save Timer Config");
                break;
            case MENU_TIMER_CONFIG_IDX_BACK:
                snprintf(str_buf, sizeof(str_buf), "Back (Don't Save)");
                break;
            case MENU_TIMER_NUM_ITEMS_IN_CONFIG_IDX:
                __unreachable();
                break;
        }
        u8g2_DrawStr(&u8g2, X_COL, Y_SPACING * (row_idx + 1), str_buf);
        
        if (row_idx == sel_index_in_subpage) {
            u8g2_DrawHLine(&u8g2,
                            X_COL,
                            (Y_SPACING * (row_idx + 1)) + 1,
                            u8g2_GetStrWidth(&u8g2, str_buf));
        }
    }

    if (subpage_index != 0) {
        // Add an up arrow if we're not on the first page
        u8g2_DrawTriangle(&u8g2,
            120, 6,
            128, 6,
            124, 1);
    }
    if (subpage_index != total_num_subpages - 1) {
        // Add a down arrow if we're not on the last page
        u8g2_DrawTriangle(&u8g2,
            121, DISPLAY_HEIGHT_PX - 4,
            127, DISPLAY_HEIGHT_PX - 4,
            124, DISPLAY_HEIGHT_PX - 1);
    }

}

static void display_draw_mode_menu_config_item(void) {
    bool draw_mode_header = false;
    if (menu_state.sel_index.config_idx == MENU_TIMER_CONFIG_IDX_REL_INTERVAL ||
        menu_state.sel_index.config_idx == MENU_TIMER_CONFIG_IDX_REL_NUM_PER_DAY) {
            snprintf(str_buf, sizeof(str_buf), "[Relative Mode]");
            draw_mode_header = true;
        } else if (menu_state.sel_index.config_idx == MENU_TIMER_CONFIG_IDX_ABS_TIME) {
        snprintf(str_buf, sizeof(str_buf), "[Absolute Mode]");
        draw_mode_header = true;
    }
    if (draw_mode_header) {
        u8g2_SetFont(&u8g2, u8g2_font_6x13B_mr);
        const u8g2_uint_t str_width = u8g2_GetStrWidth(&u8g2, str_buf);
        u8g2_DrawStr(&u8g2,
                    (DISPLAY_WIDTH_PX - str_width) / 2,
                    13,
                    str_buf);
    }

    switch (menu_state.sel_index.config_idx) {
        case MENU_TIMER_CONFIG_IDX_ACTIVE:
            snprintf(str_buf, sizeof(str_buf), "Active:");
            break;
        case MENU_TIMER_CONFIG_IDX_DISPENSER:
            snprintf(str_buf, sizeof(str_buf), "Dispenser:");
            break;
        case MENU_TIMER_CONFIG_IDX_MODE:
            snprintf(str_buf, sizeof(str_buf), "Timer Mode:");
            break;
        case MENU_TIMER_CONFIG_IDX_REL_INTERVAL:
            snprintf(str_buf, sizeof(str_buf), "Dose Interval:");
            break;
        case MENU_TIMER_CONFIG_IDX_REL_NUM_PER_DAY:
            snprintf(str_buf, sizeof(str_buf), "# Doses/Day:");
            break;
        case MENU_TIMER_CONFIG_IDX_ABS_TIME:
            snprintf(str_buf, sizeof(str_buf), "Ring Time:");
            break;
        case MENU_TIMER_CONFIG_IDX_SAVE:
        case MENU_TIMER_CONFIG_IDX_BACK:
        case MENU_TIMER_NUM_ITEMS_IN_CONFIG_IDX:
            __unreachable();
            break;
    }

    u8g2_SetFont(&u8g2, u8g2_font_9x18_mr);
    u8g2_uint_t str_width = u8g2_GetStrWidth(&u8g2, str_buf);
    u8g2_DrawStr(&u8g2,
                (DISPLAY_WIDTH_PX - str_width) / 2,
                31,
                str_buf);

    switch (menu_state.sel_index.config_idx) {
        case MENU_TIMER_CONFIG_IDX_ACTIVE:
            snprintf(str_buf, sizeof(str_buf), "%s",
                     menu_state.sel_timer_active ? "Yes" : "No");
            break;
        case MENU_TIMER_CONFIG_IDX_DISPENSER:
            snprintf(str_buf, sizeof(str_buf), "%c", 'A' + menu_state.sel_dispenser);
            break;
        case MENU_TIMER_CONFIG_IDX_MODE: {
            const char* mode_str =
                menu_state.sel_mode == PILL_TIMER_MODE_RELATIVE ? "Relative" :
                menu_state.sel_mode == PILL_TIMER_MODE_ABSOLUTE ? "Absolute" :
                                                                    "INVALID";
            snprintf(str_buf, sizeof(str_buf), "%s", mode_str);
            break;
        }
        case MENU_TIMER_CONFIG_IDX_REL_INTERVAL: {
            const uint8_t num_hours = menu_state.rel_interval / MS_IN_HOUR;
            const uint8_t num_mins = (menu_state.rel_interval % MS_IN_HOUR) / MS_IN_MINUTE;
            snprintf(str_buf, sizeof(str_buf), "%dhrs %02dmins", num_hours, num_mins);
            break;
        }
        case MENU_TIMER_CONFIG_IDX_REL_NUM_PER_DAY: {
            snprintf(str_buf, sizeof(str_buf), "%d", menu_state.rel_num_per_day);
            break;
        }
        case MENU_TIMER_CONFIG_IDX_ABS_TIME: {
        const uint8_t hours = menu_state.abs_time / MS_IN_HOUR;
        const uint8_t mins = (menu_state.abs_time % MS_IN_HOUR) / MS_IN_MINUTE;
            snprintf(str_buf, sizeof(str_buf), "%02d:%02d", hours, mins);
            break;
        }
        case MENU_TIMER_CONFIG_IDX_SAVE:
        case MENU_TIMER_CONFIG_IDX_BACK:
        case MENU_TIMER_NUM_ITEMS_IN_CONFIG_IDX:
            __unreachable();
            break;
    }

    u8g2_SetFont(&u8g2, u8g2_font_9x18_mr);
    str_width = u8g2_GetStrWidth(&u8g2, str_buf);
    u8g2_DrawStr(&u8g2,
                (DISPLAY_WIDTH_PX - str_width) / 2,
                50,
                str_buf);

    u8g2_DrawHLine(&u8g2,
                   (DISPLAY_WIDTH_PX - str_width) / 2,
                   51,
                   str_width);

    if (menu_state.config_item_has_up) {
        u8g2_DrawTriangle(&u8g2,
            117, 43, // Up direct has to start wider than down for some reason
            125, 43,
            121, 37);
    }
    if (menu_state.config_item_has_down) {
        u8g2_DrawTriangle(&u8g2,
            118, 45,
            125, 45,
            121, 50);
    }
}
