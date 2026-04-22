#include "pcf8563.h"
#include "esp_check.h"

#define PCF8563_ADDR 0x51

// Registers
#define REG_CTRL1   0x00
#define REG_CTRL2   0x01
#define REG_VL_SEC  0x02  // bit7 = VL (voltage low = clock integrity lost)
#define REG_MIN     0x03
#define REG_HOUR    0x04
#define REG_DAY     0x05
#define REG_WEEKDAY 0x06
#define REG_C_MONTH 0x07  // bit7 = century (1 = 1900s, 0 = 2000s in PCF8563)
#define REG_YEAR    0x08

static inline uint8_t bin2bcd(uint8_t b) { return ((b / 10) << 4) | (b % 10); }
static inline uint8_t bcd2bin(uint8_t b) { return (b >> 4) * 10 + (b & 0x0F); }

void pcf8563_init(pcf8563_t *rtc, i2c_master_bus_handle_t bus)
{
    const i2c_device_config_t devcfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = PCF8563_ADDR,
        .scl_speed_hz    = 400 * 1000,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus, &devcfg, &rtc->dev));

    // Clear CTRL1 and CTRL2 to a known state (normal running, no alarms/timers).
    uint8_t ctrl[] = { REG_CTRL1, 0x00, 0x00 };
    ESP_ERROR_CHECK(i2c_master_transmit(rtc->dev, ctrl, sizeof(ctrl), 100));
}

esp_err_t pcf8563_set_time(pcf8563_t *rtc, const pcf8563_time_t *t)
{
    uint8_t buf[8];
    buf[0] = REG_VL_SEC;
    buf[1] = bin2bcd(t->sec) & 0x7F;      // clear VL
    buf[2] = bin2bcd(t->min) & 0x7F;
    buf[3] = bin2bcd(t->hour) & 0x3F;
    buf[4] = bin2bcd(t->day)  & 0x3F;
    buf[5] = t->weekday & 0x07;
    uint8_t century = (t->year < 2000) ? 0x80 : 0x00;
    buf[6] = (bin2bcd(t->month) & 0x1F) | century;
    buf[7] = bin2bcd((uint8_t)(t->year % 100));
    return i2c_master_transmit(rtc->dev, buf, sizeof(buf), 100);
}

esp_err_t pcf8563_get_time(pcf8563_t *rtc, pcf8563_time_t *t, bool *clock_invalid)
{
    uint8_t reg = REG_VL_SEC;
    uint8_t r[7];
    esp_err_t err = i2c_master_transmit_receive(rtc->dev, &reg, 1, r, sizeof(r), 100);
    if (err != ESP_OK) return err;

    if (clock_invalid) *clock_invalid = (r[0] & 0x80) != 0;
    t->sec     = bcd2bin(r[0] & 0x7F);
    t->min     = bcd2bin(r[1] & 0x7F);
    t->hour    = bcd2bin(r[2] & 0x3F);
    t->day     = bcd2bin(r[3] & 0x3F);
    t->weekday = r[4] & 0x07;
    t->month   = bcd2bin(r[5] & 0x1F);
    uint8_t year_bcd = r[6];
    uint16_t base = (r[5] & 0x80) ? 1900 : 2000;
    t->year = base + bcd2bin(year_bcd);
    return ESP_OK;
}
