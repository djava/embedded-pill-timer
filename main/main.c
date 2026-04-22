#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "i2c_bus.h"
#include "pcf8563.h"
#include "u8g2.h"
#include "u8g2_esp32_hal.h"

i2c_master_bus_handle_t i2c_bus;
pcf8563_t rtc_peripheral;
static u8g2_t u8g2;

void app_main(void)
{
    i2c_bus = NULL;
    const i2c_master_bus_config_t cfg = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_NUM_0,
        .sda_io_num = I2C_SDA_GPIO,
        .scl_io_num = I2C_SCL_GPIO,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&cfg, &i2c_bus));

    pcf8563_init(&rtc_peripheral, i2c_bus);
    u8g2_esp32_init_ssd1306_i2c(&u8g2, i2c_bus);
}
