#ifndef MENUS_H
#define MENUS_H

#include "defines.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "pill_timer_mgr.h"

#define MENU_SEL_TIMER_BACK_IDX (NUM_PILL_TIMERS)

typedef enum {
    BUTTON_DOWN,
    BUTTON_UP,
    BUTTON_OK,
} ButtonType_t;

void menus_init(void);
void menus_inject_button(ButtonType_t button);

typedef enum {
    MENU_PAGE_TIMER_LIST,
    MENU_PAGE_CONFIG_LIST,
    MENU_PAGE_CONFIG_ITEM
} MenuPage_t;

typedef enum {
    MENU_TIMER_CONFIG_IDX_ACTIVE = 0,
    MENU_TIMER_CONFIG_IDX_DISPENSER,
    MENU_TIMER_CONFIG_IDX_MODE,
    MENU_TIMER_CONFIG_IDX_REL_INTERVAL,
    MENU_TIMER_CONFIG_IDX_REL_NUM_PER_DAY,
    MENU_TIMER_CONFIG_IDX_ABS_TIME,
    MENU_TIMER_CONFIG_IDX_SAVE,
    MENU_TIMER_CONFIG_IDX_BACK,
} MenuTimerConfigIdx_t;

typedef struct {
    MenuPage_t page;
    union {
        size_t timer_num;
        MenuTimerConfigIdx_t config_idx;
    } sel_index;

    size_t sel_timer;
    bool sel_timer_active;
    DispenserIdx_t sel_dispenser;
    PillTimerMode_t sel_mode;
    
    uint8_t rel_num_per_day;
    duration_ms_t rel_interval;

    time_in_day_ms_t abs_time;
} MenuState_t;


extern MenuState_t menu_state;
extern SemaphoreHandle_t menu_state_mutex;

#endif /* MENUS_H */
