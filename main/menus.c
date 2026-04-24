#include "display.h"
#include "driver/gpio.h"

#include "defines.h"
#include "menus.h"
#include "pill_timer_mgr.h"
#include "portmacro.h"
#include <string.h>
#include <sys/cdefs.h>

typedef enum {
    BUTTON_DOWN,
    BUTTON_UP,
    BUTTON_OK,
} ButtonType_t;

#define BUTTON_QUEUE_LEN (8)

MenuState_t menu_state;
SemaphoreHandle_t menu_state_mutex;

static QueueHandle_t button_queue;

static void menu_task(void*);
static void button_isr_handler(void* button_arg);
static void reset_menu_state();
static void save_configured_timer();

void menus_init(void) {
    button_queue = xQueueCreate(BUTTON_QUEUE_LEN, sizeof(ButtonType_t));

    const gpio_config_t button_config = {
        .pin_bit_mask = (1 << GPIO_PIN_MENU_BUTTON_DOWN) |
                        (1 << GPIO_PIN_MENU_BUTTON_UP)   |
                        (1 << GPIO_PIN_MENU_BUTTON_OK),
        .mode = GPIO_MODE_INPUT,
        .intr_type = GPIO_INTR_POSEDGE,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLUP_DISABLE,
    };

    gpio_config(&button_config);
    gpio_install_isr_service(ESP_INTR_FLAG_EDGE);
    gpio_isr_handler_add(GPIO_PIN_MENU_BUTTON_DOWN, button_isr_handler, (void*)BUTTON_DOWN);
    gpio_isr_handler_add(GPIO_PIN_MENU_BUTTON_UP, button_isr_handler, (void*)BUTTON_UP);
    gpio_isr_handler_add(GPIO_PIN_MENU_BUTTON_OK, button_isr_handler, (void*)BUTTON_OK);

    reset_menu_state();

    menu_state_mutex = xSemaphoreCreateMutex();

    xTaskCreate(
        menu_task,
        "Menu Task",
        2048,
        NULL,
        TASK_PRIORITY_MED,
        NULL
    );
}

