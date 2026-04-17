#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "blink";

#define BLINK_GPIO GPIO_NUM_8

static void blink_task(void *arg)
{
    gpio_reset_pin(BLINK_GPIO);
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);

    bool level = false;
    while (1) {
        gpio_set_level(BLINK_GPIO, level);
        level = !level;
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "starting blink task");
    xTaskCreate(blink_task, "blink", 2048, NULL, 5, NULL);
}
