// Poket - pocket Bluetooth MP3 player.
//
// Bring-up order matters: NVS first (the theme and the AP password live
// there), then the panel so a failure later has somewhere to be reported,
// then the card, then audio. The UI task starts before the library scan so
// the splash is up while a big card is being walked.
#include "app/app_state.h"
#include "app/ui_task.h"
#include "audio/player.h"
#include "board.h"
#include "drivers/battery.h"
#include "drivers/i2c_bus.h"
#include "drivers/input.h"
#include "drivers/ssd1306.h"
#include "storage/library.h"
#include "storage/sdcard.h"
#include "ui/theme.h"
#include "audio/bt_link.h"

#include "esp_log.h"
#include "esp_err.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "poket";

static void scan_task(void *arg) {
    (void)arg;
    if (library_scan() == ESP_OK)
        ESP_LOGI(TAG, "library: %u tracks", library_count());
    else
        ESP_LOGW(TAG, "library scan failed");
    vTaskDelete(NULL);
}

void app_main(void) {
    ESP_LOGI(TAG, "Poket %s", POKET_FW_VERSION);

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    app_state_init();
    ESP_ERROR_CHECK(i2c_bus_init());
    ESP_ERROR_CHECK(ssd1306_init());
    theme_init();                    // restores the saved choice, or minimal
    input_init();
    battery_init();
    ui_task_start();                 // splash is now on screen

    if (sdcard_mount() == ESP_OK) {
        // Walking a full card takes seconds; do it off the main task so the
        // splash keeps animating and the buttons stay live.
        xTaskCreate(scan_task, "scan", 4096, NULL, 3, NULL);
    } else {
        ESP_LOGE(TAG, "no card");
    }

    ESP_ERROR_CHECK(player_init());

    // If a headset was linked before, go straight back to it rather than
    // making the user pick from a scan on every boot.
    if (bt_link_has_saved()) {
        ESP_LOGI(TAG, "reconnecting to %s", bt_link_saved_name());
        bt_link_connect_saved();
    }
    ESP_LOGI(TAG, "ready");
}
