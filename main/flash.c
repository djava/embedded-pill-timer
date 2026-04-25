#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"

#include "flash.h"
#include "defines.h"

#define NVS_NAMESPACE "pill_timer"
#define NVS_PILL_TIMER_KEY "timers"

static nvs_handle_t storage_handle;

void flash_init() {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }
    ESP_ERROR_CHECK(nvs_open(NVS_NAMESPACE, NVS_READWRITE, &storage_handle));
}

void flash_save_pill_timers(PillTimer_t* pts) {
    nvs_set_blob(storage_handle,
                 NVS_PILL_TIMER_KEY,
                 pts,
                 sizeof(PillTimer_t) * NUM_PILL_TIMERS);
    ESP_ERROR_CHECK(nvs_commit(storage_handle));
    ESP_LOGI("flash", "Saved pill timer state");
}

bool flash_load_pill_timers(PillTimer_t* pts) {
    size_t length = sizeof(PillTimer_t) * NUM_PILL_TIMERS;
    const esp_err_t err = nvs_get_blob(storage_handle,
                                       NVS_PILL_TIMER_KEY,
                                       pts,
                                       &length);
    if (length != sizeof(PillTimer_t) * NUM_PILL_TIMERS) {
        return false;
    }
    ESP_LOGI("flash", "Restored pill timer state: %s", esp_err_to_name(err));

    return err == ESP_OK;
}

void flash_clear_pill_timer() {
    nvs_erase_key(storage_handle, NVS_PILL_TIMER_KEY);
}