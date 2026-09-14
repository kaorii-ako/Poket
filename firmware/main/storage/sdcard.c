// microSD over SPI.
//
// Not SDMMC: on the classic ESP32 the 4-bit slot puts DAT2 on GPIO12, the
// flash-voltage strap, and a card's internal pull-up there tells the
// bootloader the flash runs at 1.8 V. See the note in board.h.
//
// The socket is on the BOTTOM of the board and its detect switch is pulled up,
// closing to ground when a card is in. Mounting is deferred until the switch
// says there is something to mount, otherwise the host spends 2 s timing out
// on every boot with an empty slot.
#include "storage/sdcard.h"
#include "board.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
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

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = SD_SPI_HOST;
    host.max_freq_khz = SD_SPI_HZ / 1000;

    spi_bus_config_t bus = {
        .mosi_io_num = PIN_SD_MOSI,
        .miso_io_num = PIN_SD_MISO,
        .sclk_io_num = PIN_SD_SCK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        // One FAT sector per transfer; bigger buffers here buy nothing when
        // the consumer is a 40 kB/s MP3 stream.
        .max_transfer_sz = 4096,
    };
    esp_err_t be = spi_bus_initialize(SD_SPI_HOST, &bus, SPI_DMA_CH_AUTO);
    if (be != ESP_OK && be != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "spi bus: %s", esp_err_to_name(be));
        return be;
    }

    sdspi_device_config_t slot = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot.gpio_cs = PIN_SD_CS;
    slot.host_id = SD_SPI_HOST;

    esp_vfs_fat_sdmmc_mount_config_t mnt = {
        .format_if_mount_failed = false,           // never reformat a user's card
        .max_files = 8,
        .allocation_unit_size = 32 * 1024,
    };
    esp_err_t e = esp_vfs_fat_sdspi_mount(SD_MOUNT, &host, &slot, &mnt, &s_card);
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
