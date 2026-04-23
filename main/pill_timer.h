#ifndef PILL_TIMER_H
#define PILL_TIMER_H

#include "defines.h"

typedef enum {
    PILL_TIMER_MODE_RELATIVE,
    PILL_TIMER_MODE_ABSOLUTE,
} PillTimerMode_t;

typedef struct {
    bool active;
    PillTimerMode_t mode;
    dispenser_idx_t dispenser_idx;
    bool ringing;

    union {
        struct {
            duration_ms_t time_between;
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

extern PillTimer_t pill_timers[NUM_PILL_TIMERS];

#endif