#ifndef PILL_TIMER_MGR_H
#define PILL_TIMER_MGR_H

#include "defines.h"

typedef enum {
    PILL_TIMER_MODE_RELATIVE,
    PILL_TIMER_MODE_ABSOLUTE,
} PillTimerMode_t;

typedef struct {
    bool active;
    PillTimerMode_t mode;
    DispenserIdx_t dispenser_idx;
    bool ringing;
    TimerHandle_t timeout_timer_handle;

    union {
        struct {
            duration_ms_t interval;
            uint8_t num_per_day;

            time_in_day_ms_t today_time_last_rang;
            size_t today_num_times_rang;
        } relative;
        struct {
            time_in_day_ms_t time;
            bool today_timer_happened;
        } absolute;
    };
} PillTimer_t;

void pill_timer_mgr_init(void);
void pill_timer_set_absolute(size_t timer, DispenserIdx_t disp, time_in_day_ms_t time_in_day);
void pill_timer_set_relative(size_t timer, DispenserIdx_t disp, duration_ms_t interval, uint8_t num_per_day);
void pill_timer_disable(size_t timer);

duration_ms_t pill_timer_get_next_to_ring(PillTimer_t** out_pt);

#endif