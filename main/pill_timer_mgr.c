#include "freertos/freertos.h"
#include "freertos/projdefs.h"
#include "freertos/queue.h"
#include "driver/gpio.h"

#include "defines.h"
#include "pill_timer.h"
#include "portmacro.h"
#include "rtc.h"
#include <string.h>

#define PILL_TIMER_CLOCK_CHECK_FREQ_MS (500)
#define PILL_TIMER_EVENT_QUEUE_LENGTH (5)

static PillTimer_t pill_timers[NUM_PILL_TIMERS];
static SemaphoreHandle_t pill_timer_mutex;

typedef enum {
    PILL_TIMER_EVENT_TIMER_UP,
    PILL_TIMER_EVENT_DISPENSER_OPEN,
    PILL_TIMER_EVENT_DISPENSER_CLOSE,
    PILL_TIMER_EVENT_RINGING_TIMEOUT,
    PILL_TIMER_EVENT_MIDNIGHT_RESET,
} PillTimerEventType_t;

typedef struct {
    size_t timer_idx;
    PillTimerEventType_t type;
} PillTimerEvent_t;


static QueueHandle_t pill_timer_event_queue;

static void pill_timer_mgr_task(void*);
static void pill_timer_mgr_handle_event(PillTimerEvent_t *event);
static void start_timer_ringing(PillTimer_t* pt);
static void stop_timer_ringing(PillTimer_t* pt);
static bool is_timer_up(PillTimer_t *pt);
static void pill_timer_time_check_task(void*);
static void switch_isr_callback(void* switch_num_cast_to_dispenser_idx_t);

void pill_timer_mgr_init(void) {
    const gpio_config_t switch_gpio_config = {
        .mode = GPIO_MODE_INPUT,
        .intr_type = GPIO_INTR_ANYEDGE,
        .pin_bit_mask = (1ULL << GPIO_PIN_DISPENSER_A) | (1ULL << GPIO_PIN_DISPENSER_B),
        .pull_down_en = GPIO_PULLUP_DISABLE,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&switch_gpio_config);

    gpio_install_isr_service(ESP_INTR_FLAG_EDGE);
    gpio_isr_handler_add(GPIO_PIN_DISPENSER_A, 
                         switch_isr_callback,
                         (void*)(PILL_DISPENSER_IDX_A));
    gpio_isr_handler_add(GPIO_PIN_DISPENSER_B,
                         switch_isr_callback,
                         (void*)(PILL_DISPENSER_IDX_B));

    memset(&pill_timers, 0, sizeof(pill_timers));
    // TODO: Load froM NVM

    pill_timer_event_queue = xQueueCreate(PILL_TIMER_EVENT_QUEUE_LENGTH, sizeof(PillTimerEvent_t));
    pill_timer_mutex = xSemaphoreCreateMutex();

    xTaskCreate(
        pill_timer_mgr_task,
        "Pill Timer Manager Task",
        4096,
        NULL,
        TASK_PRIORITY_HIGH,
        NULL
    );

    xTaskCreate(
        pill_timer_time_check_task,
        "Pill Timer Clock Checker Task",
        2048,
        NULL,
        TASK_PRIORITY_MED,
        NULL
    );
}

void pill_timer_set_absolute(size_t timer, DispenserIdx_t disp, time_in_day_ms_t time_in_day) {
    xSemaphoreTake(pill_timer_mutex, portMAX_DELAY);

    pill_timers[timer].active = true;
    pill_timers[timer].ringing = false;
    pill_timers[timer].dispenser_idx = disp;
    pill_timers[timer].mode = PILL_TIMER_MODE_ABSOLUTE;
    pill_timers[timer].absolute.time = time_in_day;
    pill_timers[timer].absolute.today_timer_happened = false;

    xSemaphoreGive(pill_timer_mutex);
}

void pill_timer_set_relative(size_t timer, DispenserIdx_t disp, duration_ms_t interval, uint8_t num_per_day) {
    xSemaphoreTake(pill_timer_mutex, portMAX_DELAY);
    
    pill_timers[timer].active = true;
    pill_timers[timer].ringing = false;
    pill_timers[timer].dispenser_idx = disp;
    pill_timers[timer].mode = PILL_TIMER_MODE_RELATIVE;
    pill_timers[timer].relative.num_per_day = num_per_day;
    pill_timers[timer].relative.time_between = interval;
    pill_timers[timer].relative.today_num_times_rang = 0;
    pill_timers[timer].relative.today_time_last_rang = UINT32_MAX;

    xSemaphoreGive(pill_timer_mutex);
}

void pill_timer_disable(size_t timer) {
    xSemaphoreTake(pill_timer_mutex, portMAX_DELAY);
    
    memset(&pill_timers[timer], 0, sizeof(PillTimer_t));

    xSemaphoreGive(pill_timer_mutex);
}

