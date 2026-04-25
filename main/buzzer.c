#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "driver/ledc.h"
#include "portmacro.h"

#include "defines.h"
#include "buzzer.h"

#define BUZZER_TIMER    LEDC_TIMER_0
#define BUZZER_CHANNEL  LEDC_CHANNEL_0
#define BUZZER_MODE     LEDC_LOW_SPEED_MODE
#define BUZZER_RES      LEDC_TIMER_10_BIT
#define BUZZER_DUTY_ON  (1 << (10 - 1))

const EventBits_t DISPENSER_TO_RINGING_EVENT[NUM_PILL_DISPENSERS] = {
    [PILL_DISPENSER_IDX_A] = BUZZER_EVENT_RINGING_A,
    [PILL_DISPENSER_IDX_B] = BUZZER_EVENT_RINGING_B,
};

static EventGroupHandle_t buzzer_event_group;

static void buzzer_task(void*);
static void buzzer_tone(uint32_t freq_hz);
static void buzzer_off(void);

void buzzer_init() {
    buzzer_event_group = xEventGroupCreate();

    ledc_timer_config_t timer_cfg = {
        .speed_mode      = BUZZER_MODE,
        .timer_num       = BUZZER_TIMER,
        .duty_resolution = BUZZER_RES,
        .freq_hz         = 2000,
        .clk_cfg         = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&timer_cfg);

    ledc_channel_config_t chan_cfg = {
        .gpio_num   = GPIO_PIN_BUZZER,
        .speed_mode = BUZZER_MODE,
        .channel    = BUZZER_CHANNEL,
        .timer_sel  = BUZZER_TIMER,
        .duty       = 0,
        .hpoint     = 0,
    };
    ledc_channel_config(&chan_cfg);

    xTaskCreate(
        buzzer_task,
        "Buzzer Task",
        2048,
        NULL,
        TASK_PRIORITY_HIGH,
        NULL
    );
}

void buzzer_set_event(uint8_t event_mask) {
    xEventGroupSetBits(buzzer_event_group, event_mask);
}

void buzzer_set_event_from_isr(uint8_t event_mask, BaseType_t* higher_priority_was_woken) {
    xEventGroupSetBitsFromISR(buzzer_event_group, event_mask, higher_priority_was_woken);
}

void buzzer_clear_event(uint8_t event_mask) {
    xEventGroupClearBits(buzzer_event_group, event_mask);
}

static void buzzer_task(void*) {
    const EventBits_t all_events =
        BUZZER_EVENT_RINGING_A |
        BUZZER_EVENT_RINGING_B |
        BUZZER_EVENT_DISPENSER_OPEN |
        BUZZER_EVENT_BUTTON_PRESS;

    while (true) {
        EventBits_t bits = xEventGroupWaitBits(
            buzzer_event_group,
            all_events,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

        if (bits & BUZZER_EVENT_BUTTON_PRESS) {
            buzzer_tone(3000);
            vTaskDelay(pdMS_TO_TICKS(20));
            buzzer_off();
            xEventGroupClearBits(buzzer_event_group, BUZZER_EVENT_BUTTON_PRESS);
        } else if (bits & BUZZER_EVENT_DISPENSER_OPEN) {
            buzzer_tone(2000);
            vTaskDelay(pdMS_TO_TICKS(80));
            buzzer_tone(3000);
            vTaskDelay(pdMS_TO_TICKS(80));
            buzzer_off();
            xEventGroupClearBits(buzzer_event_group, BUZZER_EVENT_DISPENSER_OPEN);
        } else if (bits & (BUZZER_EVENT_RINGING_A)) {
            buzzer_tone(2500);
            vTaskDelay(pdMS_TO_TICKS(100));
            buzzer_off();
            vTaskDelay(pdMS_TO_TICKS(20));
            buzzer_tone(2500);
            vTaskDelay(pdMS_TO_TICKS(100));
            buzzer_off();
            vTaskDelay(pdMS_TO_TICKS(20));
            buzzer_tone(3500);
            vTaskDelay(pdMS_TO_TICKS(50));
            buzzer_off();
            vTaskDelay(pdMS_TO_TICKS(20));
            buzzer_tone(3500);
            vTaskDelay(pdMS_TO_TICKS(50));
            buzzer_off();
            vTaskDelay(pdMS_TO_TICKS(20));
            buzzer_tone(3500);
            vTaskDelay(pdMS_TO_TICKS(50));
            buzzer_off();
            vTaskDelay(pdMS_TO_TICKS(20));
        } else if (bits & (BUZZER_EVENT_RINGING_B)) {
            buzzer_tone(2500);
            vTaskDelay(pdMS_TO_TICKS(100));
            buzzer_off();
            vTaskDelay(pdMS_TO_TICKS(20));
            buzzer_tone(3500);
            vTaskDelay(pdMS_TO_TICKS(100));
            buzzer_off();
            vTaskDelay(pdMS_TO_TICKS(20));
            buzzer_tone(2500);
            vTaskDelay(pdMS_TO_TICKS(100));
            buzzer_off();
            vTaskDelay(pdMS_TO_TICKS(20));
            buzzer_tone(1500);
            vTaskDelay(pdMS_TO_TICKS(100));
            buzzer_off();
            vTaskDelay(pdMS_TO_TICKS(20));
        }
    }
}

static void buzzer_tone(uint32_t freq_hz) {
    ledc_set_freq(BUZZER_MODE, BUZZER_TIMER, freq_hz);
    ledc_set_duty(BUZZER_MODE, BUZZER_CHANNEL, BUZZER_DUTY_ON);
    ledc_update_duty(BUZZER_MODE, BUZZER_CHANNEL);
}

static void buzzer_off(void) {
    ledc_set_duty(BUZZER_MODE, BUZZER_CHANNEL, 0);
    ledc_update_duty(BUZZER_MODE, BUZZER_CHANNEL);
}