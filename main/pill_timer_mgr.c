#include "defines.h"
#include "freertos/projdefs.h"
#include "pill_timer.h"
#include "portmacro.h"
#include "rtc.h"

#define PILL_TIMER_MGR_TASK_MIN_FREQ_MS (500)

PillTimer_t pill_timers[NUM_PILL_TIMERS];

static void pill_timer_mgr_task(void*);
static bool ring_timer_if_ready(PillTimer_t *pt);

void pill_timer_mgr_init(void) {
    xTaskCreate(
        pill_timer_mgr_task,
        "Pill Timer Manager Task",
        4096,
        NULL,
        TASK_PRIORITY_HIGH,
        NULL
    );
}

static void pill_timer_mgr_task(void*) {
    while (true) {
        ulTaskNotifyTake(false, pdMS_TO_TICKS(PILL_TIMER_MGR_TASK_MIN_FREQ_MS));
        for (size_t i = 0; i < NUM_PILL_TIMERS; i++) {
            if (ring_timer_if_ready(&pill_timers[i])) {
        
            }
        }

    }
}

static bool ring_timer_if_ready(PillTimer_t *pt) {
    if (pt->active && !pt->ringing) {
        const time_in_day_ms_t current_time = rtc_get_time_in_day_ms();
        if (pt->mode == PILL_TIMER_MODE_ABSOLUTE) {
            if (current_time > pt->absolute.time && !pt->absolute.today_timer_happened) {
                pt->absolute.today_timer_happened = true;
                pt->ringing = true;
                return true;
            }
        } else if (pt->mode == PILL_TIMER_MODE_RELATIVE) {
            if (pt->relative.today_num_times_rang < pt->relative.num_per_day) {
                const time_in_day_ms_t time_since_last =
                    current_time - pt->relative.today_time_last_rang;

                if (time_since_last > pt->relative.time_between) {
                    pt->relative.today_time_last_rang = current_time;
                    pt->relative.today_num_times_rang++;
                    pt->ringing = true;
                    return true;
                }
            }
        }
    }

    return false;
}