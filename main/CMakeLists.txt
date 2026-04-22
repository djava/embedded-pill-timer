idf_component_register(
    SRCS "main.c" "pcf8563.c"
    INCLUDE_DIRS "."
    REQUIRES esp_driver_i2c u8g2
)
