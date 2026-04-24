idf_component_register(
    SRCS "main.c" "pcf8563.c" "display.c" "rtc.c" "pill_timer_mgr.c" "menus.c" "debug_console.c"
    INCLUDE_DIRS "."
    REQUIRES esp_driver_i2c esp_driver_usb_serial_jtag u8g2
)

target_compile_options(${COMPONENT_LIB} PRIVATE -std=gnu2x)
