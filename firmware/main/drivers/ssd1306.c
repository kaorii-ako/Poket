#include "drivers/ssd1306.h"
#include "drivers/i2c_bus.h"
#include "board.h"
#include <string.h>
#include "esp_log.h"

static i2c_master_dev_handle_t s_dev;
// One page-addressed blast of the whole buffer per frame. At 400 kHz a full
// 1024-byte frame is ~21 ms, which caps the UI at ~45 fps; the shell runs at
// 30 so there is headroom.
static uint8_t s_tx[1 + GFX_BUF_LEN];

static esp_err_t cmd(uint8_t c) {
    uint8_t b[2] = { 0x00, c };
    return i2c_master_transmit(s_dev, b, 2, 100);
}

esp_err_t ssd1306_init(void) {
    ESP_ERROR_CHECK(i2c_bus_init());
    ESP_ERROR_CHECK(i2c_dev_add(I2C_ADDR_OLED, 400000, &s_dev));
    static const uint8_t seq[] = {
        0xAE,              // display off
        0x20, 0x00,        // horizontal addressing
        0xB0, 0xC8,        // page start, COM scan descending
        0x00, 0x10,        // column low/high
        0x40,              // start line 0
        0x81, 0x7F,        // contrast
        0xA1,              // segment remap
        0xA6,              // normal (not inverted)
        0xA8, 0x3F,        // multiplex 64
        0xA4,              // resume from RAM
        0xD3, 0x00,        // display offset
        0xD5, 0x80,        // clock divide
        0xD9, 0xF1,        // pre-charge
        0xDA, 0x12,        // COM pins, alternate
        0xDB, 0x40,        // vcom detect
        0x8D, 0x14,        // charge pump on
        0xAF,              // display on
    };
    for (size_t i = 0; i < sizeof seq; i++)
        ESP_ERROR_CHECK(cmd(seq[i]));
    s_tx[0] = 0x40;        // Co=0, D/C=1 -> data stream
    return ESP_OK;
}

esp_err_t ssd1306_flush(const gfx_t *g) {
    ESP_ERROR_CHECK(cmd(0x21)); ESP_ERROR_CHECK(cmd(0)); ESP_ERROR_CHECK(cmd(GFX_W - 1));
    ESP_ERROR_CHECK(cmd(0x22)); ESP_ERROR_CHECK(cmd(0)); ESP_ERROR_CHECK(cmd(GFX_PAGES - 1));
    memcpy(&s_tx[1], g->buf, GFX_BUF_LEN);
    return i2c_master_transmit(s_dev, s_tx, sizeof s_tx, 200);
}

void ssd1306_contrast(uint8_t c) { cmd(0x81); cmd(c); }
void ssd1306_invert(bool on)     { cmd(on ? 0xA7 : 0xA6); }
void ssd1306_power(bool on)      { cmd(on ? 0xAF : 0xAE); }
