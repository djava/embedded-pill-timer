#include "u8g2_esp32_hal.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_rom_sys.h"
#include "esp_log.h"

#define SSD1306_I2C_ADDR 0x3C
#define I2C_BUF_CAP      64

static const char *TAG = "u8g2_hal";

typedef struct {
    i2c_master_dev_handle_t dev;
    uint8_t  buf[I2C_BUF_CAP];
    size_t   len;
} hal_ctx_t;

static uint8_t byte_i2c_cb(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
    hal_ctx_t *ctx = (hal_ctx_t *)u8x8_GetUserPtr(u8x8);
    switch (msg) {
    case U8X8_MSG_BYTE_INIT:
        return 1;
    case U8X8_MSG_BYTE_START_TRANSFER:
        ctx->len = 0;
        return 1;
    case U8X8_MSG_BYTE_SEND: {
        const uint8_t *src = (const uint8_t *)arg_ptr;
        if (ctx->len + arg_int > I2C_BUF_CAP) {
            ESP_LOGE(TAG, "i2c buf overflow");
            return 0;
        }
        for (int i = 0; i < arg_int; i++) ctx->buf[ctx->len++] = src[i];
        return 1;
    }
    case U8X8_MSG_BYTE_END_TRANSFER: {
        esp_err_t err = i2c_master_transmit(ctx->dev, ctx->buf, ctx->len, 100);
        if (err != ESP_OK) ESP_LOGE(TAG, "i2c tx: %s", esp_err_to_name(err));
        return err == ESP_OK;
    }
    case U8X8_MSG_BYTE_SET_DC:
        return 1;
    }
    return 0;
}

static uint8_t gpio_delay_cb(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
    (void)u8x8; (void)arg_ptr;
    switch (msg) {
    case U8X8_MSG_GPIO_AND_DELAY_INIT:
        return 1;
    case U8X8_MSG_DELAY_MILLI:
        vTaskDelay(pdMS_TO_TICKS(arg_int ? arg_int : 1));
        return 1;
    case U8X8_MSG_DELAY_10MICRO:
        esp_rom_delay_us(10 * arg_int);
        return 1;
    case U8X8_MSG_DELAY_100NANO:
        return 1;
    }
    return 1;
}

void u8g2_esp32_init_ssd1306_i2c(u8g2_t *u8g2, i2c_master_bus_handle_t bus)
{
    static hal_ctx_t ctx;

    const i2c_device_config_t devcfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = SSD1306_I2C_ADDR,
        .scl_speed_hz    = 400 * 1000,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus, &devcfg, &ctx.dev));

    u8g2_Setup_ssd1306_i2c_128x64_noname_f(u8g2, U8G2_R0, byte_i2c_cb, gpio_delay_cb);
    u8x8_SetUserPtr(u8g2_GetU8x8(u8g2), &ctx);
    u8x8_SetI2CAddress(u8g2_GetU8x8(u8g2), SSD1306_I2C_ADDR << 1);

    u8g2_InitDisplay(u8g2);
    u8g2_SetPowerSave(u8g2, 0);
    u8g2_ClearBuffer(u8g2);
    u8g2_SendBuffer(u8g2);
}
