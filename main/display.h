#ifndef DISPLAY_H
#define DISPLAY_H
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

typedef enum {
    DISPLAY_MODE_CLOCK,
    DISPLAY_MODE_MENU,
    DISPLAY_MODE_RINGING
} DisplayMode_t;

extern DisplayMode_t display_mode;
extern SemaphoreHandle_t display_mutex;

void display_init(void);

#endif