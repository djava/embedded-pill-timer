#include "buzzer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/projdefs.h"
#include "freertos/queue.h"
#include "driver/gpio.h"

#include "defines.h"
#include "hal/gpio_types.h"
#include "pill_timer_mgr.h"
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
    PILL_TIMER_EVENT_RINGING_TIMEOUT,
    PILL_TIMER_EVENT_MIDNIGHT_RESET,
} PillTimerEventType_t;

typedef struct {
    PillTimer_t* pt;
    PillTimerEventType_t type;
} PillTimerEvent_t;


static QueueHandle_t pill_timer_event_queue;

static void pill_timer_mgr_task(void*);
static void start_timer_ringing(PillTimer_t* pt);
static void stop_timer_ringing(PillTimer_t* pt);
static bool is_timer_up(const PillTimer_t *pt, time_in_day_ms_t current_time);
static void midnight_reset_timer(PillTimer_t* pt);
static void pill_timer_time_check_task(void*);
static void switch_isr_callback [[maybe_unused]] (void* switch_num_cast_to_dispenser_idx_t);
static void timeout_timer_callback(TimerHandle_t timer_handle);

void pill_timer_mgr_init(void) {
    const gpio_config_t switch_gpio_config = {
        .mode = GPIO_MODE_INPUT,
        .intr_type = GPIO_INTR_POSEDGE,
        .pin_bit_mask = (1ULL << GPIO_PIN_DISPENSER_A) | (1ULL << GPIO_PIN_DISPENSER_B),
        .pull_down_en = GPIO_PULLUP_DISABLE,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&switch_gpio_config);

    // gpio_isr_handler_add(GPIO_PIN_DISPENSER_A, 
    //                      switch_isr_callback,
    //                      (void*)(PILL_DISPENSER_IDX_A));
    // gpio_isr_handler_add(GPIO_PIN_DISPENSER_B,
    //                      switch_isr_callback,
    //                      (void*)(PILL_DISPENSER_IDX_B));

    memset(&pill_timers, 0, sizeof(pill_timers));
    for (size_t i = 0; i < NUM_PILL_TIMERS; i++) {
        PillTimer_t* pt = &pill_timers[i];
        pt->timeout_timer_handle = xTimerCreate("Timeout",
            pdMS_TO_TICKS(PILL_TIMER_TIMEOUT_DURATION_MS),
            pdFALSE,
            pt,
            timeout_timer_callback
        );
    }

    // TODO: Load froM NVM

    pill_timer_event_queue = xQueueCreate(PILL_TIMER_EVENT_QUEUE_LENGTH, sizeof(PillTimerEvent_t));
    pill_timer_mutex = xSemaphoreCreateMutex();

    xTaskCreate(
        pill_timer_mgr_task,
        "Pill Timer Manager Task",
        4096,
        NULL,
        TASK_PRIORITY_MED,
        NULL
    );

    xTaskCreate(
        pill_timer_time_check_task,
        "Pill Clock Checker Task",
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

    // TODO: Save to NVM

    xSemaphoreGive(pill_timer_mutex);
}

void pill_timer_set_relative(size_t timer, DispenserIdx_t disp, duration_ms_t interval, uint8_t num_per_day) {
    xSemaphoreTake(pill_timer_mutex, portMAX_DELAY);
    
    pill_timers[timer].active = true;
    pill_timers[timer].ringing = false;
    pill_timers[timer].dispenser_idx = disp;
    pill_timers[timer].mode = PILL_TIMER_MODE_RELATIVE;

    pill_timers[timer].relative.num_per_day = num_per_day;
    pill_timers[timer].relative.interval = interval;

    pill_timers[timer].relative.today_num_times_rang = 0;
    pill_timers[timer].relative.today_time_last_rang = UINT32_MAX;

    // TODO: Save to NVM

    xSemaphoreGive(pill_timer_mutex);
}

void pill_timer_disable(size_t timer) {
    xSemaphoreTake(pill_timer_mutex, portMAX_DELAY);

    pill_timers[timer].active = false;
    pill_timers[timer].ringing = false;

    // TODO: Save to NVM

    xSemaphoreGive(pill_timer_mutex);
}

duration_ms_t pill_timer_get_next_to_ring(PillTimer_t** out_pt) {
    duration_ms_t shortest_time_until = UINT32_MAX;
    PillTimer_t* shortest_time_until_pt = NULL;

    const time_in_day_ms_t current_time = rtc_get_time_in_day_ms();
    for (size_t i = 0; i < NUM_PILL_TIMERS; i++) {
        PillTimer_t* pt = &pill_timers[i];
        if (!pt->active) { continue; }

        duration_ms_t time_until = UINT32_MAX;
        if (pt->mode == PILL_TIMER_MODE_ABSOLUTE) {
            if (!pt->absolute.today_timer_happened) {
                time_until = pt->absolute.time - current_time;
            }
        } else if (pt->mode == PILL_TIMER_MODE_RELATIVE) {
            if (pt->relative.today_time_last_rang == UINT32_MAX) {
                // Hasn't been taken yet - set it as now
                time_until = 0;
            }
            else if (pt->relative.today_num_times_rang < pt->relative.num_per_day &&
                     current_time > pt->relative.today_time_last_rang) {
                const duration_ms_t time_since = current_time - pt->relative.today_time_last_rang;
                time_until = pt->relative.interval - time_since;
            }
        }

        if (time_until < shortest_time_until) {
            shortest_time_until = time_until;
            shortest_time_until_pt = pt;
        }
    }
    
    if (out_pt) { *out_pt = shortest_time_until_pt; }
    return shortest_time_until;
}

const PillTimer_t* pill_timer_get_timer(size_t timer) {
    if (timer >= NUM_PILL_TIMERS) { return NULL; }
    return &pill_timers[timer];
}

static void pill_timer_mgr_task(void*) {
    PillTimerEvent_t event;
    while (true) {
        xQueueReceive(pill_timer_event_queue, &event, portMAX_DELAY);
        
        PillTimer_t* pt = event.pt;
        if (pt && !pt->active) { continue; }

        xSemaphoreTake(pill_timer_mutex, portMAX_DELAY);
        switch (event.type) {
            case PILL_TIMER_EVENT_TIMER_UP: {
                if (pt->active && !pt->ringing) {
                    start_timer_ringing(pt);
                }
                break;
            }
            case PILL_TIMER_EVENT_DISPENSER_OPEN: {
                if (pt->active) {
                    if (pt->ringing) {
                        stop_timer_ringing(pt);
                    } else if (pt->mode == PILL_TIMER_MODE_RELATIVE && pt->relative.today_num_times_rang == 0) {
                        pt->relative.today_time_last_rang = rtc_get_time_in_day_ms();
                        // TODO: Buzzer or maybe screen element to note
                        //       relative timer started?
                    }
                }
                break;
            }
            case PILL_TIMER_EVENT_RINGING_TIMEOUT: {
                if (pt->active && pt->ringing) {
                    stop_timer_ringing(pt);
                    // TODO: Log missed timer
                }
                break;
            }
            case PILL_TIMER_EVENT_MIDNIGHT_RESET: {
                for (size_t i = 0; i < NUM_PILL_TIMERS; i++) {
                    midnight_reset_timer(&pill_timers[i]);
                }
                break;
            }
        }

        xSemaphoreGive(pill_timer_mutex);
    }
}

static void pill_timer_time_check_task(void*) {
    time_in_day_ms_t last_time_checked = rtc_get_time_in_day_ms();

    while (true) {
        const time_in_day_ms_t current_time = rtc_get_time_in_day_ms();

        xSemaphoreTake(pill_timer_mutex, portMAX_DELAY);

        if (last_time_checked > current_time) {
            // Last time-of-day is greater than this time-of-day - must
            // have clicked over midnight. Trigger a midnight reset.
            const PillTimerEvent_t event = {
                .pt = NULL,
                .type = PILL_TIMER_EVENT_MIDNIGHT_RESET
            };
            xQueueSend(pill_timer_event_queue, &event, 0);
        }

        for (size_t i = 0; i < NUM_PILL_TIMERS; i++) {
            PillTimer_t* pt = &pill_timers[i];
            if (is_timer_up(pt, current_time)) {
                const PillTimerEvent_t event = {
                    .pt = pt,
                    .type = PILL_TIMER_EVENT_TIMER_UP
                };
                xQueueSend(pill_timer_event_queue, &event, 0);
            }
        }

        xSemaphoreGive(pill_timer_mutex);
        last_time_checked = current_time;
        vTaskDelay(pdMS_TO_TICKS(PILL_TIMER_CLOCK_CHECK_FREQ_MS));
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

    xTimerStart(pt->timeout_timer_handle, 0);

    buzzer_set_event(DISPENSER_TO_RINGING_EVENT[pt->dispenser_idx]);
}

static void stop_timer_ringing(PillTimer_t* pt) {
    pt->ringing = false;
    xTimerStop(pt->timeout_timer_handle, 0);

    buzzer_clear_event(DISPENSER_TO_RINGING_EVENT[pt->dispenser_idx]);
}

static bool is_timer_up(const PillTimer_t *pt, time_in_day_ms_t current_time) {
    if (pt->active && !pt->ringing) {
        if (pt->mode == PILL_TIMER_MODE_ABSOLUTE) {
            if (current_time >= pt->absolute.time && !pt->absolute.today_timer_happened) {
                return true;
            }
        } else if (pt->mode == PILL_TIMER_MODE_RELATIVE) {
            if (pt->relative.today_num_times_rang < pt->relative.num_per_day) {
                // Guard against current_time = UINT32_MAX or other edge
                // cases causing underflow
                if (current_time > pt->relative.today_time_last_rang) {
                    const time_in_day_ms_t time_since_last =
                        current_time - pt->relative.today_time_last_rang;

                    if (time_since_last > pt->relative.interval) {
                        return true;
                    }
                }
            }
        }
    }

    return false;
}

static void midnight_reset_timer(PillTimer_t* pt) {
    if (!pt->active) { return; }

    if (pt->ringing) {
        // Obviously this is a sloppy way to do this, but there are a
        // bunch of edge cases here so easier to just not. The entire
        // concept of of a midnight reset is already kind of sloppy so
        // whatever.
        stop_timer_ringing(pt);
    }

    if (pt->mode == PILL_TIMER_MODE_ABSOLUTE) {
        pt->absolute.today_timer_happened = false;
    } else if (pt->mode == PILL_TIMER_MODE_RELATIVE) {
        pt->relative.today_num_times_rang = 0;
        pt->relative.today_time_last_rang = UINT32_MAX;
    }
}

static void switch_isr_callback(void* disp_idx_cast_to_dispenser_idx) {
    const DispenserIdx_t disp_idx = (DispenserIdx_t)(disp_idx_cast_to_dispenser_idx);
    
    BaseType_t higherPriorityTaskWoken = pdFALSE;
    for (size_t i = 0; i < NUM_PILL_TIMERS; i++) {
        PillTimer_t *pt = &pill_timers[i];
        
        // Minor race condition here  but we can't do mutex in an ISR
        if (pt->active && pt->dispenser_idx == disp_idx) {
            const PillTimerEvent_t event = {
                .type = PILL_TIMER_EVENT_DISPENSER_OPEN,
                .pt = pt
            };
            xQueueSendFromISR(pill_timer_event_queue, &event, &higherPriorityTaskWoken);
        }
    }

    buzzer_set_event_from_isr(BUZZER_EVENT_DISPENSER_OPEN,
                              &higherPriorityTaskWoken);

    if (higherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}

static void timeout_timer_callback(TimerHandle_t timer_handle) {
    PillTimer_t* pt = pvTimerGetTimerID(timer_handle);

    const PillTimerEvent_t event = {
        .type = PILL_TIMER_EVENT_RINGING_TIMEOUT,
        .pt = pt
    };
    xQueueSend(pill_timer_event_queue, &event, 0);
}

void pill_timer_mgr_inject_dispenser_open(DispenserIdx_t disp_idx) {
    for (size_t i = 0; i < NUM_PILL_TIMERS; i++) {
        PillTimer_t *pt = &pill_timers[i];
        if (pt->active && pt->dispenser_idx == disp_idx) {
            const PillTimerEvent_t event = {
                .type = PILL_TIMER_EVENT_DISPENSER_OPEN,
                .pt = pt
            };
            xQueueSend(pill_timer_event_queue, &event, 0);
        }
    }
    
    buzzer_set_event(BUZZER_EVENT_DISPENSER_OPEN);
}