static void menu_task(void*) {
    ButtonType_t button;

    while (true) {
        xSemaphoreGive(menu_state_mutex);
        xQueueReceive(button_queue, &button, portMAX_DELAY);
        xSemaphoreTake(menu_state_mutex, portMAX_DELAY);

        if (display_mode == DISPLAY_MODE_CLOCK) {
            // If we're still on clock, switch to menu
            reset_menu_state();
            display_mode = DISPLAY_MODE_MENU;
            continue;
        } else if (display_mode == DISPLAY_MODE_RINGING) {
            // Don't exit ringing mode through buttons
            continue;
        }

        if (menu_state.page == MENU_PAGE_TIMER_LIST) {
            size_t *const timer_idx = &menu_state.sel_index.timer_num;
            switch (button) {
                case BUTTON_DOWN:
                    // Button down: Cycle through timers forwards,
                    // wrapping to 'back' button
                    (*timer_idx)++;
                    if (*timer_idx > MENU_SEL_TIMER_BACK_IDX) {
                        *timer_idx = 0;
                    }
                    break;
                case BUTTON_UP:
                    // Button down: Cycle through timers backwards,
                    // wrapping to 'back' button
                    if (*timer_idx == 0) {
                        *timer_idx = MENU_SEL_TIMER_BACK_IDX;
                    }
                    (*timer_idx)--;
                    break;
                case BUTTON_OK:
                    // Button ok: If this is 'back' button then go back
                    // to clock, otherwise continue into the selected
                    // timer's configuration menu
                    if (*timer_idx == MENU_SEL_TIMER_BACK_IDX) {
                        display_mode = DISPLAY_MODE_CLOCK;
                    } else {
                        menu_state.sel_timer = *timer_idx;
                        menu_state.sel_index.config_idx = 0;
                        menu_state.page = MENU_PAGE_CONFIG_LIST;
                    }
                    break;
            }
        } else if (menu_state.page == MENU_PAGE_CONFIG_LIST) {
            MenuTimerConfigIdx_t *const config_idx = &menu_state.sel_index.config_idx;
            switch (button) {
                case BUTTON_DOWN:
                    // Button down: Cycle through config items forwards,
                    // wrapping to 'back' button
                    (*config_idx)++;
                    if (*config_idx > MENU_TIMER_CONFIG_IDX_BACK) {
                        *config_idx = 0;
                    }
                    break;
                case BUTTON_UP:
                    // Button up: Cycle through config items backwards,
                    // wrapping to 'back' button
                    if (*config_idx == 0) {
                        *config_idx = MENU_TIMER_CONFIG_IDX_BACK;
                    }
                    (*config_idx)--;
                    break;
                case BUTTON_OK:
                    // Button ok: Go back if config item is back, save
                    // if config item is save, otherwise enter the
                    // selected config item page
                    if (*config_idx == MENU_TIMER_CONFIG_IDX_BACK) {
                        menu_state.sel_index.timer_num = menu_state.sel_timer;
                        menu_state.page = MENU_PAGE_TIMER_LIST;
                    } else if (*config_idx == MENU_TIMER_CONFIG_IDX_SAVE) {
                        save_configured_timer();
                    } else {
                        menu_state.page = MENU_PAGE_CONFIG_ITEM;
                    }
                    break;
            }
        } else if (menu_state.page == MENU_PAGE_CONFIG_ITEM) {
            if (button == BUTTON_OK) {
                menu_state.page = MENU_PAGE_CONFIG_LIST;
                continue;
            }

            switch (menu_state.sel_index.config_idx) {
                case MENU_TIMER_CONFIG_IDX_ACTIVE:
                    if (button == BUTTON_UP || button == BUTTON_DOWN) {
                        menu_state.sel_timer_active = !menu_state.sel_timer_active;
                    }
                    break;
                case MENU_TIMER_CONFIG_IDX_DISPENSER:
                    if (button == BUTTON_UP) {
                        menu_state.sel_dispenser--;
                        if (menu_state.sel_dispenser == PILL_DISPENSER_IDX_INVALID) {
                            menu_state.sel_dispenser = NUM_PILL_DISPENSERS;
                        }
                    } else if (button == BUTTON_DOWN) {
                        menu_state.sel_dispenser++;
                        if (menu_state.sel_dispenser > NUM_PILL_DISPENSERS) {
                            menu_state.sel_dispenser = PILL_DISPENSER_IDX_A;
                        }
                    }
                    break;
                case MENU_TIMER_CONFIG_IDX_MODE:
                    if (button == BUTTON_UP || button == BUTTON_DOWN) {
                        menu_state.sel_mode = (menu_state.sel_mode == PILL_TIMER_MODE_RELATIVE) ?
                                               PILL_TIMER_MODE_ABSOLUTE : PILL_TIMER_MODE_RELATIVE;
                    }
                    break;
                case MENU_TIMER_CONFIG_IDX_REL_INTERVAL:
                    // Incr/decrement by REL_INTERVAL_CONFIG_INTERVAL_MS
                    if (button == BUTTON_UP) {
                        menu_state.rel_interval += REL_INTERVAL_CONFIG_INTERVAL_MS;
                        // Don't let it go above REL_INTERVAL_CONFIG_MAX_MS
                        if (menu_state.rel_interval >= REL_INTERVAL_CONFIG_MAX_MS) {
                            menu_state.rel_interval = REL_INTERVAL_CONFIG_MAX_MS;
                        }
                    } else if (button == BUTTON_DOWN) {
                        // Don't let it go below 0
                        if (menu_state.rel_interval >= REL_INTERVAL_CONFIG_INTERVAL_MS) {
                            menu_state.rel_interval -= REL_INTERVAL_CONFIG_INTERVAL_MS;
                        } else {
                            menu_state.rel_interval = 0;
                        }
                    }

                    break;
                case MENU_TIMER_CONFIG_IDX_REL_NUM_PER_DAY:
                    if (button == BUTTON_UP) {
                        menu_state.rel_num_per_day++;
                        if (menu_state.rel_num_per_day >= REL_NUM_PER_DAY_CONFIG_MAX) {
                            menu_state.rel_num_per_day = REL_NUM_PER_DAY_CONFIG_MAX;
                        }
                    } else if (button == BUTTON_DOWN) {
                        if (menu_state.rel_num_per_day > 0) {
                            menu_state.rel_num_per_day--;
                        }
                    }
                    break;
                
                case MENU_TIMER_CONFIG_IDX_ABS_TIME:
                    if (button == BUTTON_UP) {
                        menu_state.abs_time += ABS_TIME_CONFIG_INTERVAL_MS;
                        // Don't let it go above ABS_TIME_CONFIG_MAX_MS (23:59)
                        if (menu_state.abs_time >= ABS_TIME_CONFIG_MAX_MS) {
                            menu_state.abs_time = ABS_TIME_CONFIG_MAX_MS;
                        }
                    } else if (button == BUTTON_DOWN) {
                        // Don't let it go below 0
                        if (menu_state.abs_time >= ABS_TIME_CONFIG_INTERVAL_MS) {
                            menu_state.abs_time -= ABS_TIME_CONFIG_INTERVAL_MS;
                        } else {
                            menu_state.abs_time = 0;
                        }
                    }
                    break;
                
                case MENU_TIMER_CONFIG_IDX_SAVE:
                case MENU_TIMER_CONFIG_IDX_BACK:
                    __unreachable();
                    break;
            }
        }
    }
}

static void button_isr_handler(void* button_cast_to_buttontype) {
    const ButtonType_t button = (ButtonType_t)(button_cast_to_buttontype);

    BaseType_t higher_priority_was_woken;
    xQueueSendFromISR(button_queue, &button, &higher_priority_was_woken);

    if (higher_priority_was_woken) {
        portYIELD_FROM_ISR();
    }
}

static void reset_menu_state(void) {
    menu_state.page = MENU_PAGE_TIMER_LIST;
    menu_state.sel_index.timer_num = 0;

    menu_state.sel_timer = 0;
    menu_state.sel_dispenser = 0;
    menu_state.sel_mode = PILL_TIMER_MODE_RELATIVE;
    
    menu_state.rel_num_per_day = 0;
    menu_state.rel_interval = 0;

    menu_state.abs_time = 0;
}

static void save_configured_timer() {
    if (menu_state.sel_timer_active) {
        switch (menu_state.sel_mode) {
            case PILL_TIMER_MODE_RELATIVE:
                pill_timer_set_relative(menu_state.sel_timer,
                                        menu_state.sel_dispenser,
                                        menu_state.rel_interval,
                                        menu_state.rel_num_per_day);
                                        break;
            case PILL_TIMER_MODE_ABSOLUTE:
                pill_timer_set_absolute(menu_state.sel_timer,
                                        menu_state.sel_dispenser,
                                        menu_state.abs_time);
                break;
        }
    } else {
        pill_timer_disable(menu_state.sel_timer);
    }
}