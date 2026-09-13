// microSD over 4-bit SDIO.
//
// The socket is on the BOTTOM of the board and its detect switch is wired to
// GPIO42, pulled up, closing to ground when a card is in. Mounting is deferred
// until the switch says there is something to mount, otherwise the SDMMC host
// spends 2 s timing out on every boot with an empty slot.
#include "storage/sdcard.h"
#include "board.h"
#include "driver/sdmmc_host.h"
#include "sdmmc_cmd.h"
#include "esp_vfs_fat.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "sd";
static sdmmc_card_t *s_card;

bool sdcard_present(void) { return gpio_get_level(PIN_SD_CD) == 0; }
bool sdcard_mounted(void) { return s_card != NULL; }

esp_err_t sdcard_mount(void) {
    if (s_card) return ESP_OK;

    gpio_config_t cd = {
        .pin_bit_mask = 1ULL << PIN_SD_CD,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&cd);
    if (!sdcard_present()) {
        ESP_LOGW(TAG, "no card in the slot");
        return ESP_ERR_NOT_FOUND;
    }

    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.max_freq_khz = SDMMC_FREQ_HIGHSPEED;      // 40 MHz
    sdmmc_slot_config_t slot = SDMMC_SLOT_CONFIG_DEFAULT();
    slot.width = 4;
    slot.clk = PIN_SD_CLK; slot.cmd = PIN_SD_CMD;
    slot.d0 = PIN_SD_D0; slot.d1 = PIN_SD_D1;
    slot.d2 = PIN_SD_D2; slot.d3 = PIN_SD_D3;
    slot.flags = SDMMC_SLOT_FLAG_INTERNAL_PULLUP;  // belt and braces; 10k fitted

    esp_vfs_fat_sdmmc_mount_config_t mnt = {
        .format_if_mount_failed = false,           // never reformat a user's card
        .max_files = 8,
        .allocation_unit_size = 32 * 1024,
    };
    esp_err_t e = esp_vfs_fat_sdmmc_mount(SD_MOUNT, &host, &slot, &mnt, &s_card);
    if (e != ESP_OK) {
        ESP_LOGE(TAG, "mount failed: %s", esp_err_to_name(e));
        s_card = NULL;
        return e;
    }
    ESP_LOGI(TAG, "mounted %lluMB %s", ((uint64_t)s_card->csd.capacity * s_card->csd.sector_size) >> 20,
             s_card->cid.name);
    return ESP_OK;
}

void sdcard_unmount(void) {
    if (!s_card) return;
    esp_vfs_fat_sdcard_unmount(SD_MOUNT, s_card);
    s_card = NULL;
}

void sdcard_usage(uint64_t *total, uint64_t *used) {
    *total = *used = 0;
    if (!s_card) return;
    FATFS *fs; DWORD free_clusters;
    if (f_getfree("0:", &free_clusters, &fs) == FR_OK) {
        uint64_t sect = (uint64_t)fs->csize * 512;
        uint64_t tot = (uint64_t)(fs->n_fatent - 2) * sect;
        *total = tot;
        *used = tot - (uint64_t)free_clusters * sect;
    }
}
