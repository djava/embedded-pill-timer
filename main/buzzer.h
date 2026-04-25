#ifndef BUZZER_H
#define BUZZER_H
#include "defines.h"
#include <stdint.h>

void buzzer_init();

#define BUZZER_EVENT_RINGING_A (1 << 0)
#define BUZZER_EVENT_RINGING_B (1 << 1)
#define BUZZER_EVENT_DISPENSER_OPEN (1 << 2)
#define BUZZER_EVENT_BUTTON_PRESS (1 << 3)

extern const EventBits_t DISPENSER_TO_RINGING_EVENT[NUM_PILL_DISPENSERS];

void buzzer_set_event(uint8_t event_mask);
void buzzer_set_event_from_isr(uint8_t event_mask, BaseType_t* higher_priority_was_woken);
void buzzer_clear_event(uint8_t event_mask);

#endif /* BUZZER_H */
