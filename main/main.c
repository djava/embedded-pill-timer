#include <stdio.h>
#include "buzzer.h"
#include "esp_intr_alloc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "pcf8563.h"
#include "portmacro.h"
#include "u8g2.h"
#include "u8g2_esp32_hal.h"

#include "defines.h"
#include "display.h"
#include "rtc.h"
#include "pill_timer_mgr.h"
#include "menus.h"
#include "debug_console.h"

// ESP32-C3 default I2C pins: D4=SDA=GPIO6, D5=SCL=GPIO7
#define I2C_SDA_GPIO 6
#define I2C_SCL_GPIO 7

i2c_master_bus_handle_t i2c_bus;
u8g2_t u8g2;

const gpio_num_t DISPENSER_TO_GPIO_PIN[3] = {
    [PILL_DISPENSER_IDX_A] = GPIO_PIN_DISPENSER_A,
    [PILL_DISPENSER_IDX_B] = GPIO_PIN_DISPENSER_B,
};

void app_main(void) {
    i2c_bus = NULL;
    const i2c_master_bus_config_t i2c_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_NUM_0,
        .sda_io_num = I2C_SDA_GPIO,
        .scl_io_num = I2C_SCL_GPIO,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_config, &i2c_bus));

    gpio_install_isr_service(ESP_INTR_FLAG_EDGE);

    rtc_hw_init();
    display_init();
    buzzer_init();
    pill_timer_mgr_init();
    menus_init();
    debug_console_init();

    while (true) {
        vTaskDelay(portMAX_DELAY);
    }
}

