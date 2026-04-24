#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/usb_serial_jtag.h"

#include "defines.h"
#include "debug_console.h"
#include "menus.h"
#include "pill_timer_mgr.h"

static const char *TAG = "debug_console";

static void debug_console_task(void*);
static void print_help(void);

void debug_console_init(void) {
    usb_serial_jtag_driver_config_t cfg = USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(usb_serial_jtag_driver_install(&cfg));

    xTaskCreate(
        debug_console_task,
        "Debug Console Task",
        3072,
        NULL,
        TASK_PRIORITY_LOW,
        NULL
    );
}

static void print_help(void) {
    printf(
        "\n--- Debug console ---\n"
        "  u = button UP\n"
        "  d = button DOWN\n"
        "  o = button OK\n"
        "  a = dispenser A open\n"
        "  b = dispenser B open\n"
        "  ? = show this help\n"
        "---------------------\n"
    );
}

static void debug_console_task(void*) {
    print_help();

    uint8_t c;
    while (true) {
        const int n = usb_serial_jtag_read_bytes(&c, 1, portMAX_DELAY);
        if (n <= 0) { continue; }

        switch (c) {
            case 'u': case 'U':
                menus_inject_button(BUTTON_UP);
                ESP_LOGI(TAG, "-> BUTTON_UP");
                break;
            case 'd': case 'D':
                menus_inject_button(BUTTON_DOWN);
                ESP_LOGI(TAG, "-> BUTTON_DOWN");
                break;
            case 'o': case 'O':
                menus_inject_button(BUTTON_OK);
                ESP_LOGI(TAG, "-> BUTTON_OK");
                break;
            case 'a': case 'A':
                pill_timer_mgr_inject_dispenser_open(PILL_DISPENSER_IDX_A);
                ESP_LOGI(TAG, "-> DISPENSER A OPEN");
                break;
            case 'b': case 'B':
                pill_timer_mgr_inject_dispenser_open(PILL_DISPENSER_IDX_B);
                ESP_LOGI(TAG, "-> DISPENSER B OPEN");
                break;
            case '?': case 'h': case 'H':
                print_help();
                break;
            case '\n': case '\r': case ' ': case '\t':
                break;
            default:
                ESP_LOGW(TAG, "Unknown key 0x%02x", c);
                break;
        }
    }
}