static void pill_timer_mgr_task(void*) {
    PillTimerEvent_t event;
    while (true) {
        xQueueReceive(pill_timer_event_queue, &event, portMAX_DELAY);
        xSemaphoreTake(pill_timer_mutex, portMAX_DELAY);
        pill_timer_mgr_handle_event(&event);
        xSemaphoreGive(pill_timer_mutex);
    }
}

static void pill_timer_mgr_handle_event(PillTimerEvent_t *event) {
    PillTimer_t* pt = &pill_timers[event->timer_idx];
    if (!pt->active) { return; }

    switch (event->type) {
        case PILL_TIMER_EVENT_TIMER_UP: {
            if (!pt->ringing) {
                start_timer_ringing(pt);
            }
            break;
        }
        case PILL_TIMER_EVENT_DISPENSER_CLOSE: break;
        case PILL_TIMER_EVENT_DISPENSER_OPEN: break;
        case PILL_TIMER_EVENT_RINGING_TIMEOUT: break;
        case PILL_TIMER_EVENT_MIDNIGHT_RESET: break;
    }
}

static void pill_timer_time_check_task(void*) {
    TickType_t last_tick;
    while (true) {
        last_tick = xTaskGetTickCount();
        xSemaphoreTake(pill_timer_mutex, portMAX_DELAY);

        for (size_t i = 0; i < NUM_PILL_TIMERS; i++) {
            PillTimer_t* pt = &pill_timers[i];
            if (is_timer_up(pt)) {
                const PillTimerEvent_t event = {
                    .timer_idx = i,
                    .type = PILL_TIMER_EVENT_TIMER_UP
                };
                xQueueSend(pill_timer_event_queue, &event, 0);
            }
        }

        xSemaphoreGive(pill_timer_mutex);
        xTaskDelayUntil(&last_tick, pdMS_TO_TICKS(PILL_TIMER_CLOCK_CHECK_FREQ_MS));
    }
}

static void start_timer_ringing(PillTimer_t* pt) {
    pt->ringing = true;
    if (pt->mode == PILL_TIMER_MODE_ABSOLUTE) {
        pt->absolute.today_timer_happened = true;
    } else if (pt->mode == PILL_TIMER_MODE_RELATIVE) {
        pt->relative.today_num_times_rang++;
        pt->relative.today_time_last_rang = rtc_get_time_in_day_ms();
    }
    // TODO: Start buzzer
}

static void stop_timer_ringing(PillTimer_t* pt) {
    pt->ringing = true;
    if (pt->mode == PILL_TIMER_MODE_ABSOLUTE) {
        pt->absolute.today_timer_happened = true;
    } else if (pt->mode == PILL_TIMER_MODE_RELATIVE) {
        pt->relative.today_num_times_rang++;
        pt->relative.today_time_last_rang = rtc_get_time_in_day_ms();
    }
    // TODO: Stop buzzer
}

static bool is_timer_up(PillTimer_t *pt) {
    if (pt->active && !pt->ringing) {
        const time_in_day_ms_t current_time = rtc_get_time_in_day_ms();
        if (pt->mode == PILL_TIMER_MODE_ABSOLUTE) {
            if (current_time > pt->absolute.time && !pt->absolute.today_timer_happened) {
                return true;
            }
        } else if (pt->mode == PILL_TIMER_MODE_RELATIVE) {
            if (pt->relative.today_num_times_rang < pt->relative.num_per_day) {
                const time_in_day_ms_t time_since_last =
                    current_time - pt->relative.today_time_last_rang;

                if (time_since_last > pt->relative.time_between) {
                    return true;
                }
            }
        }
    }

    return false;
}

static void switch_isr_callback(void* disp_idx_cast_to_dispenser_idx) {
    const DispenserIdx_t disp_idx = (DispenserIdx_t)(disp_idx_cast_to_dispenser_idx);
    bool switch_state = gpio_get_level(DISPENSER_TO_GPIO_PIN[disp_idx]);
    PillTimerEvent_t event = {
        .type = switch_state ? PILL_TIMER_EVENT_DISPENSER_OPEN : PILL_TIMER_EVENT_DISPENSER_CLOSE,
    };

    BaseType_t higherPriorityTaskWoken;
    for (size_t i = 0; i < NUM_PILL_TIMERS; i++) {
        PillTimer_t *pt = &pill_timers[i];
        
        // Minor race condition here  but we can't do mutex in an ISR
        if (pt->active && pt->dispenser_idx == disp_idx) {
            event.timer_idx = i;
            xQueueSendFromISR(pill_timer_event_queue, &event, &higherPriorityTaskWoken);
        }
    }

    if (higherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}