#pragma once
#include "ui/gfx.h"
#include "esp_err.h"
esp_err_t ssd1306_init(void);
esp_err_t ssd1306_flush(const gfx_t *g);
void      ssd1306_contrast(uint8_t c);
void      ssd1306_invert(bool on);
void      ssd1306_power(bool on);
