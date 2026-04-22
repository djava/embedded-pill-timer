#pragma once

#include "u8g2.h"
#include "driver/i2c_master.h"

// Set up u8g2 for a 128x64 SSD1306 on the given I2C bus at addr 0x3C.
// Allocates a device handle on the bus. Caller owns `u8g2` storage.
void u8g2_esp32_init_ssd1306_i2c(u8g2_t *u8g2, i2c_master_bus_handle_t bus);
