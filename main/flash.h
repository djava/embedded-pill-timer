#ifndef FLASH_H
#define FLASH_H

#include "pill_timer_mgr.h"

void flash_init(void);

void flash_save_pill_timers(PillTimer_t* pts);
bool flash_load_pill_timers(PillTimer_t* pts);

void flash_clear_pill_timer(void);

#endif /* FLASH_H */
