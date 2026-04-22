file(GLOB U8G2_SOURCES "upstream/csrc/*.c")

idf_component_register(
    SRCS ${U8G2_SOURCES} "u8g2_esp32_hal.c"
    INCLUDE_DIRS "upstream/csrc" "."
    REQUIRES esp_driver_i2c esp_driver_gpio
)

# u8g2 has some sign-compare / const warnings — don't fail on them
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-sign-compare -Wno-unused-but-set-variable)

# Enable u8x8_GetUserPtr / u8x8_SetUserPtr (upstream gates these behind the macro)
target_compile_definitions(${COMPONENT_LIB} PUBLIC U8X8_WITH_USER_PTR)
